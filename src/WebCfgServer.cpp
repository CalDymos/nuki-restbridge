#include "WebCfgServer.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <esp_task_wdt.h>
#include "FS.h"
#include "SPIFFS.h"
#include <esp_wifi.h>
#include <WiFi.h>

extern bool timeSynced;

#include <HTTPClient.h>
#include "ArduinoJson.h"

WebCfgServer::WebCfgServer(NukiWrapper* nuki, NukiNetwork* network, Preferences* preferences, bool allowRestartToPortal)
  : _nuki(nuki),
    _network(network),
    _preferences(preferences),
    _allowRestartToPortal(allowRestartToPortal) {
}