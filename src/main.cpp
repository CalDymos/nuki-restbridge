//Bibliothek NukiBleEsp in Version 2.1.0
//Bibliothek NimBLE-Arduino in Version 1.4.3
//Bibliothek Preferences in Version 3.1.1
//Bibliothek BleScanner in Version 1.1.0
//Bibliothek WiFi in Version 3.1.1
//Bibliothek Networking in Version 3.1.1
//Bibliothek WebServer in Version 3.1.1
//Bibliothek FS in Version 3.1.1
//Bibliothek HTTPClient in Version 3.1.1
//Bibliothek NetworkClientSecure in Version 3.1.1
//Bibliothek ArduinoJson in Version 6.21.5
//Bibliothek SPIFFS in Version 3.1.1
//Bibliothek Crc16 in Version 0.1.2
// Board: OLIMEX ESP32-POE-ISO
// Flash-Size: 4MB
// Partition Scheme: No OTA (NVS 20KB / 2MB APP /2MB SPIFFS) -> custom
// Flash Mode : DIO

#include <Arduino.h>
#include "Config.h"
#include <list>
#include "NukiWrapper.h"
#include "NukiNetworkLock.h"
#include "CharBuffer.h"
#include "NukiDeviceId.h"
#include "WebCfgServer.h"
#include "ArduinoJson.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "EspMillis.h"
#include "SPIFFS.h"
#include "esp_chip_info.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_netif_sntp.h"

#ifdef DEBUG
#include "FS.h"
#endif


NukiNetworkLock* networkLock = nullptr;
BleScanner::Scanner* bleScanner = nullptr;
NukiWrapper* nuki = nullptr;
NukiDeviceId* deviceIdLock = nullptr;

bool lockEnabled = false;
bool wifiConnected = false;
bool rebootLock = false;

TaskHandle_t nukiTaskHandle = nullptr;

int64_t restartTs = (pow(2, 63) - (5 * 1000 * 60000)) / 1000;

NukiNetwork* network = nullptr;
WebCfgServer* webCfgServer = nullptr;
Preferences* preferences = nullptr;

RTC_NOINIT_ATTR int espRunning;
RTC_NOINIT_ATTR int restartReason;
RTC_NOINIT_ATTR uint64_t restartReasonValidDetect;
RTC_NOINIT_ATTR bool forceEnableWebServer;
RTC_NOINIT_ATTR bool disableNetwork;
RTC_NOINIT_ATTR bool wifiFallback;
RTC_NOINIT_ATTR bool ethCriticalFailure;
bool timeSynced = false;

bool restartReason_isValid;
RestartReason currentRestartReason = RestartReason::NotApplicable;

TaskHandle_t networkTaskHandle = nullptr;

void cbSyncTime(struct timeval* tv) {
  Log->println(F("NTP time synced"));
  timeSynced = true;
}

#ifdef DEBUG
void listDir(fs::FS& fs, const char* dirname, uint8_t levels) {
  Log->printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Log->println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Log->println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Log->print("  DIR : ");
      Log->println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Log->print("  FILE: ");
      Log->print(file.name());
      Log->print("\tSIZE: ");
      Log->println(file.size());
    }

    if (file.size() > (int)(SPIFFS.totalBytes() * 0.4)) {
      SPIFFS.remove((String) "/" + file.name());
    }

    file = root.openNextFile();
  }
}
#endif

void networkTask(void* pvParameters) {

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
      rebootLock = networkLock->update();
    }

    webCfgServer->update();


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

void nukiTask(void* pvParameters) {
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
  esp_task_wdt_add(networkTaskHandle);
}


void setup() {
  preferences = new Preferences();
  preferences->begin("nukihub", false);
  initPreferences(preferences);

#ifdef DEBUG
  Log->begin(115200);
  DebugLog* Log = new DebugLog(nullptr);
#else
  DebugLog* Log = new DebugLog(preferences);
#endif

  initializeRestartReason();

  if (SPIFFS.begin(true)) {
#ifdef DEBUG
    listDir(SPIFFS, "/", 1);
#endif
  }

  //default disableNetwork RTC_ATTR to false on power-on
  if (espRunning != 1) {
    espRunning = 1;
    forceEnableWebServer = false;
    disableNetwork = false;
    wifiFallback = false;
    ethCriticalFailure = false;
  }

  Log->print("Nuki Hub version ");
  Log->println(NUKI_REST_BRIDGE_VERSION);
  Log->print("Nuki Hub build ");
  Log->println(NUKI_REST_BRIDGE_BUILD);

  deviceIdLock = new NukiDeviceId(preferences, preference_device_id_lock);

  char16_t buffer_size = CHAR_BUFFER_SIZE;
  CharBuffer::initialize(buffer_size);

  network = new NukiNetwork(preferences, CharBuffer::get(), buffer_size);
  network->initialize();

  lockEnabled = preferences->getBool(preference_lock_enabled);

  if (network->isApOpen()) {
    forceEnableWebServer = true;
    lockEnabled = false;
  }

  if (lockEnabled) {
    bleScanner = new BleScanner::Scanner();
    // Scan interval and window according to Nuki recommendations:
    // https://developer.nuki.io/t/bluetooth-specification-questions/1109/27
    bleScanner->initialize("NukiBridge", true, 40, 40);
    bleScanner->setScanDuration(0);
  }

  Log->println(lockEnabled ? F("Nuki Lock enabled") : F("Nuki Lock disabled"));
  if (lockEnabled) {
    networkLock = new NukiNetworkLock(network, preferences, CharBuffer::get(), buffer_size);

    if (!disableNetwork) {
      networkLock->initialize();
    }

    nuki = new NukiWrapper("NukiBridge", deviceIdLock, bleScanner, networkLock, preferences, CharBuffer::get(), buffer_size);
    nuki->initialize();
  }

  if (!disableNetwork && (forceEnableWebServer || preferences->getBool(preference_webcfgserver_enabled, true))) {
    if (forceEnableWebServer || preferences->getBool(preference_webcfgserver_enabled, true)) {

      webCfgServer = new WebCfgServer(nuki, network, preferences, network->networkDeviceType() == NetworkDeviceType::WiFi);
      webCfgServer->initialize();
    }
  }

  String timeserver = preferences->getString(preference_time_server, "pool.ntp.org");
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(timeserver.c_str());
  config.start = false;
  config.server_from_dhcp = true;
  config.renew_servers_after_new_IP = true;
  config.index_of_first_server = 1;

  if (network->networkDeviceType() == NetworkDeviceType::WiFi) {
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
  } else {
    config.ip_event_to_renew = IP_EVENT_ETH_GOT_IP;
  }
  config.sync_cb = cbSyncTime;
  esp_netif_sntp_init(&config);

  setupTasks();
}


void loop() {
  // deletes the Arduino loop task
  vTaskDelete(NULL);
}