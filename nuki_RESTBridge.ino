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
// Partition Scheme: No OTA (2MB APP /2MB SPIFFS)
// Flash Mode : DIO

#include "Config.h"
#include <Arduino.h>
#include <list>
// include NukiBleEsp32 library
//-------------------------------------------
#include "lib/NukiBleEsp32/src/NukiBle.h"
#include "lib/NukiBleEsp32/src/NukiLock.h"
#include "lib/NukiBleEsp32/src/NukiLockUtils.h"
#include "lib/NukiBleEsp32/src/NukiUtils.h"
#include "lib/NukiBleEsp32/src/NukiBle.cpp"
#include "lib/NukiBleEsp32/src/NukiLock.cpp"
#include "lib/NukiBleEsp32/src/NukiLockUtils.cpp"
#include "lib/NukiBleEsp32/src/NukiUtils.cpp"
//------------------------------------------
#include "NukiWrapper.h"
#if CONNECT_OVER_LAN
#include <ETH.h>
#else
#include <WiFi.h>
#endif

#include <WebServer.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"
#include "Logger.h"
#include <Preferences.h>
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "EspMillis.h"
#include "SPIFFS.h"
#include "esp_chip_info.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"


#define LANEvent_t arduino_event_id_t

static bool LAN_connected = false;

WebServer server(SERVER_PORT);

TaskHandle_t nukiTaskHandle = nullptr;
TaskHandle_t webServerTaskHandle = nullptr;

StaticJsonDocument<512> jsonDocument;
char buffer[512];

#define BUTTON_PRESSED() (!digitalRead(34))

Preferences *preferences = nullptr;

BridgeApiToken *Apitoken = nullptr;
bool ApiEnabled = false;
bool restartOnDisconnect = false;

// Nuki Device Settings
NukiDeviceId *deviceIdLock = nullptr;
NukiLock::NukiLock *nukiLock = nullptr;
BleScanner::Scanner *scanner = nullptr;
NukiLock::Config config;
NukiLock::BatteryReport batteryReport;

NukiLock::KeyTurnerState retrievedKeyTurnerState;
std::list<NukiLock::LogEntry> requestedLogEntries;
std::list<Nuki::KeypadEntry> requestedKeypadEntries;
std::list<Nuki::AuthorizationEntry> requestedAuthorizationEntries;
std::list<NukiLock::TimeControlEntry> requestedTimeControlEntries;

int64_t restartTs = (pow(2, 63) - (5 * 1000 * 60000)) / 1000;

RTC_NOINIT_ATTR int restartReason;
RTC_NOINIT_ATTR uint64_t restartReasonValidDetect;
RTC_NOINIT_ATTR bool rebuildGpioRequested;
bool restartReason_isValid;
RestartReason currentRestartReason = RestartReason::NotApplicable;

// define Max. 3 URLs
static String callbackURLs[3] = { "", "", "" };

void addKeypadEntry() {
  // Nuki::NewKeypadEntry newKeypadEntry;
  // unsigned char nameBuff[20] = "test";

  // newKeypadEntry.code = 111111;
  // memcpy(newKeypadEntry.name, nameBuff, 20);
  // newKeypadEntry.timeLimited = 1;
  // newKeypadEntry.allowedFromYear = 2022;
  // newKeypadEntry.allowedFromMonth = 2;
  // newKeypadEntry.allowedFromDay = 1;
  // newKeypadEntry.allowedFromHour = 0;
  // newKeypadEntry.allowedFromMin = 0;
  // newKeypadEntry.allowedFromSec = 0;
  // newKeypadEntry.allowedUntilYear = 2023;
  // newKeypadEntry.allowedUntilMonth = 1;
  // newKeypadEntry.allowedUntilDay = 1;
  // newKeypadEntry.allowedUntilHour = 0;
  // newKeypadEntry.allowedUntilMin = 0;
  // newKeypadEntry.allowedUntilSec = 0;
  // newKeypadEntry.allowedFromTimeHour = 0;
  // newKeypadEntry.allowedFromTimeMin = 0;
  // newKeypadEntry.allowedUntilTimeHour = 23;
  // newKeypadEntry.allowedUntilTimeMin = 59;

  // nukiLock.addKeypadEntry(newKeypadEntry);
}

void getBatteryReport() {
  uint8_t result = nukiLock->requestBatteryReport(&batteryReport);
  if (result == 1) {
    Log->printf("Bat report voltage: %d Crit state: %d, start temp: %d\r\n", batteryReport.batteryVoltage, batteryReport.criticalBatteryState, batteryReport.startTemperature);
  } else {
    Log->printf("Bat report failed: %d", result);
  }
}

bool getKeyTurnerState() {
  uint8_t result = nukiLock->requestKeyTurnerState(&retrievedKeyTurnerState);
  if (result == 1) {
    Log->printf("Bat crit: %d, Bat perc:%d lock state: %d %d:%d:%d",
                nukiLock->isBatteryCritical(), nukiLock->getBatteryPerc(), retrievedKeyTurnerState.lockState, retrievedKeyTurnerState.currentTimeHour,
                retrievedKeyTurnerState.currentTimeMinute, retrievedKeyTurnerState.currentTimeSecond);
  } else {
    Log->printf("cmd failed: %d", result);
  }
  return result;
}

void requestLogEntries() {
  // uint8_t result = nukiLock.retrieveLogEntries(0, 10, 0, true);
  // if (result == 1) {
  //   delay(5000);
  //   nukiLock.getLogEntries(&requestedLogEntries);
  //   std::list<NukiLock::LogEntry>::iterator it = requestedLogEntries.begin();
  //   while (it != requestedLogEntries.end()) {
  //     Log->printf("Log[%d] %d-%d-%d %d:%d:%d", it->index, it->timeStampYear, it->timeStampMonth, it->timeStampDay, it->timeStampHour, it->timeStampMinute, it->timeStampSecond);
  //     it++;
  //   }
  // } else {
  //   Log->printf("get log failed: %d", result);
  // }
}

void requestKeyPadEntries() {
  // uint8_t result = nukiLock.retrieveKeypadEntries(0, 10);
  // if (result == 1) {
  //   delay(5000);
  //   nukiLock.getKeypadEntries(&requestedKeypadEntries);
  //   std::list<Nuki::KeypadEntry>::iterator it = requestedKeypadEntries.begin();
  //   while (it != requestedKeypadEntries.end()) {
  //     Log->printf("Keypad entry[%d] %d", it->codeId, it->code);
  //     it++;
  //   }
  // } else {
  //   Log->printf("get keypadentries failed: %d", result);
  // }
}

void requestAuthorizationEntries() {
  // uint8_t result = nukiLock.retrieveAuthorizationEntries(0, 10);
  // if (result == 1) {
  //   delay(5000);
  //   nukiLock.getAuthorizationEntries(&requestedAuthorizationEntries);
  //   std::list<Nuki::AuthorizationEntry>::iterator it = requestedAuthorizationEntries.begin();
  //   while (it != requestedAuthorizationEntries.end()) {
  //     Log->printf("Authorization entry[%d] type: %d name: %s", it->authId, it->idType, it->name);
  //     it++;
  //   }
  // } else {
  //   Log->printf("get authorization entries failed: %d", result);
  // }
}

void setPincode(uint16_t pincode) {
  uint8_t result = nukiLock->setSecurityPin(pincode);
  if (result == 1) {
    Log->println("Set pincode done");

  } else {
    Log->printf("Set pincode failed: %d\r\n", result);
  }
}

void addTimeControl(uint8_t weekdays, uint8_t hour, uint8_t minute, NukiLock::LockAction lockAction) {
  // NukiLock::NewTimeControlEntry newEntry;
  // newEntry.weekdays = weekdays;
  // newEntry.timeHour = hour;
  // newEntry.timeMin = minute;
  // newEntry.lockAction = lockAction;

  // nukiLock.addTimeControlEntry(newEntry);
}

void requestTimeControlEntries() {
  // Nuki::CmdResult result = nukiLock.retrieveTimeControlEntries();
  // if (result == Nuki::CmdResult::Success) {
  //   delay(5000);
  //   nukiLock.getTimeControlEntries(&requestedTimeControlEntries);
  //   std::list<NukiLock::TimeControlEntry>::iterator it = requestedTimeControlEntries.begin();
  //   while (it != requestedTimeControlEntries.end()) {
  //     Log->printf("TimeEntry[%d] weekdays:%d %d:%d enabled: %d lock action: %d", it->entryId, it->weekdays, it->timeHour, it->timeMin, it->enabled, it->lockAction);
  //     it++;
  //   }
  // } else {
  //   Log->printf("get log failed: %d, error %d", result, nukiLock.getLastError());
  // }
}

void getConfig() {
  if (nukiLock->requestConfig(&config) == 1) {
    Log->printf("Name: %s\r\n", config.name);
  } else {
    Log->println("getConfig failed");
  }
}

bool notified = false;
class Handler : public Nuki::SmartlockEventHandler {
public:
  virtual ~Handler(){};
  void notify(Nuki::EventType eventType) {
    notified = true;
  }
};

Handler handler;

void setupRouting() {
  server.on("/auth", nuki_auth);
  server.on("/configAuth", nuki_configAuth);
  server.on("/list", nuki_list);
  server.on("/lockState", nuki_lockState);
  server.on("/lockAction", nuki_lockAction);
  server.on("/lock", nuki_lock);
  server.on("/unlock", nuki_unlock);
  server.on("/unpair", nuki_unpair);
  server.on("/info", nuki_info);
  server.on("/callback/add", nuki_callback_add);
  server.on("/callback/list", nuki_callback_list);
  server.on("/callback/remove", nuki_callback_remove);
  server.on("/log", nuki_log);
  server.on("/clearlog", nuki_clearlog);

  server.begin();
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
    1536,             // Stack-Größe (Wörter)
    NULL,             // Parameter
    1,                // Priorität
    &nukiTaskHandle,  // Task-Handle (optional)
    0                 // Core 0 für Lastverteilung
  );
  esp_task_wdt_add(nukiTaskHandle);

  xTaskCreatePinnedToCore(
    webServerTask,          // Task-Funktion
    "webServer",            // Name des Tasks
    1280,                   // Stack-Größe (Wörter)
    NULL,                   // Parameter
    1,                      // Priorität
    &webServerTaskHandle,   // Task-Handle (optional)
    (espCores > 1) ? 1 : 0  // Core 1
  );
  esp_task_wdt_add(webServerTaskHandle);
}



void nuki_auth() {
  Log->println("Enable API and Get Token");
  unsigned long currentMillis = 0;
  unsigned long previousMillis = 0;
  bool confirmed = false;
  int count = 0;

  for (int countSec = 0; countSec < 30;) {
    currentMillis = espMillis();
    if (currentMillis - previousMillis > 100) {
      previousMillis = currentMillis;
      if (BUTTON_PRESSED()) {
        Log->println("confirmed");
        confirmed = true;
        break;
      }
      count++;
    }
    if (count > 9) {
      count = 0;
      countSec++;
    }
  }
  if (confirmed) {
    if (strlen(Apitoken->get()) == 0) {
      Apitoken->assignNewToken();
    }

    ApiEnabled = true;
    preferences->putBool(preference_API_enabled, ApiEnabled);

    jsonDocument.clear();
    jsonDocument["token"] = Apitoken->get();
    jsonDocument["success"] = true;
    serializeJson(jsonDocument, buffer);
    server.send(200, "application/json", buffer);
  } else {
    Log->println("not confirmed");
    server.send(403, "text/html");
  }
}

void nuki_configAuth() {
  Log->println("Enable / Disable API");
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    if (server.hasArg("enable") && (server.arg("enable").toInt() == 0 || server.arg("enable").toInt() == 1)) {
      if (server.arg("enable").toInt() == 0) {

        ApiEnabled = false;
      } else {

        ApiEnabled = true;
      }
      preferences->putBool(preference_API_enabled, ApiEnabled);
      jsonDocument.clear();
      jsonDocument["success"] = true;
      serializeJson(jsonDocument, buffer);

      server.send(200, "application/json", buffer);
    } else {
      server.send(400, "text/html");
    }
  } else {
    server.send(401, "text/html");
  }
}

void nuki_get_lastKnownState(void *obj, bool nested = false) {

  if (nested) {
    //JsonObject *doc = (JsonObject *)obj;
  } else {
    //JsonDocument *doc = (JsonDocument *)obj;
  }
}

void nuki_list() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    jsonDocument.clear();
    jsonDocument["nukiId"] = config.nukiId;
    jsonDocument["deviceType"] = 0;
    jsonDocument["name"] = config.name;
    JsonObject doc = jsonDocument["lastKnownState"].to<JsonObject>();  // JsonObject doc = jsonDocument.createNestedObject("lastKnownState");
    doc["mode"] = (int)retrievedKeyTurnerState.nukiState;
    doc["state"] = (int)retrievedKeyTurnerState.lockState;
    char stateName[20];
    NukiLock::lockstateToString(retrievedKeyTurnerState.lockState, stateName);
    doc["stateName"] = stateName;
    bool critical = (retrievedKeyTurnerState.criticalBatteryState & 0b00000001) > 0;
    bool charging = (retrievedKeyTurnerState.criticalBatteryState & 0b00000010) > 0;
    uint8_t level = (retrievedKeyTurnerState.criticalBatteryState & 0b11111100) >> 1;
    doc["batteryCritical"] = critical;
    doc["batteryCharging"] = charging;
    doc["batteryChargeState"] = level;
    doc["keypadBatteryCritical"] = false;  // TODO: Get from api
    doc["doorsensorState"] = (int)retrievedKeyTurnerState.doorSensorState;
    char doorsensorStateName[20];
    NukiLock::doorSensorStateToString(retrievedKeyTurnerState.doorSensorState, doorsensorStateName);
    doc["doorsensorStateName"] = doorsensorStateName;
    char timestamp[36];
    sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02d+%02d:00", (int)retrievedKeyTurnerState.currentTimeYear, (int)retrievedKeyTurnerState.currentTimeMonth,
            (int)retrievedKeyTurnerState.currentTimeDay, (int)retrievedKeyTurnerState.currentTimeHour,
            (int)retrievedKeyTurnerState.currentTimeMinute, (int)retrievedKeyTurnerState.currentTimeSecond,
            (int)retrievedKeyTurnerState.timeZoneOffset);
    doc["timestamp"] = timestamp;
    serializeJson(jsonDocument, buffer);

    server.send(200, "application/json", buffer);
  } else {
    server.send(401, "text/html");
  }
}

void nuki_lockState() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    if (server.hasArg("nukiId") && server.arg("nukiId") == (char *)config.nukiId) {
      if ((nukiLock->requestConfig(&config) == 3) || (nukiLock->requestBatteryReport(&batteryReport) == 3) || (nukiLock->requestKeyTurnerState(&retrievedKeyTurnerState) == 3)) {
        server.send(503, "text/html");
        return;
      }

      jsonDocument.clear();
      jsonDocument["mode"] = (int)retrievedKeyTurnerState.nukiState;
      jsonDocument["state"] = (int)retrievedKeyTurnerState.lockState;
      char stateName[20];
      NukiLock::lockstateToString(retrievedKeyTurnerState.lockState, stateName);
      jsonDocument["stateName"] = stateName;
      bool critical = (retrievedKeyTurnerState.criticalBatteryState & 0b00000001) > 0;
      bool charging = (retrievedKeyTurnerState.criticalBatteryState & 0b00000010) > 0;
      uint8_t level = (retrievedKeyTurnerState.criticalBatteryState & 0b11111100) >> 1;
      jsonDocument["batteryCritical"] = critical;
      jsonDocument["batteryCharging"] = charging;
      jsonDocument["batteryChargeState"] = level;
      jsonDocument["keypadBatteryCritical"] = false;  // TODO: Get from api
      jsonDocument["doorsensorState"] = (int)retrievedKeyTurnerState.doorSensorState;
      char doorsensorStateName[20];
      NukiLock::doorSensorStateToString(retrievedKeyTurnerState.doorSensorState, doorsensorStateName);
      jsonDocument["doorsensorStateName"] = doorsensorStateName;
      jsonDocument["success"] = true;

      serializeJson(jsonDocument, buffer);

      server.send(200, "application/json", buffer);
    } else {
      server.send(404, "text/html");
    }
  } else {
    server.send(401, "text/html");
  }
}

void nuki_lockAction() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    if (server.hasArg("nukiId") && server.arg("nukiId") == (char *)config.nukiId) {
      if (server.hasArg("action")) {
        NukiLock::LockAction lockAction = (NukiLock::LockAction)server.arg("action").toInt();
        if (nukiLock->lockAction(lockAction) == NukiLock::CmdResult::Success) {
          logToBridgeFile(F("SmartLock"), "Lock action success: " + String((int)lockAction));
          jsonDocument.clear();
          jsonDocument["mode"] = (int)retrievedKeyTurnerState.nukiState;
          jsonDocument["state"] = (int)retrievedKeyTurnerState.lockState;
          char stateName[20];
          NukiLock::lockstateToString(retrievedKeyTurnerState.lockState, stateName);
          jsonDocument["stateName"] = stateName;
          bool critical = (retrievedKeyTurnerState.criticalBatteryState & 0b00000001) > 0;
          bool charging = (retrievedKeyTurnerState.criticalBatteryState & 0b00000010) > 0;
          uint8_t level = (retrievedKeyTurnerState.criticalBatteryState & 0b11111100) >> 1;
          jsonDocument["batteryCritical"] = critical;
          jsonDocument["batteryCharging"] = charging;
          jsonDocument["batteryChargeState"] = level;
          jsonDocument["keypadBatteryCritical"] = false;  // TODO: Get from api
          jsonDocument["doorsensorState"] = (int)retrievedKeyTurnerState.doorSensorState;
          char doorsensorStateName[20];
          NukiLock::doorSensorStateToString(retrievedKeyTurnerState.doorSensorState, doorsensorStateName);
          jsonDocument["doorsensorStateName"] = doorsensorStateName;
          jsonDocument["success"] = true;

          serializeJson(jsonDocument, buffer);

          server.send(200, "application/json", buffer);
        } else {
          logToBridgeFile(F("SmartLock"), "Lock action failed: " + String((int)lockAction));
          server.send(400, "text/html");
        }

      } else {
        server.send(400, "text/html");
      }
    } else {
      server.send(404, "text/html");
    }
  } else {
    server.send(401, "text/html");
  }
}

void nuki_lock() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    if (server.hasArg("nukiId") && server.arg("nukiId") == (char *)config.nukiId) {
      if (nukiLock->lockAction(NukiLock::LockAction::Lock) == NukiLock::CmdResult::Success) {
        jsonDocument.clear();
        bool critical = (retrievedKeyTurnerState.criticalBatteryState & 0b00000001) > 0;
        jsonDocument["batteryCritical"] = critical;
        jsonDocument["success"] = true;

        serializeJson(jsonDocument, buffer);

        server.send(200, "application/json", buffer);
      } else {
        server.send(503, "text/html");
      }
    } else {
      server.send(404, "text/html");
    }
  } else {
    server.send(401, "text/html");
  }
}

void nuki_unlock() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    if (server.hasArg("nukiId") && server.arg("nukiId") == (char *)config.nukiId) {
      if (nukiLock->lockAction(NukiLock::LockAction::Unlock) == NukiLock::CmdResult::Success) {
        jsonDocument.clear();
        bool critical = (retrievedKeyTurnerState.criticalBatteryState & 0b00000001) > 0;
        jsonDocument["batteryCritical"] = critical;
        jsonDocument["success"] = true;

        serializeJson(jsonDocument, buffer);

        server.send(200, "application/json", buffer);
      } else {
        server.send(503, "text/html");
      }
    } else {
      server.send(404, "text/html");
    }
  } else {
    server.send(401, "text/html");
  }
}

void nuki_unpair() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    if (server.hasArg("nukiId") && server.arg("nukiId") == (char *)config.nukiId) {
      nukiLock->unPairNuki();
      jsonDocument.clear();
      jsonDocument["success"] = true;

      serializeJson(jsonDocument, buffer);

      server.send(200, "application/json", buffer);

    } else {
      server.send(404, "text/html");
    }
  } else {
    server.send(401, "text/html");
  }
}
void nuki_info() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    jsonDocument.clear();

    jsonDocument["bridgeType"] = (int)1;
    JsonObject ids = jsonDocument["ids"].to<JsonObject>();  // JsonObject ids = jsonDocument.createNestedObject("ids");
    ids["hardwareId"] = NUKI_REST_BRIDGE_HW_ID;
    ids["serverId"] = NUKI_REST_BRIDGE_HW_ID;
    JsonObject versions = jsonDocument["versions"].to<JsonObject>();  // JsonObject versions = jsonDocument.createNestedObject("versions");
    versions["firmwareVersion"] = NUKI_REST_BRIDGE_VERSION;
    versions["wifiFirmwareVersion"] = NUKI_REST_BRIDGE_WIFI_VERSION;
    uint64_t uptime = esp_timer_get_time();
    uint32_t uptime_low = uptime % 0xFFFFFFFF;
    uint32_t uptime_high = (uptime >> 32) % 0xFFFFFFFF;
    jsonDocument["uptime"] = (uptime_low + uptime_high);
    char timestamp[36];
    sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02dZ", (int)retrievedKeyTurnerState.currentTimeYear, (int)retrievedKeyTurnerState.currentTimeMonth,
            (int)retrievedKeyTurnerState.currentTimeDay, (int)retrievedKeyTurnerState.currentTimeHour,
            (int)retrievedKeyTurnerState.currentTimeMinute, (int)retrievedKeyTurnerState.currentTimeSecond);
    jsonDocument["currentTime"] = timestamp;
    jsonDocument["serverConnected"] = false;
    JsonObject scanResults = jsonDocument["scanResults"].to<JsonObject>();  // JsonObject scanResults = jsonDocument.createNestedObject("scanResults");
    scanResults["nukiId"] = config.nukiId;
    scanResults["type"] = 0;
    scanResults["name"] = config.name;
    scanResults["rssi"] = nukiLock->getRssi();
    scanResults["paired"] = nukiLock->isPairedWithLock();

    serializeJson(jsonDocument, buffer);

    server.send(200, "application/json", buffer);
  } else {
    server.send(401, "text/html");
  }
}

void saveCallbackToPreferences(int index, const String &url) {
  char key[16];
  sprintf(key, "%s%d", preference_callback_key_prefix, index);
  preferences->putString(key, url);
}

String buildLockStateJson() {
  StaticJsonDocument<256> doc;
  doc["nukiId"] = config.nukiId;  // z. B. 11
  doc["deviceType"] = 0;          // 0 = Smart Lock
  doc["mode"] = (int)retrievedKeyTurnerState.nukiState;
  doc["state"] = (int)retrievedKeyTurnerState.lockState;
  char stateName[20];
  NukiLock::lockstateToString(retrievedKeyTurnerState.lockState, stateName);
  doc["stateName"] = stateName;

  bool critical = (retrievedKeyTurnerState.criticalBatteryState & 0b00000001) > 0;
  bool charging = (retrievedKeyTurnerState.criticalBatteryState & 0b00000010) > 0;
  uint8_t level = (retrievedKeyTurnerState.criticalBatteryState & 0b11111100) >> 1;
  doc["batteryCritical"] = critical;
  doc["batteryCharging"] = charging;
  doc["batteryChargeState"] = level;

  // Falls Türsensor
  doc["doorsensorState"] = (int)retrievedKeyTurnerState.doorSensorState;
  char doorsensorStateName[20];
  NukiLock::doorSensorStateToString(retrievedKeyTurnerState.doorSensorState, doorsensorStateName);
  doc["doorsensorStateName"] = doorsensorStateName;

  // ggf. Zeitstempel
  // ...

  String output;
  serializeJson(doc, output);
  return output;
}

void sendLockStateCallback(const String &url, const String &jsonData) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");  // Wichtig für JSON-POST
  int httpCode = http.POST((uint8_t *)jsonData.c_str(), jsonData.length());
  if (httpCode > 0) {
    // Erfolg oder Fehlercode
    Log->printf("Callback to %s, Code: %d\n", url.c_str(), httpCode);
  } else {
    Log->printf("Callback to %s failed: %s\n", url.c_str(), http.errorToString(httpCode).c_str());
  }
  http.end();
}


void nuki_callback_add() {
  // 1) Token prüfen
  if (!server.hasArg("token") || server.arg("token") != Apitoken->get()) {
    server.send(401, "text/html");
    return;
  }

  // 2) URL-Parameter prüfen
  if (!server.hasArg("url")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"missing url\"}");
    return;
  }
  String url = server.arg("url");
  if (url.length() == 0 || url.length() > 254) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"invalid or too long url\"}");
    return;
  }
  // Optionale Prüfung: kein HTTPS?
  if (url.startsWith("https://")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"HTTPS not supported\"}");
    return;
  }

  // 3) Freien Slot suchen
  int freeSlot = -1;
  for (int i = 0; i < 3; i++) {
    if (callbackURLs[i].isEmpty()) {
      freeSlot = i;
      break;
    }
  }
  if (freeSlot == -1) {
    // Alle 3 Plätze voll
    server.send(400, "application/json", "{\"success\":false,\"message\":\"all 3 callbacks used\"}");
    return;
  }

  // 4) Speichern
  callbackURLs[freeSlot] = url;
  saveCallbackToPreferences(freeSlot, url);

  // 5) Antwort
  StaticJsonDocument<200> doc;
  doc["success"] = true;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void nuki_callback_list() {
  // 1) Token prüfen
  if (!server.hasArg("token") || server.arg("token") != Apitoken->get()) {
    server.send(401, "text/html");
    return;
  }

  // 2) JSON bauen
  StaticJsonDocument<512> doc;
  JsonArray arr = doc.createNestedArray("callbacks");

  for (int i = 0; i < 3; i++) {
    if (!callbackURLs[i].isEmpty()) {
      JsonObject cb = arr.createNestedObject();
      cb["id"] = i;
      cb["url"] = callbackURLs[i];
    }
  }

  // 3) Senden
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void nuki_callback_remove() {
  // 1) Token prüfen
  if (!server.hasArg("token") || server.arg("token") != Apitoken->get()) {
    server.send(401, "text/html");
    return;
  }

  // 2) ID prüfen
  if (!server.hasArg("id")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"missing id\"}");
    return;
  }
  int index = server.arg("id").toInt();
  if (index < 0 || index > 2) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"invalid id\"}");
    return;
  }

  // 3) Entfernen
  callbackURLs[index] = "";
  saveCallbackToPreferences(index, "");  // ggf. leere String

  // 4) Antwort
  StaticJsonDocument<128> doc;
  doc["success"] = true;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// -----------------------------------------------------------
// Prüft, ob LOG_FILENAME größer als MAX_LOG_FILE_SIZE kB ist
// -----------------------------------------------------------
bool isLogTooBig() {
  File f = SPIFFS.open(LOG_FILENAME, FILE_READ);
  if (!f) return false;  // Datei existiert nicht -> nicht zu groß

  size_t sizeBytes = f.size();
  f.close();

  // Vergleiche Dateigröße mit 500 * 1024 Bytes
  return (sizeBytes > (MAX_LOG_FILE_SIZE * 1024UL));
}

// -----------------------------------------------------------
// Löscht das aktuelle LOG_FILENAME (falls vorhanden)
// und legt eine neue, leere Datei an
// -----------------------------------------------------------
void clearLog() {
  if (SPIFFS.exists(LOG_FILENAME)) {
    SPIFFS.remove(LOG_FILENAME);
  }
  // Neue, leere Datei anlegen
  File f = SPIFFS.open(LOG_FILENAME, FILE_WRITE);
  if (f) {
    f.close();
  }
}

// -----------------------------------------------------------
// Schreibt einen JSON-Logeintrag in LOG_FILENAME.
// Falls Datei > MAX_LOG_FILE_SIZE, wird das Log geleert.
// -----------------------------------------------------------
void logToBridgeFile(const String &deviceType, String message) {
  // 1) Nachricht auf MAX_LOG_MESSAGE_LEN kürzen
  if (message.length() > MAX_LOG_MESSAGE_LEN) {
    message = message.substring(0, MAX_LOG_MESSAGE_LEN);
  }

  // 2) Dateigröße prüfen, wenn zu groß -> löschen
  if (isLogTooBig()) {
    Log->println(F("Log file too big, clearing..."));
    clearLog();
  }

  // 3) JSON-Eintrag zusammenbauen (eine Zeile)
  StaticJsonDocument<256> doc;

  // Beispiel: Dummy-Zeitstempel
  char timeStr[25];
  snprintf(timeStr, sizeof(timeStr), "2025-02-25T12:34:56Z");
  doc["timestamp"] = timeStr;
  doc["deviceType"] = deviceType;
  doc["message"] = message;

  String line;
  serializeJson(doc, line);

  // 4) An Datei anhängen
  File f = SPIFFS.open(LOG_FILENAME, FILE_APPEND);
  if (!f) {
    Log->println(F("Failed to open log file for appending"));
    return;
  }
  f.println(line);
  f.close();
}

void nuki_log() {
  // 1) Token prüfen
  if (!server.hasArg("token") || server.arg("token") != Apitoken->get()) {
    server.send(401, "text/html");
    return;
  }

  // 2) offset / count
  int offset = 0;
  if (server.hasArg("offset")) {
    offset = server.arg("offset").toInt();
    if (offset < 0) offset = 0;
  }
  int count = 100;
  if (server.hasArg("count")) {
    count = server.arg("count").toInt();
    if (count < 0) count = 100;
  }

  // 3) Logdatei öffnen
  File logFile = SPIFFS.open(LOG_FILENAME, FILE_READ);
  if (!logFile) {
    // Keine Datei = leeres Log
    server.send(200, "application/json", "[]");
    return;
  }

  // 4) Überspringe offset-Zeilen
  for (int i = 0; i < offset; i++) {
    if (!logFile.available()) {
      // Ende der Datei erreicht
      server.send(200, "application/json", "[]");
      logFile.close();
      return;
    }
    logFile.readStringUntil('\n');  // Liest bis zum Zeilenende
  }

  // 5) Jetzt bis zu count Zeilen einlesen
  StaticJsonDocument<8192> doc;
  // Achtung: je nachdem wie groß dein Log pro Zeile ist und wie viele du einliest,
  // muss das hier größer oder evtl. ein DynamicJsonDocument sein

  JsonArray array = doc.to<JsonArray>();

  for (int i = 0; i < count; i++) {
    if (!logFile.available()) break;  // Ende erreicht

    String line = logFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;  // leere Zeile überspringen

    // 6) Parsen
    StaticJsonDocument<256> lineDoc;
    DeserializationError err = deserializeJson(lineDoc, line);
    if (!err) {
      // Gültiges JSON, übernehmen ins Array
      array.add(lineDoc);
    } else {
      // Falls unparseable, könnte man es als "plain text" übernehmen oder ignorieren
      // array.add(line);
    }
  }
  logFile.close();

  // 7) Ergebnis serialisieren
  String response;
  serializeJson(array, response);
  server.send(200, "application/json", response);
}

void nuki_clearlog() {
  // Token prüfen
  if (!server.hasArg("token") || server.arg("token") != Apitoken->get()) {
    server.send(401, "text/html");
    return;
  }

  // Log löschen
  if (SPIFFS.exists(LOG_FILENAME)) {
    SPIFFS.remove(LOG_FILENAME);
  }

  // Keine oder nur Minimal-Antwort
  server.send(200, "text/plain", "");
}


void nuki_fwupdate() {
  server.send(501, "text/html");
}

void nuki_reboot() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    logToBridgeFile(F("System"), F("Reboot requested via REST"));
    server.send(200, "text/plain", "restart Bridge");
    delay(200);
    restartEsp(RestartReason::RequestedViaREST);
  } else {
    server.send(401, "text/html");
  }
}

void nuki_factoryReset() {
  if (server.hasArg("token") && server.arg("token") == Apitoken->get()) {
    server.send(200, "text/plain", "Perform factory reset");
    delay(100);
    preferences->clear();
    delay(1000);
    restartEsp(RestartReason::FactoryReset);
  } else {
    server.send(401, "text/html");
  }
}



void LANEvent(LANEvent_t event) {
  switch (event) {
#if CONNECT_OVER_LAN
    case ARDUINO_EVENT_ETH_START:
      Log->println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Log->println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Log->print("ETH MAC: ");
      Log->print(ETH.macAddress());
      Log->print(", IPv4: ");
      Log->print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Log->print(", FULL_DUPLEX");
      }
      Log->print(", ");
      Log->print(ETH.linkSpeed());
      Log->println("Mbps");
      LAN_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Log->println("ETH Disconnected");
      LAN_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Log->println("ETH Stopped");
      LAN_connected = false;
      break;
#else
    case ARDUINO_EVENT_WIFI_STA_START:
      Log->println("WiFi Started");
      //set WiFi hostname here
      Log->print("Hostname: ");
      Log->println(WiFi.getHostname());
      Log->print("Mode: ");
      Log->println(WiFi.getMode());
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Log->println(F("WiFi Connected"));
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      // print the SSID of the network you're attached to:
      Log->print(F("SSID: "));
      Log->println(WiFi.SSID());
      // print your WiFi shield's IP address:
      Log->print(F("IPv4 Address: "));
      Log->println(WiFi.localIP());
      // print the received signal strength:
      Log->print(F("signal strength (RSSI):"));
      Log->print(WiFi.RSSI());
      Log->println(F(" dBm"));
      LAN_connected = true;
      logToBridgeFile(F("Network"), "WiFi connected, IP: " + WiFi.localIP().toString());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Log->println(F("WiFi Disconnected"));
      LAN_connected = false;
      logToBridgeFile(F("Network"), F("WiFi disconnected"));
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
      Log->println(F("WiFi Stopped"));
      LAN_connected = false;
      break;
#endif
    default:
      break;
  }
}

bool initPreferences() {
  preferences = new Preferences();
  preferences->begin("nukiBridge", false);

  return true;
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Log = &Serial;
#else
  Log = new EmptyPrint();
#endif

  Log->print(F("NUKI REST Bridge version "));
  Log->println(NUKI_REST_BRIDGE_VERSION);

  initPreferences();

  initializeRestartReason();

  if (!SPIFFS.begin(true)) {
    Serial.println(F("SPIFFS Mount Failed!"));
    return;
  } else {

    logToBridgeFile(F("System"), F("NUKI REST Bridge gestartet"));
  }

  // load callback urls from preferences

  for (int i = 0; i < 3; i++) {
    char key[16];
    sprintf(key, "%s%d", preference_callback_key_prefix, i);
    callbackURLs[i] = preferences->getString(key, "");
  }

  // Setup Device / Sensor
  // ....
  //

  deviceIdLock = new NukiDeviceId(preferences, preference_device_id_lock);
  ApiEnabled = preferences->getBool(preference_API_enabled);
  Apitoken = new BridgeApiToken(preferences, preference_API_Token);

  nukiLock = new NukiLock::NukiLock("NukiBridge", deviceIdLock->get());
  scanner = new BleScanner::Scanner;

  restartOnDisconnect = preferences->getBool(preference_restart_on_disconnect);

  WiFi.onEvent(LANEvent);

#if CONNECT_OVER_LAN
  ETH.begin();
#if !DHCP
  ETH.config(localIP, gateway, subnet, primaryDNS, secondaryDNS);
#endif
#else
  // must placed here, because WIFI_STA_START event is only raised after wifi.begin
  //reset hostname
  WiFi.mode(WIFI_MODE_NULL);
  //set WiFi hostname here
  WiFi.setHostname("esp32-WiFi");
  WiFi.mode(WIFI_STA);
#if !DHCP
  WiFi.config(localIP, gateway, subnet, primaryDNS, secondaryDNS);
#endif
  WiFi.begin(SSID, PWD);
#endif

  delay(100);

  Log->println(F("Starting NUKI BLE..."));
  scanner->initialize("NukiBridge");
  scanner->setScanDuration(10);

  nukiLock->registerBleScanner(scanner);
  nukiLock->initialize();

  if (nukiLock->isPairedWithLock()) {
    Log->println("paired");
    nukiLock->setEventHandler(&handler);
    getConfig();
    getBatteryReport();
    getKeyTurnerState();
    nukiLock->enableLedFlash(false);
  }

  setupRouting();

  setupTasks();
}

void nukiTask(void *pvParameters) {
  while (true) {
    scanner->update();
    if (!nukiLock->isPairedWithLock()) {
      if (nukiLock->pairNuki() == Nuki::PairingResult::Success) {
        Log->println("paired");
        nukiLock->setEventHandler(&handler);
        getConfig();
        getBatteryReport();
        getKeyTurnerState();
      }
    }

    // Handling Nuki Events
    if (notified) {
      if (getKeyTurnerState()) {
        String stateStr = String("State changed to ") + String((int)retrievedKeyTurnerState.lockState);
        logToBridgeFile("SmartLock", stateStr);

        String jsonPayload = buildLockStateJson();
        for (int i = 0; i < 3; i++) {
          if (!callbackURLs[i].isEmpty()) {
            sendLockStateCallback(callbackURLs[i], jsonPayload);
          }
        }
        notified = false;
      }
    }
    esp_task_wdt_reset();
  }
}

void webServerTask(void *pvParameters) {
  uint64_t lastReconnectAttempt = 0;
  bool result;

  while (true) {
    // Prüfen, ob wir bereits verbunden sind
    if (!LAN_connected) {
      // Nur alle 5 Sekunden reconnecten, nicht jedes Loop
      if (espMillis() - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = espMillis();

        if (restartOnDisconnect && espMillis() > 60000) {
          restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }

        Log->println(F("Network not connected. Trying reconnect."));
#if CONNECT_OVER_LAN
        result = ETH.reconnect();
#else
        result = WiFi.reconnect();
#endif

        if (!result) {
          Log->println("Reconnect failed");
        } else {
          Log->println(F("Reconnect successful"));
        }
      }
    }

    server.handleClient();

    // millis() is about to overflow. Restart device to prevent problems with overflow
    if (espMillis() > restartTs) {
      Log->println(F("Restart timer expired, restarting device."));
      delay(200);
      restartEsp(RestartReason::RestartTimer);
    }
    esp_task_wdt_reset();
  }
}


void loop() {
  // deletes the Arduino loop task
  vTaskDelete(NULL);
}