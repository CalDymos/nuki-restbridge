#define IS_VALID_DETECT 0xa00ab00bc00bd00d;

#include <Arduino.h>
#include "esp_http_client.h"
#include "esp_task_wdt.h"
#include "Config.h"
#include "esp32-hal-log.h"
#include "hal/wdt_hal.h"
#include "esp_chip_info.h"
#include "time.h"
#include "esp_core_dump.h"
#include <FS.h>
#include <LittleFS.h>
#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
#include "esp_psram.h"
#endif
#include "NukiWrapper.h"
#include "NukiNetwork.h"
#include "CharBuffer.hpp"
#include "NukiDeviceId.hpp"
#include "WebCfgServer.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "EspMillis.h"
#include "NimBLEDevice.h"
#include "ImportExport.h"

Preferences *preferences = nullptr;        // Pointer to non-volatile key-value storage (nvs).
ImportExport* importExport = nullptr;      // Import/Export handler for settings backup.
NukiNetwork *network = nullptr;            // Main network interface (WiFi/Ethernet, REST API).
BleScanner::Scanner *bleScanner = nullptr; // BLE scanner to discover/connect Nuki devices.
NukiWrapper *nuki = nullptr;               // Core smart lock wrapper.
NukiDeviceId *deviceIdLock = nullptr;      // Unique device ID handler.
WebCfgServer *webCfgServer = nullptr;      // Web-based configuration interface.
Logger *Log = nullptr;                     // Global logger instance.

bool bleDone = false;                           // Whether BLE initialization is complete.
bool lockEnabled = false;                       // Whether lock operations are currently permitted.
bool netwConnected = false;                      // Whether network is connected / ready
bool netwGateOpen = false;                      // Whether network gate is open (i.e., network operations are permitted).
bool rebootLock = false;                        // Whether to reboot lock after failure.
uint8_t lockBleRestartAttemptCount = 0;         // Counter for lock restart attempts.
bool coredumpPrinted = true;                    // Prevent repeated printing of core dump on each boot.
bool timeSynced = false;                        // Whether NTP time sync was successful.
bool restartReason_isValid;                     // True if restart reason could be determined.
bool fsReady = false;                           // true, if LittleFS was successfully mounted
bool webCfgStarted = false;                     // true, if web configuration server was started
bool lockStarted = false;                       // true, if Nuki lock handling was started
bool bleScannerStarted = false;                 // true, if BLE scanner was started
char16_t buffer_size = CHAR_BUFFER_SIZE;        // Size of the shared character buffer.

int64_t restartTs = (pow(2, 63) - (5 * 1000 * 60000)) / 1000; // Time stamp for restarting the ESP to prevent the ESPtimer from overflowing

RTC_NOINIT_ATTR int espRunning;                    // Persisted runtime state across deep sleep/restart.
RTC_NOINIT_ATTR int restartReason;                 // Last restart reason code.
RTC_NOINIT_ATTR uint64_t restartReasonValidDetect; // Timestamp used for validating restart cause.
RTC_NOINIT_ATTR bool forceEnableWebCfgServer;      // Flag to force-enable web config mode.
RTC_NOINIT_ATTR bool disableNetwork;               // Flag to disable all network activity.
RTC_NOINIT_ATTR bool wifiFallback;                 // Whether WiFi fallback was triggered (e.g. AP mode).
RTC_NOINIT_ATTR bool ethCriticalFailure;           // Flag for Ethernet hardware failure (e.g. PHY).
RTC_NOINIT_ATTR uint64_t bootloopValidDetect;      // Validation value for bootloop detection.
RTC_NOINIT_ATTR int8_t bootloopCounter;            // Counter for bootloop detection.

int lastHTTPeventId = -1;                                          // ID of last received HTTP event.
RestartReason currentRestartReason = RestartReason::NotApplicable; // Tracks current restart state (e.g. Watchdog, Manual, Error etc.).

// Taskhandles
TaskHandle_t nukiTaskHandle = nullptr;    // Handle for BLE/Nuki lock task.
TaskHandle_t networkTaskHandle = nullptr; // Handle for network-related task.
TaskHandle_t webCfgTaskHandle = nullptr;  // Handle for web config server task.

#ifdef DEBUG_NUKIBRIDGE
/**
 * @brief Recursively lists files in a directory and optionally deletes large files.
 * @param fs Reference to filesystem instance (e.g. LittleFS).
 * @param dirname Directory to list.
 * @param levels Recursion depth for subdirectories.
 */
void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Log->printf(F("[DEBUG] Listing directory: %s\r\n"), dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Log->println("[ERROR] - failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Log->println("[ERROR] - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Log->print("[DEBUG]  DIR : ");
      Log->println(file.name());
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
      Log->printf(F("[DEBUG]  FILE: %s\tSIZE: %lu\n"), file.name(), file.size());
    }

    if (file.size() > (int)(LittleFS.totalBytes() * 0.4))
    {
      LittleFS.remove((String) "/" + file.name());
    }

    file = root.openNextFile();
  }
}
#endif

/**
 * @brief Initialize the file system once, if not already mounted.
 *
 * @return true if LittleFS was successfully mounted, false otherwise.
 */
bool initializeFileSystem()
{
  if (fsReady)
    return true;

  if (LittleFS.begin(true))
  {
    fsReady = true;
    return true;
  }

  Serial.println(F("[ERROR] LittleFS init failed."));
  return false;
}
/**
 * @brief Starts the web configuration server if not already started.
 */
void startWebCfgServer()
{
  if (webCfgStarted)
  {
    Log->println("[INFO] Restarting web server");
  }
  else
  {
    const bool enableWebCfg =
        forceEnableWebCfgServer ||
        preferences->getBool(preference_webcfgserver_enabled, true);

    if (!enableWebCfg)
    {
      return;
    }
    Log->println("[INFO] Starting web server");
  }

  if (webCfgServer == nullptr)
  {
    webCfgServer = new WebCfgServer(nuki, network, preferences, importExport);
    Log->println(F("[INFO] Initializing WebCfgServer"));
    webCfgServer->initialize();
    webCfgStarted = true;
    Log->println("[INFO] (Re-)Starting web server done");
  }
  else
  {
    Log->println("[WARNING] (Re-)starting not possible, WebCfgServer is still running");
  }
}
/**
 * @brief Stops the web configuration server if running.
 */
static void stopWebCfgServer()
{
  
  if (webCfgServer != nullptr)
  {
    Log->println("[DEBUG] Deleting webCfgServer");
    delete webCfgServer;
    webCfgServer = nullptr;
    Log->println("[DEBUG] Deleting webCfgServer done");
  }
}

/**
 * @brief Detects bootloops based on reset reasons and increments a counter.
 *        If a bootloop is detected, resets certain preferences to default values.
 */
void bootloopDetection()
{
  uint64_t cmp = IS_VALID_DETECT;
  bool bootloopIsValid = (bootloopValidDetect == cmp);
  Log->printf(F("[DEBUG] %d\n"), bootloopIsValid);

  if (!bootloopIsValid)
  {
    bootloopCounter = (int8_t)0;
    bootloopValidDetect = IS_VALID_DETECT;
    return;
  }

  if (esp_reset_reason() == esp_reset_reason_t::ESP_RST_PANIC ||
      esp_reset_reason() == esp_reset_reason_t::ESP_RST_INT_WDT ||
      esp_reset_reason() == esp_reset_reason_t::ESP_RST_TASK_WDT ||
      esp_reset_reason() == esp_reset_reason_t::ESP_RST_WDT)
  {
    bootloopCounter++;
    Log->printf(F("[DEBUG] Bootloop counter incremented: %d\n"), bootloopCounter);

    if (bootloopCounter == 10)
    {
      Log->print("Bootloop detected.");

      preferences->putInt(preference_buffer_size, CHAR_BUFFER_SIZE);
      preferences->putInt(preference_task_size_network, NETWORK_TASK_SIZE);
      preferences->putInt(preference_task_size_nuki, NUKI_TASK_SIZE);
      preferences->putInt(preference_authlog_max_entries, MAX_AUTHLOG);
      preferences->putInt(preference_keypad_max_entries, MAX_KEYPAD);
      preferences->putInt(preference_timecontrol_max_entries, MAX_TIMECONTROL);
      preferences->putInt(preference_auth_max_entries, MAX_AUTH);
      bootloopCounter = 0;
    }
  }
}

void restartServices(bool reconnect)
{
    bleDone = false;
    lockEnabled = preferences->getBool(preference_lock_enabled);
    importExport->readSettings();
    network->readSettings();

    if (reconnect)
    {
        network->restartNetworkServices(NetworkServiceState::UNKNOWN);
    }

    if(webCfgStarted)
    {
        stopWebCfgServer();
        
    }

    if(lockStarted)
    {
        Log->println("[DEBUG] Deleting nuki");
        delete nuki;
        nuki = nullptr;
        if (reconnect)
        {
            lockStarted = false;
        }
        Log->println("[DEBUG] Deleting nuki done");
    }

    if (bleScannerStarted)
    {
        bleScannerStarted = false;
        Log->println("[DEBUG] Destroying scanner from main");
        delete bleScanner;
        Log->println("[DEBUG] Scanner deleted");
        bleScanner = nullptr;
        Log->println("[DEBUG] Scanner nulled from main");
    }

    if (BLEDevice::isInitialized())
    {
        Log->println("[DEBUG] Deinit BLE device");
        BLEDevice::deinit(false);
        Log->println("[DEBUG] Deinit BLE device done");
    }

    TaskWdtResetAndDelay(2000);

    if(lockEnabled)
    {
		    Log->println("[INFO] Restarting BLE Scanner");
        bleScanner = new BleScanner::Scanner();
        bleScanner->initialize("NukiHub", true, 40, 40);
        bleScanner->setScanDuration(0);
        bleScannerStarted = true;
        Log->println("[DEBUG] Restarting BLE Scanner done");
    }

    if(lockEnabled)
    {
        Log->println("[INFO] Restarting Nuki lock");

        if (reconnect)
        {
            lockStarted = true;
        }

        new NukiWrapper(DEVICE_NAME, deviceIdLock, bleScanner, network, preferences, CharBuffer::get(), buffer_size);
        nuki->initialize();
        bleScanner->whitelist(nuki->getBleAddress());
        Log->println("[DEBUG] Restarting Nuki lock done");
    }

    TaskWdtResetAndDelay(2000);

    bleDone = true;

    startWebCfgServer();
}

/**
 * @brief Nuki-Task: handle Nuki events()
 */
void nukiTask(void *parameter)
{
  TaskWdtReset();

  int64_t nukiLoopTs = 0;
  bool whiteListed = false;

  if (!nukiLoopTs)
    Log->println(F("[DEBUG] run nukiTask()"));

  while (true)
  {

    if ((disableNetwork || netwGateOpen) && bleDone)
    {
      if(bleScannerStarted)
      {
      bleScanner->update();
      TaskWdtResetAndDelay(20);
      }

      bool needsPairing = (lockStarted && !nuki->isPaired());

      if (needsPairing)
      {
        TaskWdtResetAndDelay(2500);
      }
      else if (!whiteListed)
      {
        whiteListed = true;
        if (lockEnabled)
        {
          bleScanner->whitelist(nuki->getBleAddress());
        }
      }

      if (lockStarted)
      {
        NukiWrapper::BleControllerRestartReason restartReason = nuki->getBleControllerRestartReason();
        if (restartReason != NukiWrapper::BleControllerRestartReason::None)
        {
          if (lockBleRestartAttemptCount > 3)
          {
            if (restartReason == NukiWrapper::BleControllerRestartReason::DisconnectError)
            {
              restartEsp(RestartReason::BLEError);
            }
            else if (restartReason == NukiWrapper::BleControllerRestartReason::BeaconWatchdog)
            {
              restartEsp(RestartReason::BLEBeaconWatchdog);
            }
          }
          else
          {
            lockBleRestartAttemptCount += 1;
            restartServices(false);
            continue;
          }
        }
        else
        {
          if (lockBleRestartAttemptCount > 0 && nuki->hasConnected())
          {
            lockBleRestartAttemptCount = 0;
          }

          nuki->update(rebootLock);
          rebootLock = false;
        }
      }
    }
    if (espMillis() - nukiLoopTs > 120000)
    {
      Log->println(F("[DEBUG] nukiTask is running"));
      nukiLoopTs = espMillis();
    }

    TaskWdtResetAndDelay(50);
  }
}

/**
 * @brief Netzwerk-Task: processes HTTP REST API requests and updates to HA
 */
void networkTask(void *parameter)
{
  int64_t networkLoopTs = 0;
  bool updateTime = true;

  if (!networkLoopTs)
    Log->println(F("[DEBUG] run networkTask()"));

  if (preferences->getBool(preference_show_secrets, false))
  {
    preferences->putBool(preference_show_secrets, false);
  }

  while (true)
  {

    int64_t ts = espMillis();
    if (ts > 120000 && ts < 125000)
    {
      if (bootloopCounter > 0)
      {
        bootloopCounter = (int8_t)0;
        Log->println(F("[DEBUG] Bootloop counter reset"));
      }
    }

    network->update();
    TaskWdtResetAndDelay(50);
    netwConnected = network->isConnected();

    if (netwConnected && updateTime)
    {
      if (preferences->getBool(preference_update_time, false))
      {
        String timeserver = preferences->getString(preference_time_server, "pool.ntp.org");
        String timezone = preferences->getString(preference_timezone, "");
        Log->println(F("[INFO] Start NTP Time sync"));
        if (timezone.length() >= 3)
        {
          configTzTime(timezone.c_str(), timeserver.c_str());
        }
        else
        {
          configTime(0, 0, timeserver.c_str());
        }

        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
          Log->println(F("[INFO] NTP time synced"));
          timeSynced = true;
        }
        else
        {
          Log->println(F("[INFO] NTP sync failed"));
          timeSynced = false;
        }
      }
      updateTime = false;
    }

    netwConnected = network->networkGateOpen();
    NukiNetwork::ServiceRestartRequest req = network->consumeServiceRestartRequest();

    if (req == NukiNetwork::ServiceRestartRequest::Restart)
    {
      restartServices(false);
    }
    else if (req == NukiNetwork::ServiceRestartRequest::RestartWithReconnect)
    {
      restartServices(true);
    }
    else
    {
      // No restart requested
      if (netwConnected && lockStarted)
      {
        // rebootLock = method for setting rebootLock
      }
    }

    if (espMillis() - networkLoopTs > 120000)
    {
      Log->println(F("[DEBUG] networkTask is running"));
      networkLoopTs = espMillis();
    }

    if (espMillis() > restartTs)
    {
      Log->disableFileLog();
      TaskWdtResetAndDelay(10);
      restartEsp(RestartReason::RestartTimer);
    }

    TaskWdtResetAndDelay(50);
  }
}

/**
 * @brief WebConfig task: processes HTTP requests to Web Configurator
 */
void webCfgTask(void *parameter)
{
  int64_t webCfgLoopTs = 0;
  if (!webCfgLoopTs)
    Log->println(F("[DEBUG] run webCfgTask()"));
  while (true)
  {

    if (webCfgServer != nullptr)
    {
      webCfgServer->handleClient();
    }
    if (espMillis() - webCfgLoopTs > 120000)
    {
      Log->println(F("[DEBUG] webCfgTask is running"));
      webCfgLoopTs = espMillis();
    }
    TaskWdtResetAndDelay(2);
  }
}

/**
 * @brief Prints current FreeRTOS task state information to the log.
 *
 * Useful for debugging watchdog issues or lockups by showing task runtime and stack usage.
 */
void printTaskInfo()
{
  const UBaseType_t numTasks = uxTaskGetNumberOfTasks();
  TaskStatus_t *taskArray = (TaskStatus_t *)malloc(numTasks * sizeof(TaskStatus_t));

  if (taskArray)
  {
    uint32_t totalRunTime;
    UBaseType_t numTasksRecorded = uxTaskGetSystemState(taskArray, numTasks, &totalRunTime);

    Log->println(F("\n=== Task List ==="));
    Log->println(F("Name\t\tState\tPrio\tStack\tTask Number"));
    for (UBaseType_t i = 0; i < numTasksRecorded; i++)
    {
      Log->printf(F("%s\t\t%d\t%d\t%d\t%d\r\n"),
                  taskArray[i].pcTaskName,
                  taskArray[i].eCurrentState,
                  taskArray[i].uxCurrentPriority,
                  taskArray[i].usStackHighWaterMark,
                  (int)taskArray[i].xTaskNumber);
    }
    Log->println(F("=================\n"));
    free(taskArray);
  }
  else
  {
    Log->println(F("[ERROR] malloc failed for task list!"));
  }
}

/**
 * @brief Creates and starts all FreeRTOS tasks used in the application.
 *
 * This includes tasks for BLE lock control, network handling, and web configuration.
 * Task handles are stored globally for status tracking.
 */
void setupTasks()
{
  Log->println(F("[DEBUG] setup Tasks"));

  esp_chip_info_t info;
  esp_chip_info(&info);
  uint8_t espCores = info.cores;
  Log->print("[DEBUG] Cores: ");
  Log->println(espCores);

  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = 300000,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_err_t err = esp_task_wdt_reconfigure(&twdt_config);

  if (err != ESP_OK)
  {
    Log->printf(F("[ERROR] esp_task_wdt_reconfigure failed: %d\r\n"), err);
  }
  else
  {
    Log->println(F("[DEBUG] Watchdog successfully reconfigured"));
  }

  Log->println(F("[DEBUG] Create webCfgTask"));
  if (xTaskCreatePinnedToCore(webCfgTask, "WebCfg", WEBCFGSERVER_TASK_SIZE, NULL, 2, &webCfgTaskHandle, (espCores > 1) ? 1 : 0) != pdPASS)
  {
    Log->println(F("[ERROR] webCfgTask could not be started!"));
  }
  Log->printf(F("[DEBUG] Created webCfgTaskHandle: %p\r\n"), webCfgTaskHandle);

  Log->println(F("[DEBUG] Adding webCfgTask to Watchdog..."));
  if (webCfgTaskHandle != NULL)
  {
    esp_err_t err = esp_task_wdt_add(webCfgTaskHandle);
    if (err != ESP_OK)
    {
      Log->printf(F("[ERROR] esp_task_wdt_add failed for webCfgTask: %d\r\n"), err);
    }
    else
    {
      Log->println(F("[DEBUG] webCfgTask successfully added to Watchdog"));
    }
  }
  else
  {
    Log->println(F("[ERROR] Failed to create webCfgTask"));
  }

  if (!disableNetwork)
  {
    Log->println(F("[DEBUG] Create networkTask"));
    if (xTaskCreatePinnedToCore(networkTask, "ntw", NETWORK_TASK_SIZE, NULL, 3, &networkTaskHandle, (espCores > 1) ? 1 : 0) != pdPASS)
    {
      Log->println(F("[ERROR] networkTask could not be started!"));
    }
    Log->printf(F("[DEBUG] Created networkTaskHandle: %p\n"), networkTaskHandle);
    Log->println(F("[DEBUG] Adding networkTask to Watchdog..."));
    if (networkTaskHandle != NULL)
    {
      esp_err_t err = esp_task_wdt_add(networkTaskHandle);
      if (err != ESP_OK)
      {
        Log->printf(F("[ERROR] esp_task_wdt_add failed for networkTask: %d\n"), err);
      }
      else
      {
        Log->println(F("[DEBUG] networkTask successfully added to Watchdog"));
      }
    }
    else
    {
      Log->println(F("[ERROR] Failed to create networkTas"));
    }
  }

  if (!network->isApOpen() && lockEnabled)
  {
    Log->println(F("[DEBUG] Create nukiTask"));
    if (xTaskCreatePinnedToCore(nukiTask, "nuki", NUKI_TASK_SIZE, NULL, 2, &nukiTaskHandle, 0) != pdPASS)
    {
      Log->println(F("[ERROR] nukiTask could not be started!"));
    }

    if (nukiTaskHandle != NULL)
    {
      esp_err_t err = esp_task_wdt_add(nukiTaskHandle);
      if (err != ESP_OK)
      {
        Log->printf(F("[ERROR] esp_task_wdt_add failed for nukiTask: %d\n"), err);
      }
      else
      {
        Log->println(F("[DEBUG] nukiTask successfully added to Watchdog"));
      }
    }
    else
    {
      Log->println(F("[ERROR] Failed to create nukiTask"));
    }
  }
}

/**
 * @brief Logs the last ESP32 core dump information, if available.
 *
 * Reads crash details such as reason, PC address, and cause from persistent memory (e.g. RTC/exception frame).
 * Intended to be called once after boot to provide post-mortem debug info.
 */
void logCoreDump()
{
  coredumpPrinted = false;
  TaskWdtResetAndDelay(500);
  Log->println(F("[INFO] Printing coredump and saving to coredump.hex on LittleFS"));
  TaskWdtDelay(5);

  size_t size = 0;
  size_t address = 0;

  // Verify integrity of the core dump image
  esp_err_t verifyErr = esp_core_dump_image_check();
  if (verifyErr != ESP_OK)
  {
    Log->printf(F("[ERROR] Core dump integrity check failed: %d\n"), verifyErr);

    // Attempt to erase the invalid core dump
    if (esp_core_dump_image_erase() == ESP_OK)
    {
      Log->println(F("[INFO] Invalid core dump erased"));
    }
    else
    {
      Log->println(F("[WARNING] Failed to erase invalid core dump"));
    }

    // Restart to allow a fresh core dump to be written if needed
    TaskWdtResetAndDelay(200);
    restartEsp(RestartReason::EraseInvalidCoreDump);

    return;
  }

  if (esp_core_dump_image_get(&address, &size) == ESP_OK)
  {
    if (size == 0 || size > 65536) // Safety for maximum partition size
    {
      Log->println(F("[ERROR] Invalid coredump size!"));
      return;
    }
    const esp_partition_t *pt = NULL;
    pt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");

    if (pt != NULL)
    {
      File file;
      uint8_t bf[256];
      char str_dst[640];
      int16_t toRead;

      if (size == 0 || size > pt->size)
      {
        Log->println(F("[ERROR] Invalid coredump size!"));
        return;
      }

      if (!fsReady)
      {
        Log->println(F("[ERROR] LittleFS Mount Failed"));
        return;
      }
      else
      {
        file = LittleFS.open("/coredump.hex", FILE_WRITE);
        if (!file)
        {
          Log->println(F("[ERROR] Failed to open /coredump.hex for writing"));
        }
        else
        {
          file.printf("%s\r\n", NUKI_REST_BRIDGE_HW);
          file.printf("%s\r\n", NUKI_REST_BRIDGE_BUILD);
        }
      }

      Serial.printf("%s\r\n", NUKI_REST_BRIDGE_HW);
      Serial.printf("%s\r\n", NUKI_REST_BRIDGE_BUILD);

      for (int16_t i = 0; i < (size / 256) + 1; i++)
      {
        strcpy(str_dst, "");
        toRead = (size - i * 256) > 256 ? 256 : (size - i * 256);

        esp_err_t er = esp_partition_read(pt, i * 256, bf, toRead);
        if (er != ESP_OK)
        {
          Serial.printf("FAIL [%x]", er);
          break;
        }

        for (int16_t j = 0; j < 256; j++)
        {
          char str_tmp[3];
          if (bf[j] <= 0x0F)
          {
            sprintf(str_tmp, "0%x", bf[j]);
          }
          else
          {
            sprintf(str_tmp, "%x", bf[j]);
          }
          strcat(str_dst, str_tmp);
        }
        Serial.printf("%s", str_dst);

        if (file)
        {
          file.printf("%s", str_dst);
        }
      }

      Serial.println("");

      if (file)
      {
        file.println("");
        file.close();
        Log->println(F("[INFO] Core dump successfully saved to /coredump.hex"));

        // Erase core dump after successful extraction
        if (esp_core_dump_image_erase() == ESP_OK)
        {
          Log->println(F("[INFO] Core dump erased from flash"));
        }
        else
        {
          Log->println(F("[WARNING] Failed to erase core dump after export"));
        }
      }
    }
    else
    {
      Serial.println(F("[ERROR] Partition NULL"));
    }
  }
  else
  {
    Serial.println(F("[ERROR] esp_core_dump_image_get() FAIL"));
  }
  coredumpPrinted = true;
}

void setup()
{

#ifdef DEBUG_NUKIBRIDGE
  esp_log_level_set("*", ESP_LOG_DEBUG);
  esp_log_level_set("nvs", ESP_LOG_INFO);
  esp_log_level_set("wifi", ESP_LOG_INFO);
#else
  // Set Log level to error for all TAGS
  esp_log_level_set("*", ESP_LOG_ERROR);
#endif

  initPreferences(preferences);

  Serial.begin(115200);

  initializeFileSystem();

  Log = new Logger(&Serial, preferences);
  Log->setLevel((Logger::msgtype)preferences->getInt(preference_log_level, Logger::MSG_INFO));

  initializeRestartReason();

  esp_reset_reason_t reason = esp_reset_reason();

  if (reason == ESP_RST_PANIC ||
      reason == ESP_RST_INT_WDT ||
      reason == ESP_RST_TASK_WDT)
  {
      logCoreDump();
  }

#ifdef DEBUG_NUKIBRIDGE
  if (fsReady)
  {
    listDir(LittleFS, "/", 1);
  }
#endif

  // default disableNetwork RTC_ATTR to false on power-on
  if (espRunning != 1)
  {
    espRunning = 1;
    forceEnableWebCfgServer = false;
    disableNetwork = false;
    wifiFallback = false;
    ethCriticalFailure = false;
  }

  if (preferences->getBool(preference_enable_bootloop_reset, false))
  {
    bootloopDetection();
  }

  Log->print("[INFO] Nuki Bridge version: ");
  Log->println(NUKI_REST_BRIDGE_VERSION);
  Log->print("[INFO] Nuki Bridge build date: ");
  Log->println(NUKI_REST_BRIDGE_DATE);

  deviceIdLock = new NukiDeviceId(preferences, preference_device_id_lock);

  CharBuffer::initialize(buffer_size);

  importExport = new ImportExport(preferences);

  network = new NukiNetwork(preferences, CharBuffer::get(), buffer_size, importExport);
  network->initialize();

  lockEnabled = preferences->getBool(preference_lock_enabled);

  if (network->isApOpen())
  {
    forceEnableWebCfgServer = true;
    lockEnabled = false;
  }

  if (lockEnabled)
  {
    bleScanner = new BleScanner::Scanner();
    // Scan interval and window according to Nuki recommendations:
    // https://developer.nuki.io/t/bluetooth-specification-questions/1109/27
    bleScanner->initialize(DEVICE_NAME, true, 40, 40);
    bleScanner->setScanDuration(0);
    bleScannerStarted = true;
  }

  Log->println(lockEnabled ? F("[INFO] Nuki Lock enabled") : F("[INFO] Nuki Lock disabled"));
  if (lockEnabled)
  {
    nuki = new NukiWrapper(DEVICE_NAME, deviceIdLock, bleScanner, network, preferences, CharBuffer::get(), buffer_size);
    nuki->initialize();
    lockStarted = true;
  }

  bleDone = true;

  startWebCfgServer();

#ifdef DEBUG_NUKIBRIDGE
  Log->printf(F("[DEBUG] Heap before setupTasks: %d bytes\r\n"), ESP.getFreeHeap());
#endif

  setupTasks();

#ifdef DEBUG_NUKIBRIDGE
  Log->printf(F("[DEBUG] Heap after setupTasks: %d bytes\r\n"), ESP.getFreeHeap());
  printTaskInfo();
#endif
}

void loop()
{
  Log->println(F("[DEBUG] run loop()"));
  // deletes the Arduino loop task
  vTaskDelete(NULL);
}