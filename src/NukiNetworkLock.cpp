#include "NukiNetworkLock.h"
#include "Arduino.h"
#include "Config.h"
#include "RestApiPaths.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "ArduinoJson.h"

extern bool forceEnableWebServer;

NukiNetworkLock::NukiNetworkLock(NukiNetwork* network, Preferences* preferences, char* buffer, size_t bufferSize)
  : _network(network),
    _preferences(preferences),
    _buffer(buffer),
    _bufferSize(bufferSize) {

  _network->registerRestDataReceiver(this);
  _apiEnabled = _preferences->getBool(preference_api_enabled);
}

NukiNetworkLock::~NukiNetworkLock() {
}

void NukiNetworkLock::initialize() {
  strncpy(_apiLockPath, api_path_lock, sizeof(_apiLockPath) - 1);
}

bool NukiNetworkLock::update() {
  bool ret = false;

  return ret;
}

char* NukiNetworkLock::getArgs(WebServer& server) {
  JsonDocument doc;

  if (server.args() == 1 && server.argName(0) == "val") {
    // If only one parameter with the name "val" exists, save only the value
    strncpy(_buffer, server.arg(0).c_str(), _bufferSize - 1);
  } else if (server.args() == 1) {
    // If only one parameter exists, save only the name
    strncpy(_buffer, server.argName(0).c_str(), _bufferSize - 1);
  } else if (server.args() > 1) {
    for (uint8_t i = 0; i < server.args(); i++) {
      doc[server.argName(i)] = server.arg(i);  // Add parameters to the JSON document
    }
    // Serialization of the JSON to _buffer
    serializeJson(doc, _buffer, _bufferSize);
  } else {
    _buffer[0] = '\0';
  }

  return _buffer;  // Returns the global buffer as char*
}

void NukiNetworkLock::onRestDataReceived(const char* path, WebServer& server) {
  JsonDocument json;

  char* data = getArgs(server);

  if (!_apiEnabled) return;

  if (comparePrefixedPath(path, api_path_lock_action)) {

    if (!data || !*data) {
      json[F("result")] = "missing data";
      _network->sendResponse(json, false, 400);
    }
    return;

    Log->print(F("Lock action received: "));
    Log->println(data);

    LockActionResult lockActionResult = LockActionResult::Failed;
    if (_lockActionReceivedCallback != NULL) {
      lockActionResult = _lockActionReceivedCallback(data);
    }

    switch (lockActionResult) {
      case LockActionResult::Success:
        _network->sendResponse(json);
        break;
      case LockActionResult::UnknownAction:
        json[F("result")] = "unknown_action";
        _network->sendResponse(json, false, 404);
        break;
      case LockActionResult::AccessDenied:
        json[F("result")] = "denied";
        _network->sendResponse(json, false, 403);
        break;
      case LockActionResult::Failed:
        json[F("result")] = "error";
        _network->sendResponse(json, false, 500);
        break;
    }
    return;
  }

  if (comparePrefixedPath(path, api_path_keypad_command_action)) {
    if (_keypadCommandReceivedReceivedCallback != nullptr) {
      if (!data || !*data) {
        json[F("result")] = "missing data";
        _network->sendResponse(json, false, 400);
        return;
      }

      _keypadCommandReceivedReceivedCallback(data, _keypadCommandId, _keypadCommandName, _keypadCommandCode, _keypadCommandEnabled);

      _keypadCommandId = 0;
      _keypadCommandName = "";
      _keypadCommandCode = "000000";
      _keypadCommandEnabled = 1;

      return;
    }
  } else if (comparePrefixedPath(path, api_path_keypad_command_id)) {
    _keypadCommandId = atoi(data);
    return;
  } else if (comparePrefixedPath(path, api_path_keypad_command_name)) {
    _keypadCommandName = data;
    return;
  } else if (comparePrefixedPath(path, api_path_keypad_command_code)) {
    _keypadCommandCode = data;
    return;
  } else if (comparePrefixedPath(path, api_path_keypad_command_enabled)) {
    _keypadCommandEnabled = atoi(data);
    return;
  }

  bool queryCmdSet = false;
  if (strcmp(data, "1") == 0) {
    if (comparePrefixedPath(path, api_path_query_config)) {
      _queryCommands = _queryCommands | QUERY_COMMAND_CONFIG;
      queryCmdSet = true;
    } else if (comparePrefixedPath(path, api_path_query_lockstate)) {
      _queryCommands = _queryCommands | QUERY_COMMAND_LOCKSTATE;
      queryCmdSet = true;
    } else if (comparePrefixedPath(path, api_path_query_keypad)) {
      _queryCommands = _queryCommands | QUERY_COMMAND_KEYPAD;
      queryCmdSet = true;
    } else if (comparePrefixedPath(path, api_path_query_battery)) {
      _queryCommands = _queryCommands | QUERY_COMMAND_BATTERY;
      queryCmdSet = true;
    }
    if (queryCmdSet) {
      _network->sendResponse(json);
      return;
    }
  }

  if (comparePrefixedPath(path, api_path_config_action)) {

    if (!data || !*data) {
      json[F("result")] = "missing data";
      _network->sendResponse(json, false, 400);
      return;
    }

    if (_configUpdateReceivedCallback != NULL) {
      _configUpdateReceivedCallback(data);
    }
    return;
  }

  if (comparePrefixedPath(path, api_path_timecontrol_action)) {
    if (!data || !*data) {
      json[F("result")] = "missing data";
      _network->sendResponse(json, false, 400);
      return;
    }

    if (_timeControlCommandReceivedReceivedCallback != NULL) {
      _timeControlCommandReceivedReceivedCallback(data);
    }
    return;
  }

  if (comparePrefixedPath(path, api_path_auth_action)) {
    if (!data || !*data) {
      json[F("result")] = "missing data";
      _network->sendResponse(json, false, 400);
      return;
    }

    if (_authCommandReceivedReceivedCallback != NULL) {
      _authCommandReceivedReceivedCallback(data);
    }
    return;
  }
}

void NukiNetworkLock::setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char*)) {
  _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NukiNetworkLock::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char*)) {
  _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

bool NukiNetworkLock::comparePrefixedPath(const char* fullPath, const char* subPath) {
  char prefixedPath[385];
  buildApiPath(subPath, prefixedPath);
  return strcmp(fullPath, prefixedPath) == 0;
}

void NukiNetworkLock::buildApiPath(const char* path, char* outPath) {
  // Copy (_apiPath) to outPath
  strncpy(outPath, _apiLockPath, sizeof(_apiLockPath) - 1);

  // Append the (path) zo outPath
  strncat(outPath, path, 384 - strlen(outPath));  // Sicherheitsgrenze beachten
}

// void NukiNetworkLock::publishConfigCommandResult(const char* result) {
//   JsonDocument json;
//   deserializeJson(json, result);
//   json[F("result")] = result;
//   _network->sendResponse(json);
// }

// void NukiNetworkLock::publishKeypadCommandResult(const char* result) {
//   JsonDocument json;
//   deserializeJson(json, result);
//   json[F("result")] = result;
//   _network->sendResponse(json);
// }

// void NukiNetworkLock::publishTimeControlCommandResult(const char* result) {
//   JsonDocument json;
//   deserializeJson(json, result);
//   json[F("result")] = result;
//   _network->sendResponse(json);
// }

// void NukiNetworkLock::publishStatusUpdated(const bool statusUpdated) {
//   JsonDocument json;
//   deserializeJson(json, result);
//   json[F("result")] = statusUpdated ? 1 : 0;
//   _network->sendResponse(json);
// }