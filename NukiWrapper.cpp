#include "NukiWrapper.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "espMillis.h"
#include "RestartReason.h"

NukiWrapper::NukiWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, Preferences* preferences)
  : _deviceName(deviceName),
    _deviceId(deviceId),
    _bleScanner(scanner),
    _preferences(preferences),
    _nukiLock(deviceName, _deviceId->get()) {
  Log->print("Device id lock: ");
  Log->println(_deviceId->get());

  memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
  memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
  memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
  memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
  _keyTurnerState.lockState = NukiLock::LockState::Undefined;

  //ToDo:
  //network->setLockActionReceivedCallback(nukiInst->onLockActionReceivedCallback);
  //network->setConfigUpdateReceivedCallback(nukiInst->onConfigUpdateReceivedCallback);
  //network->setKeypadCommandReceivedCallback(nukiInst->onKeypadCommandReceivedCallback);
}

NukiWrapper::~NukiWrapper() {
  _bleScanner = nullptr;
}

void NukiWrapper::initialize(const bool& firstStart) {
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
  _rssiUpdateInterval = _preferences->getInt(preference_rssi_update_interval) * 1000;
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
        Log->print(" of ");
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
    Log->printf("cmd KeyTurnerState failed: %d", result);
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
  Log->print("Querying lock battery state: ");
  Nuki::CmdResult result = _nukiLock.requestBatteryReport(&_batteryReport);
  CommandResultToString(result, "requestBatteryReport result");
  if (result != Nuki::CmdResult::Success) {
    Log->printf("Battery report failed: %d", result);
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

    entries.sort([](const NukiLock::KeypadEntry& a, const NukiLock::KeypadEntry& b) {
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
    for (const auto& entry : entries) {
      _keypadCodeIds.push_back(entry.codeId);
    }
  }

  postponeBleWatchdog();
}

void NukiWrapper::postponeBleWatchdog() {
  _disableBleWatchdogTs = espMillis() + 15000;
}

NukiLock::LockAction NukiWrapper::lockActionToEnum(const char* str) {
  if (strcmp(str, "unlock") == 0) return NukiLock::LockAction::Unlock;
  else if (strcmp(str, "lock") == 0) return NukiLock::LockAction::Lock;
  else if (strcmp(str, "unlatch") == 0) return NukiLock::LockAction::Unlatch;
  else if (strcmp(str, "lockNgo") == 0) return NukiLock::LockAction::LockNgo;
  else if (strcmp(str, "lockNgoUnlatch") == 0) return NukiLock::LockAction::LockNgoUnlatch;
  else if (strcmp(str, "fullLock") == 0) return NukiLock::LockAction::FullLock;
  else if (strcmp(str, "fobAction2") == 0) return NukiLock::LockAction::FobAction2;
  else if (strcmp(str, "fobAction1") == 0) return NukiLock::LockAction::FobAction1;
  else if (strcmp(str, "fobAction3") == 0) return NukiLock::LockAction::FobAction3;
  return (NukiLock::LockAction)0xff;
}

const NukiLock::KeyTurnerState &NukiWrapper::keyTurnerState()
{
    return _keyTurnerState;
}

bool NukiWrapper::isPaired() const
{
    return _paired;
}

bool NukiWrapper::hasKeypad() const
{
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

std::string NukiWrapper::CommandResultToString(Nuki::CmdResult result, const char* logDescription) {
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