#include <Arduino.h>
#include "esp_http_client.h"
#include "esp_task_wdt.h"
#include "Config.h"
#include "esp32-hal-log.h"
#include "hal/wdt_hal.h"
#include "esp_chip_info.h"
#include "esp_netif_sntp.h"
#include "FS.h"
#include "SPIFFS.h"
#include "NukiWrapper.h"
#include "NukiNetwork.h"
#include "CharBuffer.hpp"
#include "NukiDeviceId.hpp"
#include "WebCfgServer.h"
#include "Logger.hpp"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "EspMillis.h"

Preferences *preferences = nullptr;
NukiNetwork *network = nullptr;
BleScanner::Scanner *bleScanner = nullptr;
NukiWrapper *nuki = nullptr;
NukiDeviceId *deviceIdLock = nullptr;
WebCfgServer *webCfgServer = nullptr;

bool lockEnabled = false;
bool wifiConnected = false;
bool rebootLock = false;

int64_t restartTs = (pow(2, 63) - (5 * 1000 * 60000)) / 1000;

RTC_NOINIT_ATTR int espRunning;
RTC_NOINIT_ATTR int restartReason;
RTC_NOINIT_ATTR uint64_t restartReasonValidDetect;
RTC_NOINIT_ATTR bool forceEnableWebCfgServer;
RTC_NOINIT_ATTR bool disableNetwork;
RTC_NOINIT_ATTR bool wifiFallback;
RTC_NOINIT_ATTR bool ethCriticalFailure;
bool timeSynced = false;

RestartReason currentRestartReason = RestartReason::NotApplicable;

// Taskhandles
TaskHandle_t nukiTaskHandle     = nullptr;
TaskHandle_t networkTaskHandle  = nullptr;
TaskHandle_t webCfgTaskHandle   = nullptr;

void cbSyncTime(struct timeval *tv)
{
  Log->println(F("NTP time synced"));
  timeSynced = true;
}

#ifdef DEBUG
void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Log->printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Log->println("- failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Log->println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Log->print("  DIR : ");
      Log->println(file.name());
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
      Log->print("  FILE: ");
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
#endif

void setup()
{
  preferences = new Preferences();
  preferences->begin("nukibridge", false);
  initPreferences(preferences);

#ifdef DEBUG
  Log->begin(115200);
  DebugLog *Log = new DebugLog(nullptr);
#else
  DebugLog *Log = new DebugLog(preferences);
#endif

  initializeRestartReason();

  if (SPIFFS.begin(true))
  {
#ifdef DEBUG
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

  Log->print("Nuki Bridge version: ");
  Log->println(NUKI_REST_BRIDGE_VERSION);
  Log->print("Nuki Bridge build date: ");
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

  Log->println(lockEnabled ? F("Nuki Lock enabled") : F("Nuki Lock disabled"));
  if (lockEnabled)
  {
    nuki = new NukiWrapper("NukiBridge", deviceIdLock, bleScanner, network, preferences, CharBuffer::get(), buffer_size);
    nuki->initialize();
  }

  if (!disableNetwork && (forceEnableWebCfgServer || preferences->getBool(preference_webcfgserver_enabled, true), false))
  {
    webCfgServer = new WebCfgServer(nuki, network, preferences);
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

  setupTasks();

}

void setupTasks() {

  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = 300000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&twdt_config);

  esp_chip_info_t info;
  esp_chip_info(&info);
  uint8_t espCores = info.cores;

  xTaskCreatePinnedToCore(
    nukiTask,         // Task-Funktion
    "nuki",           // Name des Tasks
    NUKI_TASK_SIZE,   // Stack-Größe (Wörter)
    NULL,             // Parameter
    1,                // Priorität
    &nukiTaskHandle,  // Task-Handle (optional)
    0                 // Core 0 für Lastverteilung
  );
  esp_task_wdt_add(nukiTaskHandle);

  xTaskCreatePinnedToCore(
    networkTask,            // Task-Funktion
    "ntw",                  // Name des Tasks
    NETWORK_TASK_SIZE,      // Stack-Größe (Wörter)
    NULL,                   // Parameter
    1,                      // Priorität
    &networkTaskHandle,     // Task-Handle (optional)
    (espCores > 1) ? 1 : 0  // Core 1
  );

  xTaskCreatePinnedToCore(
    webCfgTask,
    "WebCfg",
    WEBCFGSERVER_TASK_SIZE,
    NULL,
    1,
    &webCfgTaskHandle,
    (espCores > 1) ? 1 : 0
  );

  esp_task_wdt_add(networkTaskHandle);
}

// ------------------------
// Nuki-Task: führt update() aus
// ------------------------
void nukiTask(void* parameter)
{
  int64_t nukiLoopTs = 0;
  bool whiteListed = false;

  while (true) {
    if (disableNetwork || wifiConnected) {
      bleScanner->update();
      delay(20);

      bool needsPairing = (lockEnabled && !nuki->isPaired());

      if (needsPairing) {
        delay(2500);
      } else if (!whiteListed) {
        whiteListed = true;
        if (lockEnabled) {
          bleScanner->whitelist(nuki->getBleAddress());
        }
      }

      if (lockEnabled) {
        nuki->update(rebootLock);
        rebootLock = false;
      }
    }

    if (espMillis() - nukiLoopTs > 120000) {
      Log->println(F("nukiTask is running"));
      nukiLoopTs = espMillis();
    }

    esp_task_wdt_reset();
  }
}

// ------------------------
// Netzwerk-Task: führt update() aus
// ------------------------
void networkTask(void* parameter)
{
  int64_t networkLoopTs = 0;
  bool updateTime = true;

  while (true) {

    network->update();
    bool connected = network->isConnected();

    if (connected && updateTime) {
      if (preferences->getBool(preference_update_time, false)) {
        esp_netif_sntp_start();
      }
      updateTime = false;
    }

    wifiConnected = network->isWifiConnected();

    if (connected && lockEnabled) {
      rebootLock = network->update();
    }

    if (espMillis() - networkLoopTs > 120000) {
      Log->println(F("networkTask is running"));
      networkLoopTs = espMillis();
    }

    if (espMillis() > restartTs) {
      restartEsp(RestartReason::RestartTimer);
    }
    esp_task_wdt_reset();
  }
}

// ------------------------
// WebConfig-Task: bearbeitet HTTP-Anfragen
// ------------------------
void webCfgTask(void* parameter)
{
  int64_t webCfgLoopTs = 0;
  while(true)
  {

    webCfgServer->handleClient();
    if (espMillis() - webCfgLoopTs > 120000) {
      Log->println(F("webCfgTask is running"));
      webCfgLoopTs = espMillis();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
    esp_task_wdt_reset();
  }
}


void loop()
{
  // deletes the Arduino loop task
  vTaskDelete(NULL);
}