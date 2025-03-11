#include "NukiWrapper.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "espMillis.h"
#include "RestartReason.h"

NukiWrapper *nukiInst = nullptr;

NukiWrapper::NukiWrapper(const std::string &deviceName, NukiDeviceId *deviceId, BleScanner::Scanner *scanner, NukiNetworkLock *network, Preferences *preferences, char *buffer, size_t bufferSize)
  : _deviceName(deviceName),
    _deviceId(deviceId),
    _bleScanner(scanner),
    _nukiLock(deviceName, _deviceId->get()),
    _network(network),
    _preferences(preferences),
    _buffer(buffer),
    _bufferSize(bufferSize) {

  Log->print("Device id lock: ");
  Log->println(_deviceId->get());

  nukiInst = this;

  memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
  memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
  memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
  memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
  _keyTurnerState.lockState = NukiLock::LockState::Undefined;

  network->setLockActionReceivedCallback(nukiInst->onLockActionReceivedCallback);
  network->setConfigUpdateReceivedCallback(nukiInst->onConfigUpdateReceivedCallback);
  network->setKeypadCommandReceivedCallback(nukiInst->onKeypadCommandReceivedCallback);
  network->setTimeControlCommandReceivedCallback(nukiInst->onTimeControlCommandReceivedCallback);
  network->setAuthCommandReceivedCallback(nukiInst->onAuthCommandReceivedCallback);
}

NukiWrapper::~NukiWrapper() {
  _bleScanner = nullptr;
}

void NukiWrapper::initialize() {
  _nukiLock.initialize();
  _nukiLock.registerBleScanner(_bleScanner);

  _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
  _intervalConfig = _preferences->getInt(preference_query_interval_battery);
  _intervalBattery = _preferences->getInt(preference_query_interval_battery);
  _intervalKeypad = _preferences->getInt(preference_query_interval_keypad);
  _keypadEnabled = _preferences->getBool(preference_keypad_control_enabled);
  _maxKeypadCodeCount = _preferences->getUInt(preference_lock_max_keypad_code_count);
  _restartBeaconTimeout = _preferences->getInt(preference_restart_ble_beacon_lost);
  _nrOfRetries = _preferences->getInt(preference_command_nr_of_retries);
  _retryDelay = _preferences->getInt(preference_command_retry_delay);
  _rssiUpdateInterval = _preferences->getInt(preference_rssi_publish_interval) * 1000;
  _accessLevel = (AccessLevel)_preferences->getInt(preference_access_level);

  if (firstStart) {
    _preferences->putInt(preference_command_nr_of_retries, 3);
    _preferences->putInt(preference_command_retry_delay, 1000);
    _preferences->putInt(preference_restart_ble_beacon_lost, 60);
  }

  if (_retryDelay <= 100) {
    _retryDelay = 100;
    _preferences->putInt(preference_command_retry_delay, _retryDelay);
  }

  if (_intervalLockstate == 0) {
    _intervalLockstate = 60 * 30;
    _preferences->putInt(preference_query_interval_lockstate, _intervalLockstate);
  }
  if (_intervalConfig == 0) {
    _intervalConfig = 60 * 60;
    _preferences->putInt(preference_query_interval_configuration, _intervalConfig);
  }
  if (_intervalBattery == 0) {
    _intervalBattery = 60 * 30;
    _preferences->putInt(preference_query_interval_battery, _intervalBattery);
  }
  if (_intervalKeypad == 0) {
    _intervalKeypad = 60 * 30;
    _preferences->putInt(preference_query_interval_keypad, _intervalKeypad);
  }

  if (_restartBeaconTimeout < 10) {
    _restartBeaconTimeout = -1;
    _preferences->putInt(preference_restart_ble_beacon_lost, _restartBeaconTimeout);
  }

  _nukiLock.setEventHandler(this);
}

void NukiWrapper::update(uint8_t queryCommands) {
  if (!_paired) {
    Log->println(F("Nuki lock start pairing"));
    // _network->publishBleAddress(""); // TODO:

    Nuki::AuthorizationIdType idType = Nuki::AuthorizationIdType::Bridge;

    if (_nukiLock.pairNuki(idType) == Nuki::PairingResult::Success) {
      Log->println(F("Nuki paired"));
      _paired = true;
      // _network->publishBleAddress(_nukiLock.getBleAddress().toString()); // TODO:
    } else {
      delay(200);
      return;
    }
  }

  int64_t lastReceivedBeaconTs = _nukiLock.getLastReceivedBeaconTs();
  int64_t ts = espMillis();
  // uint8_t queryCommands = _network->queryCommands(); // TODO:

  if (_restartBeaconTimeout > 0 && ts > 60000 && lastReceivedBeaconTs > 0 && _disableBleWatchdogTs < ts && (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000)) {
    Log->print("No BLE beacon received from the lock for ");
    Log->print((ts - lastReceivedBeaconTs) / 1000);
    Log->println(" seconds, restarting device.");
    delay(200);
    restartEsp(RestartReason::BLEBeaconWatchdog);
  }

  _nukiLock.updateConnectionState();

  if (_statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0) {
    _statusUpdated = false;
    _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
    updateKeyTurnerState();
  }
  if (_nextBatteryReportTs == 0 || ts > _nextBatteryReportTs || (queryCommands & QUERY_COMMAND_BATTERY) > 0) {
    _nextBatteryReportTs = ts + _intervalBattery * 1000;
    updateBatteryState();
  }
  if (_nextConfigUpdateTs == 0 || ts > _nextConfigUpdateTs || (queryCommands & QUERY_COMMAND_CONFIG) > 0) {
    _nextConfigUpdateTs = ts + _intervalConfig * 1000;
    updateConfig();
  }
  if (_rssiUpdateInterval > 0 && (_nextRssiTs == 0 || ts > _nextRssiTs)) {
    _nextRssiTs = ts + _rssiUpdateInterval;

    int rssi = _nukiLock.getRssi();
    if (rssi != _lastRssi) {
      //_network->publishRssi(rssi); // TODO:
      _lastRssi = rssi;
    }
  }

  if (_hasKeypad && _keypadEnabled && (_nextKeypadUpdateTs == 0 || ts > _nextKeypadUpdateTs || (queryCommands & QUERY_COMMAND_KEYPAD) > 0)) {
    _nextKeypadUpdateTs = ts + _intervalKeypad * 1000;
    updateKeypad();
  }

  if (_nextLockAction != (NukiLock::LockAction)0xff && ts > _nextRetryTs) {
    Nuki::CmdResult cmdResult = _nukiLock.lockAction(_nextLockAction, 0, 0);

    std::string resultStr = CommandResultToString(cmdResult, "lockAction result");

    //_network->publishCommandResult(resultStr); // TODO:

    if (cmdResult == Nuki::CmdResult::Success) {
      _retryCount = 0;
      _nextLockAction = (NukiLock::LockAction)0xff;

      if (_intervalLockstate > 10) {
        _nextLockStateUpdateTs = ts + 10 * 1000;
      }
    } else {
      if (_retryCount < _nrOfRetries) {
        Log->print(F("Lock: Last command failed, retrying after "));
        Log->print(_retryDelay);
        Log->print(F(" milliseconds. Retry "));
        Log->print(_retryCount + 1);
        Log->print(F(" of "));
        Log->println(_nrOfRetries);


        _nextRetryTs = espMillis() + _retryDelay;

        ++_retryCount;
      } else {
        Log->println(F("Lock: Maximum number of retries exceeded, aborting."));

        _retryCount = 0;
        _nextRetryTs = 0;
        _nextLockAction = (NukiLock::LockAction)0xff;
      }
    }
    postponeBleWatchdog();
  }

  if (_clearAuthData) {
    _clearAuthData = false;
  }

  memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(NukiLock::KeyTurnerState));
}

void NukiWrapper::updateKeyTurnerState() {
  Log->print(F("Querying lock state: "));
  Nuki::CmdResult result = _nukiLock.requestKeyTurnerState(&_keyTurnerState);

  std::string resultStr = CommandResultToString(result, "requestKeyTurnerState result");
  //_network->publishLockstateCommandResult(resultStr);

  if (result != Nuki::CmdResult::Success) {
    Log->printf(F("cmd KeyTurnerState failed: %d"), result);
    _retryLockstateCount++;
    postponeBleWatchdog();
    if (_retryLockstateCount < _nrOfRetries) {
      _nextLockStateUpdateTs = espMillis() + _retryDelay;
    }
    return;
  }
  _retryLockstateCount = 0;

  //_network->publishKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);

  char lockStateStr[20];
  lockstateToString(_keyTurnerState.lockState, lockStateStr);
  Log->println(lockStateStr);

  postponeBleWatchdog();
}

void NukiWrapper::updateBatteryState() {
  Log->print(F("Querying lock battery state: "));
  Nuki::CmdResult result = _nukiLock.requestBatteryReport(&_batteryReport);
  CommandResultToString(result, "requestBatteryReport result");
  if (result != Nuki::CmdResult::Success) {
    Log->printf(F("Battery report failed: %d"), result);
  } else {
    //_network->publishBatteryReport(_batteryReport);
  }
  postponeBleWatchdog();
}

void NukiWrapper::updateConfig() {
  readConfig();
  readAdvancedConfig();
  _configRead = true;
  _hasKeypad = _nukiConfig.hasKeypad > 0 || _nukiConfig.hasKeypadV2;
  if (_nukiConfigValid) {
    _firmwareVersion = std::to_string(_nukiConfig.firmwareVersion[0]) + "." + std::to_string(_nukiConfig.firmwareVersion[1]) + "." + std::to_string(_nukiConfig.firmwareVersion[2]);
    _hardwareVersion = std::to_string(_nukiConfig.hardwareRevision[0]) + "." + std::to_string(_nukiConfig.hardwareRevision[1]);
    //_network->publishConfig(_nukiConfig);
  }
  if (_nukiAdvancedConfigValid) {
    //_network->publishAdvancedConfig(_nukiAdvancedConfig);
  }
}

void NukiWrapper::updateAuthData() {
  Nuki::CmdResult result = _nukiLock.retrieveLogEntries(0, 0, 0, true);
  CommandResultToString(result, "retrieveLogEntries result");

  if (result != Nuki::CmdResult::Success) {
    return;
  }
  delay(100);

  uint16_t count = _nukiLock.getLogEntryCount();

  result = _nukiLock.retrieveLogEntries(0, count < 5 ? count : 5, 1, false);
  if (result != Nuki::CmdResult::Success) {
    return;
  }
  delay(1000);

  std::list<NukiLock::LogEntry> log;
  _nukiLock.getLogEntries(&log);

  if (log.size() > 0) {
    //_network->publishAuthorizationInfo(log);
  }
  postponeBleWatchdog();
}

void NukiWrapper::updateKeypad() {
  Log->print(F("Querying lock keypad: "));
  Nuki::CmdResult result = _nukiLock.retrieveKeypadEntries(0, 0xffff);
  CommandResultToString(result, "retrieveKeypadEntries result");
  if (result == Nuki::CmdResult::Success) {
    std::list<NukiLock::KeypadEntry> entries;
    _nukiLock.getKeypadEntries(&entries);

    entries.sort([](const NukiLock::KeypadEntry &a, const NukiLock::KeypadEntry &b) {
      return a.codeId < b.codeId;
    });

    uint keypadCount = entries.size();
    if (keypadCount > _maxKeypadCodeCount) {
      _maxKeypadCodeCount = keypadCount;
      _preferences->putUInt(preference_lock_max_keypad_code_count, _maxKeypadCodeCount);
    }

    //_network->publishKeypad(entries, _maxKeypadCodeCount);

    _keypadCodeIds.clear();
    _keypadCodeIds.reserve(entries.size());
    for (const auto &entry : entries) {
      _keypadCodeIds.push_back(entry.codeId);
    }
  }

  postponeBleWatchdog();
}

void NukiWrapper::postponeBleWatchdog() {
  _disableBleWatchdogTs = espMillis() + 15000;
}

NukiLock::LockAction NukiWrapper::lockActionToEnum(const char *str) {
  if (strcmp(str, "unlock") == 0 || strcmp(str, "Unlock") == 0) return NukiLock::LockAction::Unlock;
  else if (strcmp(str, "lock") == 0 || strcmp(str, "Lock") == 0) return NukiLock::LockAction::Lock;
  else if (strcmp(str, "unlatch") == 0 || strcmp(str, "Unlatch") == 0) return NukiLock::LockAction::Unlatch;
  else if (strcmp(str, "lockNgo") == 0 || strcmp(str, "LockNgo") == 0) return NukiLock::LockAction::LockNgo;
  else if (strcmp(str, "lockNgoUnlatch") == 0 || strcmp(str, "LockNgoUnlatch") == 0) return NukiLock::LockAction::LockNgoUnlatch;
  else if (strcmp(str, "fullLock") == 0 || strcmp(str, "FullLock") == 0) return NukiLock::LockAction::FullLock;
  else if (strcmp(str, "fobAction2") == 0 || strcmp(str, "FobAction2") == 0) return NukiLock::LockAction::FobAction2;
  else if (strcmp(str, "fobAction1") == 0 || strcmp(str, "FobAction1") == 0) return NukiLock::LockAction::FobAction1;
  else if (strcmp(str, "fobAction3") == 0 || strcmp(str, "FobAction3") == 0) return NukiLock::LockAction::FobAction3;
  return (NukiLock::LockAction)0xff;
}

LockActionResult NukiWrapper::onLockActionReceivedCallback(const char *value) {
  return nukiInst->onLockActionReceived(value);
}

void NukiWrapper::onConfigUpdateReceivedCallback(const char *value) {
  nukiInst->onConfigUpdateReceived(value);
}

void NukiWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int &enabled) {
  nukiInst->onKeypadCommandReceived(command, id, name, code, enabled);
}

void NukiWrapper::onTimeControlCommandReceivedCallback(const char *value) {
  nukiInst->onTimeControlCommandReceived(value);
}

LockActionResult NukiWrapper::onLockActionReceived(const char *value) {
  NukiLock::LockAction action;

  if (value) {
    if (strlen(value) > 0) {
      action = nukiInst->lockActionToEnum(value);
      if ((int)action == 0xff) {
        return LockActionResult::UnknownAction;
      }
    } else {
      return LockActionResult::UnknownAction;
    }
  } else {
    return LockActionResult::UnknownAction;
  }

  uint32_t aclPrefs[17];
  _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

  if ((action == NukiLock::LockAction::Lock && (int)aclPrefs[0] == 1) || (action == NukiLock::LockAction::Unlock && (int)aclPrefs[1] == 1) || (action == NukiLock::LockAction::Unlatch && (int)aclPrefs[2] == 1) || (action == NukiLock::LockAction::LockNgo && (int)aclPrefs[3] == 1) || (action == NukiLock::LockAction::LockNgoUnlatch && (int)aclPrefs[4] == 1) || (action == NukiLock::LockAction::FullLock && (int)aclPrefs[5] == 1) || (action == NukiLock::LockAction::FobAction1 && (int)aclPrefs[6] == 1) || (action == NukiLock::LockAction::FobAction2 && (int)aclPrefs[7] == 1) || (action == NukiLock::LockAction::FobAction3 && (int)aclPrefs[8] == 1)) {
    nukiInst->_nextLockAction = action;
    return LockActionResult::Success;
  }

  return LockActionResult::AccessDenied;
}

void NukiWrapper::onConfigUpdateReceived(const char *value) {
  JsonDocument jsonResult;


  if (!_nukiConfigValid) {
    jsonResult[F("result")] = "configNotReady";
    _network->sendResponse(jsonResult, false, 412);
    return;
  }

  if (!isPinValid()) {
    jsonResult["result"] = "noValidPinSet";
    _network->sendResponse(jsonResult, false, 412);
    return;
  }

  JsonDocument json;
  DeserializationError jsonError = deserializeJson(json, value);

  if (jsonError) {
    jsonResult["result"] = "invalidQuery";
    _network->sendResponse(jsonResult, false, 422);
    return;
  }

  Nuki::CmdResult cmdResult;
  const char *basicKeys[16] = { "name", "latitude", "longitude", "autoUnlatch", "pairingEnabled", "buttonEnabled", "ledEnabled", "ledBrightness", "timeZoneOffset", "dstMode", "fobAction1", "fobAction2", "fobAction3", "singleLock", "advertisingMode", "timeZone" };
  const char *advancedKeys[25] = { "unlockedPositionOffsetDegrees", "lockedPositionOffsetDegrees", "singleLockedPositionOffsetDegrees", "unlockedToLockedTransitionOffsetDegrees", "lockNgoTimeout", "singleButtonPressAction", "doubleButtonPressAction", "detachedCylinder", "batteryType", "automaticBatteryTypeDetection", "unlatchDuration", "autoLockTimeOut", "autoUnLockDisabled", "nightModeEnabled", "nightModeStartTime", "nightModeEndTime", "nightModeAutoLockEnabled", "nightModeAutoUnlockDisabled", "nightModeImmediateLockOnStart", "autoLockEnabled", "immediateAutoLockEnabled", "autoUpdateEnabled", "rebootNuki", "motorSpeed", "enableSlowSpeedDuringNightMode" };
  bool basicUpdated = false;
  bool advancedUpdated = false;

  for (int i = 0; i < 16; i++) {
    if (json[basicKeys[i]].is<JsonVariantConst>()) {
      JsonVariantConst jsonKey = json[basicKeys[i]];
      char *jsonchar;

      if (jsonKey.is<float>()) {
        itoa(jsonKey, jsonchar, 10);
      } else if (jsonKey.is<bool>()) {
        if (jsonKey) {
          itoa(1, jsonchar, 10);
        } else {
          itoa(0, jsonchar, 10);
        }
      } else if (jsonKey.is<const char *>()) {
        jsonchar = (char *)jsonKey.as<const char *>();
      }

      if (strlen(jsonchar) == 0) {
        jsonResult[basicKeys[i]] = "noValueSet";
        continue;
      }

      if ((int)_basicLockConfigaclPrefs[i] == 1) {
        cmdResult = Nuki::CmdResult::Error;
        int retryCount = 0;

        while (retryCount < _nrOfRetries + 1) {
          if (strcmp(basicKeys[i], "name") == 0) {
            if (strlen(jsonchar) <= 32) {
              if (strcmp((const char *)_nukiConfig.name, jsonchar) == 0) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setName(std::string(jsonchar));
              }
            } else {
              jsonResult[basicKeys[i]] = "valueTooLong";
            }
          } else if (strcmp(basicKeys[i], "latitude") == 0) {
            const float keyvalue = atof(jsonchar);

            if (keyvalue > 0) {
              if (_nukiConfig.latitude == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setLatitude(keyvalue);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "longitude") == 0) {
            const float keyvalue = atof(jsonchar);

            if (keyvalue > 0) {
              if (_nukiConfig.longitude == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setLongitude(keyvalue);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "autoUnlatch") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiConfig.autoUnlatch == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableAutoUnlatch((keyvalue > 0));
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "pairingEnabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiConfig.pairingEnabled == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enablePairing((keyvalue > 0));
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "buttonEnabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiConfig.buttonEnabled == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableButton((keyvalue > 0));
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "ledEnabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiConfig.ledEnabled == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableLedFlash((keyvalue > 0));
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "ledBrightness") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 0 && keyvalue <= 5) {
              if (_nukiConfig.ledBrightness == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setLedBrightness(keyvalue);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "timeZoneOffset") == 0) {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 0 && keyvalue <= 60) {
              if (_nukiConfig.timeZoneOffset == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setTimeZoneOffset(keyvalue);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "dstMode") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiConfig.dstMode == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableDst((keyvalue > 0));
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "fobAction1") == 0) {
            const uint8_t fobAct1 = nukiInst->fobActionToInt(jsonchar);

            if (fobAct1 != 99) {
              if (_nukiConfig.fobAction1 == fobAct1) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setFobAction(1, fobAct1);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "fobAction2") == 0) {
            const uint8_t fobAct2 = nukiInst->fobActionToInt(jsonchar);

            if (fobAct2 != 99) {
              if (_nukiConfig.fobAction2 == fobAct2) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setFobAction(2, fobAct2);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "fobAction3") == 0) {
            const uint8_t fobAct3 = nukiInst->fobActionToInt(jsonchar);

            if (fobAct3 != 99) {
              if (_nukiConfig.fobAction3 == fobAct3) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setFobAction(3, fobAct3);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "singleLock") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiConfig.singleLock == keyvalue) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableSingleLock((keyvalue > 0));
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "advertisingMode") == 0) {
            Nuki::AdvertisingMode advmode = nukiInst->advertisingModeToEnum(jsonchar);

            if ((int)advmode != 0xff) {
              if (_nukiConfig.advertisingMode == advmode) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setAdvertisingMode(advmode);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          } else if (strcmp(basicKeys[i], "timeZone") == 0) {
            Nuki::TimeZoneId tzid = nukiInst->timeZoneToEnum(jsonchar);

            if ((int)tzid != 0xff) {
              if (_nukiConfig.timeZoneId == tzid) {
                jsonResult[basicKeys[i]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setTimeZoneId(tzid);
              }
            } else {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }

          if (cmdResult != Nuki::CmdResult::Success) {
            ++retryCount;
          } else {
            break;
          }
        }

        if (cmdResult == Nuki::CmdResult::Success) {
          basicUpdated = true;
        }

        if (!jsonResult[basicKeys[i]]) {
          char resultStr[15] = { 0 };
          NukiLock::cmdResultToString(cmdResult, resultStr);
          jsonResult[basicKeys[i]] = resultStr;
        }
      } else {
        jsonResult[basicKeys[i]] = "accessDenied";
      }
    }
  }

  for (int j = 0; j < 25; j++) {
    if (json[advancedKeys[j]].is<JsonVariantConst>()) {
      JsonVariantConst jsonKey = json[advancedKeys[j]];
      char *jsonchar;

      if (jsonKey.is<float>()) {
        itoa(jsonKey, jsonchar, 10);
      } else if (jsonKey.is<bool>()) {
        if (jsonKey) {
          itoa(1, jsonchar, 10);
        } else {
          itoa(0, jsonchar, 10);
        }
      } else if (jsonKey.is<const char *>()) {
        jsonchar = (char *)jsonKey.as<const char *>();
      }

      if (strlen(jsonchar) == 0) {
        jsonResult[advancedKeys[j]] = "noValueSet";
        continue;
      }

      if ((int)_advancedLockConfigaclPrefs[j] == 1) {
        cmdResult = Nuki::CmdResult::Error;
        int retryCount = 0;

        while (retryCount < _nrOfRetries + 1) {
          if (strcmp(advancedKeys[j], "unlockedPositionOffsetDegrees") == 0) {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= -90 && keyvalue <= 180) {
              if (_nukiAdvancedConfig.unlockedPositionOffsetDegrees == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setUnlockedPositionOffsetDegrees(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "lockedPositionOffsetDegrees") == 0) {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= -180 && keyvalue <= 90) {
              if (_nukiAdvancedConfig.lockedPositionOffsetDegrees == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setLockedPositionOffsetDegrees(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "singleLockedPositionOffsetDegrees") == 0) {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= -180 && keyvalue <= 180) {
              if (_nukiAdvancedConfig.singleLockedPositionOffsetDegrees == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setSingleLockedPositionOffsetDegrees(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "unlockedToLockedTransitionOffsetDegrees") == 0) {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= -180 && keyvalue <= 180) {
              if (_nukiAdvancedConfig.unlockedToLockedTransitionOffsetDegrees == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setUnlockedToLockedTransitionOffsetDegrees(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "lockNgoTimeout") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 5 && keyvalue <= 60) {
              if (_nukiAdvancedConfig.lockNgoTimeout == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setLockNgoTimeout(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "singleButtonPressAction") == 0) {
            NukiLock::ButtonPressAction sbpa = nukiInst->buttonPressActionToEnum(jsonchar);

            if ((int)sbpa != 0xff) {
              if (_nukiAdvancedConfig.singleButtonPressAction == sbpa) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setSingleButtonPressAction(sbpa);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "doubleButtonPressAction") == 0) {
            NukiLock::ButtonPressAction dbpa = nukiInst->buttonPressActionToEnum(jsonchar);

            if ((int)dbpa != 0xff) {
              if (_nukiAdvancedConfig.doubleButtonPressAction == dbpa) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setDoubleButtonPressAction(dbpa);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "detachedCylinder") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.detachedCylinder == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableDetachedCylinder((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "batteryType") == 0) {
            Nuki::BatteryType battype = nukiInst->batteryTypeToEnum(jsonchar);

            if ((int)battype != 0xff && !_isUltra) {
              if (_nukiAdvancedConfig.batteryType == battype) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setBatteryType(battype);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "automaticBatteryTypeDetection") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if ((keyvalue == 0 || keyvalue == 1) && !_isUltra) {
              if (_nukiAdvancedConfig.automaticBatteryTypeDetection == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableAutoBatteryTypeDetection((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "unlatchDuration") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 1 && keyvalue <= 30) {
              if (_nukiAdvancedConfig.unlatchDuration == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setUnlatchDuration(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "autoLockTimeOut") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 30 && keyvalue <= 1800) {
              if (_nukiAdvancedConfig.autoLockTimeOut == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setAutoLockTimeOut(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "autoUnLockDisabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.autoUnLockDisabled == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.disableAutoUnlock((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "nightModeEnabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.nightModeEnabled == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableNightMode((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "nightModeStartTime") == 0) {
            String keystr = jsonchar;
            unsigned char keyvalue[2];
            keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
            keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
            if (keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59) {
              if (_nukiAdvancedConfig.nightModeStartTime == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setNightModeStartTime(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "nightModeEndTime") == 0) {
            String keystr = jsonchar;
            unsigned char keyvalue[2];
            keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
            keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
            if (keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59) {
              if (_nukiAdvancedConfig.nightModeEndTime == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setNightModeEndTime(keyvalue);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "nightModeAutoLockEnabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.nightModeAutoLockEnabled == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableNightModeAutoLock((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "nightModeAutoUnlockDisabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.nightModeAutoUnlockDisabled == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.disableNightModeAutoUnlock((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "nightModeImmediateLockOnStart") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.nightModeImmediateLockOnStart == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableNightModeImmediateLockOnStart((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "autoLockEnabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.autoLockEnabled == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableAutoLock((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "immediateAutoLockEnabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.immediateAutoLockEnabled == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableImmediateAutoLock((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "autoUpdateEnabled") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.autoUpdateEnabled == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableAutoUpdate((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "rebootNuki") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 1) {
              cmdResult = _nukiLock.requestReboot();
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "motorSpeed") == 0) {
            NukiLock::MotorSpeed motorSpeed = nukiInst->motorSpeedToEnum(jsonchar);

            if ((int)motorSpeed != 0xff) {
              if (_nukiAdvancedConfig.motorSpeed == motorSpeed) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.setMotorSpeed(motorSpeed);
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          } else if (strcmp(advancedKeys[j], "enableSlowSpeedDuringNightMode") == 0) {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1) {
              if (_nukiAdvancedConfig.enableSlowSpeedDuringNightMode == keyvalue) {
                jsonResult[advancedKeys[j]] = "unchanged";
              } else {
                cmdResult = _nukiLock.enableSlowSpeedDuringNightMode((keyvalue > 0));
              }
            } else {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }

          if (cmdResult != Nuki::CmdResult::Success) {
            ++retryCount;
          } else {
            break;
          }
        }

        if (cmdResult == Nuki::CmdResult::Success) {
          advancedUpdated = true;
        }

        if (!jsonResult[advancedKeys[j]]) {
          char resultStr[15] = { 0 };
          NukiLock::cmdResultToString(cmdResult, resultStr);
          jsonResult[advancedKeys[j]] = resultStr;
        }
      } else {
        jsonResult[advancedKeys[j]] = "accessDenied";
      }
    }
  }

  if (basicUpdated || advancedUpdated) {
    jsonResult["general"] = "success";
  } else {
    jsonResult["general"] = "noChange";
  }

  _nextConfigUpdateTs = espMillis() + 300;

  serializeJson(jsonResult, _buffer, _bufferSize);
  _network->publishConfigCommandResult(_buffer);

  return;
}

void NukiWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int &enabled) {

  if (!_preferences->getBool(preference_keypad_control_enabled)) {
    _network->publishKeypadCommandResult("KeypadControlDisabled");
    return;
  }

  if (!hasKeypad()) {
    if (_nukiConfigValid) {
      _network->publishKeypadCommandResult("KeypadNotAvailable");
    }
    return;
  }
  if (!_keypadEnabled) {
    return;
  }

  bool idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), id) != _keypadCodeIds.end();
  int codeInt = code.toInt();
  bool codeValid = codeInt > 100000 && codeInt < 1000000 && (code.indexOf('0') == -1);
  Nuki::CmdResult result = (Nuki::CmdResult)-1;
  int retryCount = 0;

  while (retryCount < _nrOfRetries + 1) {
    if (strcmp(command, "add") == 0) {
      if (name == "") {
        _network->publishKeypadCommandResult("MissingParameterName");
        return;
      }
      if (codeInt == 0) {
        _network->publishKeypadCommandResult("MissingParameterCode");
        return;
      }
      if (!codeValid) {
        _network->publishKeypadCommandResult("CodeInvalid");
        return;
      }

      NukiLock::NewKeypadEntry entry;
      memset(&entry, 0, sizeof(entry));
      size_t nameLen = name.length();
      memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
      entry.code = codeInt;
      result = _nukiLock.addKeypadEntry(entry);
      Log->print("Add keypad code: ");
      Log->println((int)result);
      updateKeypad(false);
    } else if (strcmp(command, "delete") == 0) {
      if (!idExists) {
        _network->publishKeypadCommandResult("UnknownId");
        return;
      }

      result = _nukiLock.deleteKeypadEntry(id);
      Log->print("Delete keypad code: ");
      Log->println((int)result);
      updateKeypad(false);
    } else if (strcmp(command, "update") == 0) {
      if (name == "") {
        _network->publishKeypadCommandResult("MissingParameterName");
        return;
      }
      if (codeInt == 0) {
        _network->publishKeypadCommandResult("MissingParameterCode");
        return;
      }
      if (!codeValid) {
        _network->publishKeypadCommandResult("CodeInvalid");
        return;
      }
      if (!idExists) {
        _network->publishKeypadCommandResult("UnknownId");
        return;
      }

      NukiLock::UpdatedKeypadEntry entry;
      memset(&entry, 0, sizeof(entry));
      entry.codeId = id;
      size_t nameLen = name.length();
      memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
      entry.code = codeInt;
      entry.enabled = enabled == 0 ? 0 : 1;
      result = _nukiLock.updateKeypadEntry(entry);
      Log->print("Update keypad code: ");
      Log->println((int)result);
      updateKeypad(false);
    } else if (strcmp(command, "") == 0) {
      return;
    } else {
      _network->publishKeypadCommandResult("UnknownCommand");
      return;
    }

    if (result != Nuki::CmdResult::Success) {
      ++retryCount;
    } else {
      break;
    }
  }

  if ((int)result != -1) {
    char resultStr[15];
    memset(&resultStr, 0, sizeof(resultStr));
    NukiLock::cmdResultToString(result, resultStr);
    _network->publishKeypadCommandResult(resultStr);
  }
}

void NukiWrapper::onTimeControlCommandReceived(const char *value) {
  if (!_nukiConfigValid) {
    _network->publishTimeControlCommandResult("configNotReady");
    return;
  }

  if (!isPinValid()) {
    _network->publishTimeControlCommandResult("noValidPinSet");
    return;
  }

  if (!_preferences->getBool(preference_timecontrol_control_enabled)) {
    _network->publishTimeControlCommandResult("timeControlControlDisabled");
    return;
  }

  JsonDocument json;
  DeserializationError jsonError = deserializeJson(json, value);

  if (jsonError) {
    _network->publishTimeControlCommandResult("invalidJson");
    return;
  }

  const char *action = json["action"].as<const char *>();
  uint8_t entryId = json["entryId"].as<unsigned int>();
  uint8_t enabled;
  String weekdays;
  String time;
  String lockAction;
  NukiLock::LockAction timeControlLockAction;

  if (json["enabled"].is<JsonVariant>()) {
    enabled = json["enabled"].as<unsigned int>();
  } else {
    enabled = 2;
  }

  if (json["weekdays"].is<JsonVariant>()) {
    weekdays = json["weekdays"].as<String>();
  }
  if (json["time"].is<JsonVariant>()) {
    time = json["time"].as<String>();
  }
  if (json["lockAction"].is<JsonVariant>()) {
    lockAction = json["lockAction"].as<String>();
  }

  if (lockAction.length() > 0) {
    timeControlLockAction = nukiInst->lockActionToEnum(lockAction.c_str());

    if ((int)timeControlLockAction == 0xff) {
      _network->publishTimeControlCommandResult("invalidLockAction");
      return;
    }
  }

  if (action) {
    bool idExists = false;

    if (entryId) {
      idExists = std::find(_timeControlIds.begin(), _timeControlIds.end(), entryId) != _timeControlIds.end();
    }

    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    while (retryCount < _nrOfRetries + 1) {
      if (strcmp(action, "delete") == 0) {
        if (idExists) {
          result = _nukiLock.removeTimeControlEntry(entryId);
          Log->print("Delete timecontrol: ");
          Log->println((int)result);
        } else {
          _network->publishTimeControlCommandResult("noExistingEntryIdSet");
          return;
        }
      } else if (strcmp(action, "add") == 0 || strcmp(action, "update") == 0) {
        uint8_t timeHour;
        uint8_t timeMin;
        uint8_t weekdaysInt = 0;
        unsigned int timeAr[2];

        if (time.length() > 0) {
          if (time.length() == 5) {
            timeAr[0] = (uint8_t)time.substring(0, 2).toInt();
            timeAr[1] = (uint8_t)time.substring(3, 5).toInt();

            if (timeAr[0] < 0 || timeAr[0] > 23 || timeAr[1] < 0 || timeAr[1] > 59) {
              _network->publishTimeControlCommandResult("invalidTime");
              return;
            }
          } else {
            _network->publishTimeControlCommandResult("invalidTime");
            return;
          }
        }

        if (weekdays.indexOf("mon") >= 0) {
          weekdaysInt += 64;
        }
        if (weekdays.indexOf("tue") >= 0) {
          weekdaysInt += 32;
        }
        if (weekdays.indexOf("wed") >= 0) {
          weekdaysInt += 16;
        }
        if (weekdays.indexOf("thu") >= 0) {
          weekdaysInt += 8;
        }
        if (weekdays.indexOf("fri") >= 0) {
          weekdaysInt += 4;
        }
        if (weekdays.indexOf("sat") >= 0) {
          weekdaysInt += 2;
        }
        if (weekdays.indexOf("sun") >= 0) {
          weekdaysInt += 1;
        }

        if (strcmp(action, "add") == 0) {
          NukiLock::NewTimeControlEntry entry;
          memset(&entry, 0, sizeof(entry));
          entry.weekdays = weekdaysInt;

          if (time.length() > 0) {
            entry.timeHour = timeAr[0];
            entry.timeMin = timeAr[1];
          }

          entry.lockAction = timeControlLockAction;

          result = _nukiLock.addTimeControlEntry(entry);
          Log->print("Add timecontrol: ");
          Log->println((int)result);
        } else if (strcmp(action, "update") == 0) {
          if (!idExists) {
            _network->publishTimeControlCommandResult("noExistingEntryIdSet");
            return;
          }

          Nuki::CmdResult resultTc = _nukiLock.retrieveTimeControlEntries();
          bool foundExisting = false;

          if (resultTc == Nuki::CmdResult::Success) {
            delay(5000);
            std::list<NukiLock::TimeControlEntry> timeControlEntries;
            _nukiLock.getTimeControlEntries(&timeControlEntries);

            for (const auto &entry : timeControlEntries) {
              if (entryId != entry.entryId) {
                continue;
              } else {
                foundExisting = true;
              }

              if (enabled == 2) {
                enabled = entry.enabled;
              }
              if (weekdays.length() < 1) {
                weekdaysInt = entry.weekdays;
              }
              if (time.length() < 1) {
                time = "old";
                timeAr[0] = entry.timeHour;
                timeAr[1] = entry.timeMin;
              }
              if (lockAction.length() < 1) {
                timeControlLockAction = entry.lockAction;
              }
            }

            if (!foundExisting) {
              _network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
              return;
            }
          } else {
            _network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
            return;
          }

          NukiLock::TimeControlEntry entry;
          memset(&entry, 0, sizeof(entry));
          entry.entryId = entryId;
          entry.enabled = enabled;
          entry.weekdays = weekdaysInt;

          if (time.length() > 0) {
            entry.timeHour = timeAr[0];
            entry.timeMin = timeAr[1];
          }

          entry.lockAction = timeControlLockAction;

          result = _nukiLock.updateTimeControlEntry(entry);
          Log->print("Update timecontrol: ");
          Log->println((int)result);
        }
      } else {
        _network->publishTimeControlCommandResult("invalidAction");
        return;
      }

      if (result != Nuki::CmdResult::Success) {
        ++retryCount;
      } else {
        break;
      }
    }

    if ((int)result != -1) {
      char resultStr[15];
      memset(&resultStr, 0, sizeof(resultStr));
      NukiLock::cmdResultToString(result, resultStr);
      _network->publishTimeControlCommandResult(resultStr);
    }

    _nextConfigUpdateTs = espMillis() + 300;
  } else {
    _network->publishTimeControlCommandResult("noActionSet");
    return;
  }
}

const NukiLock::KeyTurnerState &NukiWrapper::keyTurnerState() {
  return _keyTurnerState;
}

const NukiLock::Config &NukiWrapper::Config() const {
  return _nukiConfig;
}

bool NukiWrapper::isPaired() const {
  return _paired;
}

bool NukiWrapper::hasKeypad() const {
  return _hasKeypad;
}

void NukiWrapper::notify(Nuki::EventType eventType) {
  if (eventType == Nuki::EventType::KeyTurnerStatusUpdated) {
    _statusUpdated = true;
  }
}

void NukiWrapper::readConfig() {
  Log->print(F("Reading config. Result: "));
  Nuki::CmdResult result = _nukiLock.requestConfig(&_nukiConfig);
  _nukiConfigValid = result == Nuki::CmdResult::Success;
  CommandResultToString(result, "requestConfig result");
}

void NukiWrapper::readAdvancedConfig() {
  Log->print(F("Reading advanced config. Result: "));
  Nuki::CmdResult result = _nukiLock.requestAdvancedConfig(&_nukiAdvancedConfig);
  _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;
  CommandResultToString(result, "requestAdvancedConfig result");
}

const BLEAddress NukiWrapper::getBleAddress() const {
  return _nukiLock.getBleAddress();
}

std::string NukiWrapper::CommandResultToString(Nuki::CmdResult result, const char *logDescription) {
  char resultStr[20] = { 0 };
  NukiLock::cmdResultToString(result, resultStr);
  if (logDescription && *logDescription) {
    Log->print(logDescription);
    Log->print(": ");
    Log->println(resultStr);
  }

  return std::string(resultStr);
}

std::string NukiWrapper::firmwareVersion() const {
  return _firmwareVersion;
}

std::string NukiWrapper::hardwareVersion() const {
  return _hardwareVersion;
}

void NukiWrapper::disableWatchdog() {
  _restartBeaconTimeout = -1;
}