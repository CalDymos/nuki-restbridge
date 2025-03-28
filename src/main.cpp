#include <Arduino.h>
#include "esp_http_client.h"
#include "esp_task_wdt.h"
#include "Config.h"
#include "esp32-hal-log.h"
#include "hal/wdt_hal.h"
#include "esp_chip_info.h"
#include "esp_netif_sntp.h"
#include "esp_core_dump.h"
#include "FS.h"
#include "SPIFFS.h"
#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
#include "esp_psram.h"
#endif
#include "NukiWrapper.h"
#include "NukiNetwork.h"
#include "CharBuffer.hpp"
#include "NukiDeviceId.hpp"
#include "WebCfgServer.h"
#include "Logger.hpp"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "EspMillis.h"

Preferences *preferences = nullptr;        // Pointer to non-volatile key-value storage (nvs).
NukiNetwork *network = nullptr;            // Main network interface (WiFi/Ethernet, REST API).
BleScanner::Scanner *bleScanner = nullptr; // BLE scanner to discover/connect Nuki devices.
NukiWrapper *nuki = nullptr;               // Core smart lock wrapper.
NukiDeviceId *deviceIdLock = nullptr;      // Unique device ID handler.
WebCfgServer *webCfgServer = nullptr;      // Web-based configuration interface.
DebugLog *Log = nullptr;                   // Global logger instance.

bool lockEnabled = false;   // Whether lock operations are currently permitted.
bool wifiConnected = false; // WiFi connection status.
bool rebootLock = false;    // Whether to reboot lock logic after failure.

int64_t restartTs = (pow(2, 63) - (5 * 1000 * 60000)) / 1000; // Time stamp for restarting the ESP to prevent the ESPtimer from overflowing

RTC_NOINIT_ATTR int espRunning;                    // Persisted runtime state across deep sleep/restart.
RTC_NOINIT_ATTR int restartReason;                 // Last restart reason code.
RTC_NOINIT_ATTR uint64_t restartReasonValidDetect; // Timestamp used for validating restart cause.
RTC_NOINIT_ATTR bool forceEnableWebCfgServer;      // Flag to force-enable web config mode.
RTC_NOINIT_ATTR bool disableNetwork;               // Flag to disable all network activity.
RTC_NOINIT_ATTR bool wifiFallback;                 // Whether WiFi fallback was triggered (e.g. AP mode).
RTC_NOINIT_ATTR bool ethCriticalFailure;           // Flag for Ethernet hardware failure (e.g. PHY).

bool coredumpPrinted = true; // Prevent repeated printing of core dump on each boot.
bool timeSynced = false;     // Whether NTP time sync was successful.

int lastHTTPeventId = -1;   // ID of last received HTTP event.
bool restartReason_isValid; // True if restart reason could be determined.

RestartReason currentRestartReason = RestartReason::NotApplicable; // Tracks current restart state (e.g. Watchdog, Manual, Error etc.).

// Taskhandles
TaskHandle_t nukiTaskHandle = nullptr;    // Handle for BLE/Nuki lock task.
TaskHandle_t networkTaskHandle = nullptr; // Handle for network-related task.
TaskHandle_t webCfgTaskHandle = nullptr;  // Handle for web config server task.

/**
 * @brief Callback function invoked by SNTP when the time is synchronized.
 * @param tv Pointer to timeval struct (unused).
 */
void cbSyncTime(struct timeval *tv)
{
  Log->println(F("[INFO] NTP time synced"));
  timeSynced = true;
}

/**
 * @brief Recursively lists files in a directory and optionally deletes large files.
 * @param fs Reference to filesystem instance (e.g. SPIFFS).
 * @param dirname Directory to list.
 * @param levels Recursion depth for subdirectories.
 */
void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Log->printf("[DEBUG] Listing directory: %s\r\n", dirname);

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
      Log->print("[DEBUG]  FILE: ");
      Log->print(file.name());
      Log->print("\tSIZE: ");
      Log->println(file.size());
    }

    if (file.size() > (int)(SPIFFS.totalBytes() * 0.4))
    {
      SPIFFS.remove((String) "/" + file.name());
    }

    file = root.openNextFile();
  }
}

/**
 * @brief Nuki-Task: handle Nuki events()
 */
void nukiTask(void *parameter)
{
  int64_t nukiLoopTs = 0;
  bool whiteListed = false;

  if (!nukiLoopTs)
    Log->println(F("[DEBUG] run nukiTask()"));

  while (true)
  {
    if (disableNetwork || wifiConnected)
    {
      bleScanner->update();
      delay(20);

      bool needsPairing = (lockEnabled && !nuki->isPaired());

      if (needsPairing)
      {
        delay(2500);
      }
      else if (!whiteListed)
      {
        whiteListed = true;
        if (lockEnabled)
        {
          bleScanner->whitelist(nuki->getBleAddress());
        }
      }

      if (lockEnabled)
      {
        nuki->update(rebootLock);
        rebootLock = false;
      }
    }

    if (espMillis() - nukiLoopTs > 120000)
    {
      Log->println(F("[DEBUG] nukiTask is running"));
      nukiLoopTs = espMillis();
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
      esp_task_wdt_reset();
    }
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

  while (true)
  {

    network->update();
    bool connected = network->isConnected();

    if (connected && updateTime)
    {
      if (preferences->getBool(preference_update_time, false))
      {
        esp_netif_sntp_start();
      }
      updateTime = false;
    }

    wifiConnected = network->isWifiConnected();

    if (connected && lockEnabled)
    {
      rebootLock = network->update();
    }

    if (espMillis() - networkLoopTs > 120000)
    {
      Log->println(F("[DEBUG] networkTask is running"));
      networkLoopTs = espMillis();
    }

    if (espMillis() > restartTs)
    {
      restartEsp(RestartReason::RestartTimer);
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
    if (esp_task_wdt_status(networkTaskHandle) == ESP_OK)
    {
      esp_task_wdt_reset();
    }
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
    vTaskDelay(2 / portTICK_PERIOD_MS);
    if (esp_task_wdt_status(webCfgTaskHandle) == ESP_OK)
    {
      esp_task_wdt_reset();
    }
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
      Log->printf("%s\t\t%d\t%d\t%d\t%d\n",
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

  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = 300000,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_err_t err = esp_task_wdt_reconfigure(&twdt_config);

  if (err != ESP_OK)
  {
    Log->printf("[ERROR] esp_task_wdt_reconfigure failed: %d\n", err);
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
  Log->printf("[DEBUG] Created webCfgTaskHandle: %p\n", webCfgTaskHandle);

  Log->println(F("[DEBUG] Adding webCfgTask to Watchdog..."));
  if (webCfgTaskHandle != NULL)
  {
    esp_err_t err = esp_task_wdt_add(webCfgTaskHandle);
    if (err != ESP_OK)
    {
      Log->printf("[ERROR] esp_task_wdt_add failed for webCfgTask: %d\n", err);
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
    Log->printf("[DEBUG] Created networkTaskHandle: %p\n", networkTaskHandle);
    Log->println(F("[DEBUG] Adding networkTask to Watchdog..."));
    if (networkTaskHandle != NULL)
    {
      esp_err_t err = esp_task_wdt_add(networkTaskHandle);
      if (err != ESP_OK)
      {
        Log->printf("[ERROR] esp_task_wdt_add failed for networkTask: %d\n", err);
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
        Log->printf("[ERROR] esp_task_wdt_add failed for nukiTask: %d\n", err);
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
  delay(500);
  Log->println(F("[INFO] Printing coredump and saving to coredump.hex on SPIFFS"));
  size_t size = 0;
  size_t address = 0;
  if (esp_core_dump_image_get(&address, &size) == ESP_OK)
  {
    const esp_partition_t *pt = NULL;
    pt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");

    if (pt != NULL)
    {
      File file;
      uint8_t bf[256];
      char str_dst[640];
      int16_t toRead;

      if (!SPIFFS.begin(true))
      {
        Log->println(F("[ERROR] SPIFFS Mount Failed"));
      }
      else
      {
        file = SPIFFS.open("/coredump.hex", FILE_WRITE);
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
          char str_tmp[2];
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
  preferences = new Preferences();
  preferences->begin("nukibridge", false);
  initPreferences(preferences);

  Serial.begin(115200);
  Log = new DebugLog(&Serial, preferences);
  Log->setLogLevel((DebugLog::msgtype) preferences->getInt(preference_log_level, 0));

  initializeRestartReason();

  if (esp_reset_reason() == esp_reset_reason_t::ESP_RST_PANIC ||
      esp_reset_reason() == esp_reset_reason_t::ESP_RST_INT_WDT ||
      esp_reset_reason() == esp_reset_reason_t::ESP_RST_TASK_WDT ||
      esp_reset_reason() == esp_reset_reason_t::ESP_RST_WDT)
  {
    logCoreDump();
  }

  if (SPIFFS.begin(true))
  {
#ifdef DEBUG_NUKIBRIDGE
    listDir(SPIFFS, "/", 1);
#endif
  }

  // default disableNetwork RTC_ATTR to false on power-on
  if (espRunning != 1)
  {
    espRunning = 1;
    forceEnableWebCfgServer = false;
    disableNetwork = false;
    wifiFallback = false;
    ethCriticalFailure = false;
  }

  Log->print("[DEBUG] Nuki Bridge version: ");
  Log->println(NUKI_REST_BRIDGE_VERSION);
  Log->print("[DEBUG] Nuki Bridge build date: ");
  Log->println(NUKI_REST_BRIDGE_BUILD);

  deviceIdLock = new NukiDeviceId(preferences, preference_device_id_lock);

  char16_t buffer_size = CHAR_BUFFER_SIZE;
  CharBuffer::initialize(buffer_size);

  network = new NukiNetwork(preferences, CharBuffer::get(), buffer_size);
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
    bleScanner->initialize("NukiBridge", true, 40, 40);
    bleScanner->setScanDuration(0);
  }

  Log->println(lockEnabled ? F("[DEBUG] Nuki Lock enabled") : F("[DEBUG] Nuki Lock disabled"));
  if (lockEnabled)
  {
    nuki = new NukiWrapper("NukiBridge", deviceIdLock, bleScanner, network, preferences, CharBuffer::get(), buffer_size);
    nuki->initialize();
  }

  if (!disableNetwork && (forceEnableWebCfgServer || preferences->getBool(preference_webcfgserver_enabled, true)))
  {
    webCfgServer = new WebCfgServer(nuki, network, preferences);
    Log->println("[DEBUG] Start to initialize WebCfgServer...");
    webCfgServer->initialize();
  }

  String timeserver = preferences->getString(preference_time_server, "pool.ntp.org");
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(timeserver.c_str());
  config.start = false;
  config.server_from_dhcp = true;
  config.renew_servers_after_new_IP = true;
  config.index_of_first_server = 1;

  if (network->networkDeviceType() == NetworkDeviceType::WiFi)
  {
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
  }
  else
  {
    config.ip_event_to_renew = IP_EVENT_ETH_GOT_IP;
  }
  config.sync_cb = cbSyncTime;
  esp_netif_sntp_init(&config);

#ifdef DEBUG_NUKIBRIDGE
  Log->printf("[DEBUG] Heap before setupTasks: %d bytes\n", ESP.getFreeHeap());
#endif

  setupTasks();

#ifdef DEBUG_NUKIBRIDGE
  Log->printf("[DEBUG] Heap after setupTasks: %d bytes\n", ESP.getFreeHeap());
  printTaskInfo();
#endif
}

void loop()
{
  Log->println(F("[DEBUG] run loop()"));
  // deletes the Arduino loop task
  vTaskDelete(NULL);
}