#include "NukiWrapper.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include "RestartReason.h"
#include "NukiPinState.h"
#include "espMillis.h"
#include "hal/wdt_hal.h"

NukiWrapper *nukiInst = nullptr;

NukiWrapper::NukiWrapper(const std::string &deviceName, NukiDeviceId *deviceId, BleScanner::Scanner *scanner, NukiNetworkLock *network, Preferences *preferences, char *buffer, size_t bufferSize)
    : _deviceName(deviceName),
      _deviceId(deviceId),
      _bleScanner(scanner),
      _nukiLock(deviceName, _deviceId->get()),
      _network(network),
      _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize)
{

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

NukiWrapper::~NukiWrapper()
{
  _bleScanner = nullptr;
}

void NukiWrapper::initialize()
{
  _nukiLock.setDebugConnect(_preferences->getBool(preference_debug_connect, false));
  _nukiLock.setDebugCommunication(_preferences->getBool(preference_debug_communication, false));
  _nukiLock.setDebugReadableData(_preferences->getBool(preference_debug_readable_data, false));
  _nukiLock.setDebugHexData(_preferences->getBool(preference_debug_hex_data, false));
  _nukiLock.setDebugCommand(_preferences->getBool(preference_debug_command, false));
  _nukiLock.registerLogger(Log);

  _nukiLock.initialize(_preferences->getBool(preference_connect_mode, true));
  _nukiLock.registerBleScanner(_bleScanner);
  _nukiLock.setEventHandler(this);
  _nukiLock.setConnectTimeout(3);
  _nukiLock.setDisconnectTimeout(2000);

  readSettings();
}

void NukiWrapper::readSettings()
{
  esp_power_level_t powerLevel;
  int pwrLvl = _preferences->getInt(preference_ble_tx_power, 9);

  if (pwrLvl >= 9)
  {
#if defined(CONFIG_IDF_TARGET_ESP32)
    powerLevel = ESP_PWR_LVL_P9;
#else
    if (pwrLvl >= 20)
    {
      powerLevel = ESP_PWR_LVL_P20;
    }
    else if (pwrLvl >= 18)
    {
      powerLevel = ESP_PWR_LVL_P18;
    }
    else if (pwrLvl >= 15)
    {
      powerLevel = ESP_PWR_LVL_P15;
    }
    else if (pwrLvl >= 12)
    {
      powerLevel = ESP_PWR_LVL_P12;
    }
    else
    {
      powerLevel = ESP_PWR_LVL_P9;
    }
#endif
  }
  else if (pwrLvl >= 6)
  {
    powerLevel = ESP_PWR_LVL_P6;
  }
  else if (pwrLvl >= 3)
  {
    powerLevel = ESP_PWR_LVL_P6;
  }
  else if (pwrLvl >= 0)
  {
    powerLevel = ESP_PWR_LVL_P3;
  }
  else if (pwrLvl >= -3)
  {
    powerLevel = ESP_PWR_LVL_N3;
  }
  else if (pwrLvl >= -6)
  {
    powerLevel = ESP_PWR_LVL_N6;
  }
  else if (pwrLvl >= -9)
  {
    powerLevel = ESP_PWR_LVL_N9;
  }
  else if (pwrLvl >= -12)
  {
    powerLevel = ESP_PWR_LVL_N12;
  }

  _nukiLock.setPower(powerLevel);

  _intervalLockstate = _preferences->getInt(preference_query_interval_lockstate);
  _intervalConfig = _preferences->getInt(preference_query_interval_configuration);
  _intervalBattery = _preferences->getInt(preference_query_interval_battery);
  _intervalKeypad = _preferences->getInt(preference_query_interval_keypad);
  _keypadEnabled = _preferences->getBool(preference_keypad_info_enabled);
  _publishAuthData = _preferences->getBool(preference_publish_authdata);
  _maxKeypadCodeCount = _preferences->getUInt(preference_lock_max_keypad_code_count);
  _maxTimeControlEntryCount = _preferences->getUInt(preference_lock_max_timecontrol_entry_count);
  _maxAuthEntryCount = _preferences->getUInt(preference_lock_max_auth_entry_count);
  _restartBeaconTimeout = _preferences->getInt(preference_restart_ble_beacon_lost);
  _nrOfRetries = _preferences->getInt(preference_command_nr_of_retries, 200);
  _retryDelay = _preferences->getInt(preference_command_retry_delay);
  _rssiPublishInterval = _preferences->getInt(preference_rssi_publish_interval) * 1000;
  _checkKeypadCodes = _preferences->getBool(preference_keypad_check_code_enabled, false);
  _forceKeypad = _preferences->getBool(preference_lock_force_keypad, false);
  _forceId = _preferences->getBool(preference_lock_force_id, false);
  _homeAutomationEnabled = _preferences->getBool(preference_ha_enabled);

  _preferences->getBytes(preference_conf_lock_basic_acl, &_basicLockConfigaclPrefs, sizeof(_basicLockConfigaclPrefs));
  _preferences->getBytes(preference_conf_lock_advanced_acl, &_advancedLockConfigaclPrefs, sizeof(_advancedLockConfigaclPrefs));

  if (_nrOfRetries < 0 || _nrOfRetries == 200)
  {
    Log->println("Invalid nrOfRetries, revert to default (3)");
    _nrOfRetries = 3;
    _preferences->putInt(preference_command_nr_of_retries, _nrOfRetries);
  }
  if (_retryDelay < 100)
  {
    Log->println("Invalid retryDelay, revert to default (100)");
    _retryDelay = 100;
    _preferences->putInt(preference_command_retry_delay, _retryDelay);
  }
  if (_intervalLockstate == 0)
  {
    Log->println("Invalid intervalLockstate, revert to default (1800)");
    _intervalLockstate = 60 * 30;
    _preferences->putInt(preference_query_interval_lockstate, _intervalLockstate);
  }
  if (_intervalConfig == 0)
  {
    Log->println("Invalid intervalConfig, revert to default (3600)");
    _intervalConfig = 60 * 60;
    _preferences->putInt(preference_query_interval_configuration, _intervalConfig);
  }
  if (_intervalBattery == 0)
  {
    Log->println("Invalid intervalBattery, revert to default (1800)");
    _intervalBattery = 60 * 30;
    _preferences->putInt(preference_query_interval_battery, _intervalBattery);
  }
  if (_intervalKeypad == 0)
  {
    Log->println("Invalid intervalKeypad, revert to default (1800)");
    _intervalKeypad = 60 * 30;
    _preferences->putInt(preference_query_interval_keypad, _intervalKeypad);
  }
  if (_restartBeaconTimeout != -1 && _restartBeaconTimeout < 10)
  {
    Log->println("Invalid restartBeaconTimeout, revert to default (-1)");
    _restartBeaconTimeout = -1;
    _preferences->putInt(preference_restart_ble_beacon_lost, _restartBeaconTimeout);
  }

  Log->print("Lock state interval: ");
  Log->print(_intervalLockstate);
  Log->print(" | Battery interval: ");
  Log->print(_intervalBattery);
  Log->print(" | Publish auth data: ");
  Log->println(_publishAuthData ? "yes" : "no");

  if (!_publishAuthData)
  {
    _clearAuthData = true;
  }
}

void NukiWrapper::update(bool reboot)
{
  wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
  wdt_hal_write_protect_disable(&rtc_wdt_ctx);
  wdt_hal_feed(&rtc_wdt_ctx);
  wdt_hal_write_protect_enable(&rtc_wdt_ctx);
  if (!_paired)
  {
    Log->println(F("Nuki lock start pairing"));

    Nuki::AuthorizationIdType idType = Nuki::AuthorizationIdType::Bridge;

    if (_nukiLock.pairNuki(idType) == Nuki::PairingResult::Success)
    {
      Log->println(F("Nuki paired"));
      _paired = true;
      _network->sendToHABleAddress(_nukiLock.getBleAddress().toString());
    }
    else
    {
      delay(200);
      return;
    }
  }

  int64_t lastReceivedBeaconTs = _nukiLock.getLastReceivedBeaconTs();
  int64_t ts = espMillis();
  uint8_t queryCommands = _network->queryCommands();

  if (_restartBeaconTimeout > 0 && ts > 60000 && lastReceivedBeaconTs > 0 && _disableBleWatchdogTs < ts && (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000))
  {
    Log->print("No BLE beacon received from the lock for ");
    Log->print((ts - lastReceivedBeaconTs) / 1000);
    Log->println(" seconds, restarting device.");
    delay(200);
    restartEsp(RestartReason::BLEBeaconWatchdog);
  }

  _nukiLock.updateConnectionState();

  if (_nextLockAction != (NukiLock::LockAction)0xff)
  {
    int retryCount = 0;
    Nuki::CmdResult cmdResult;

    while (retryCount < _nrOfRetries + 1 && cmdResult != Nuki::CmdResult::Success)
    {
      cmdResult = _nukiLock.lockAction(_nextLockAction, 0, 0);
      char resultStr[15] = {0};
      NukiLock::cmdResultToString(cmdResult, resultStr);

      _network->sendToHACommandResult(resultStr);

      Log->print("Lock action result: ");
      Log->println(resultStr);

      if (cmdResult != Nuki::CmdResult::Success)
      {
        Log->print("Lock: Last command failed, retrying after ");
        Log->print(_retryDelay);
        Log->print(" milliseconds. Retry ");
        Log->print(retryCount + 1);
        Log->print(" of ");
        Log->println(_nrOfRetries);

        _network->sendToHARetry(std::to_string(retryCount + 1));

        delay(_retryDelay);

        ++retryCount;
      }
      postponeBleWatchdog();
    }

    if (cmdResult == Nuki::CmdResult::Success)
    {
      _nextLockAction = (NukiLock::LockAction)0xff;
      retryCount = 0;

      Log->println("Lock: updating status after action");
      _statusUpdatedTs = ts;
      if (_intervalLockstate > 10)
      {
        _nextLockStateUpdateTs = ts + 10 * 1000;
      }
    }
    else
    {
      Log->println("Lock: Maximum number of retries exceeded, aborting.");
      _network->sendToHABleAddress("-1"); // failed
      retryCount = 0;
      _nextLockAction = (NukiLock::LockAction)0xff;
    }
  }
  if (_statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
  {
    Log->println("Updating Lock state based on status, timer or query");
    _statusUpdated = updateKeyTurnerState();
    _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
    _network->sendToHAStatusUpdated(_statusUpdated);
  }
  if (_network->NetworkServicesState() == 0)
  {
    if (!_statusUpdated)
    {
      if (_nextBatteryReportTs == 0 || ts > _nextBatteryReportTs || (queryCommands & QUERY_COMMAND_BATTERY) > 0)
      {
        Log->println("Updating Lock battery state based on timer or query");
        _nextBatteryReportTs = ts + _intervalBattery * 1000;
        updateBatteryState();
      }
      if (_nextConfigUpdateTs == 0 || ts > _nextConfigUpdateTs || (queryCommands & QUERY_COMMAND_CONFIG) > 0)
      {
        Log->println("Updating Lock config based on timer or query");
        _nextConfigUpdateTs = ts + _intervalConfig * 1000;
        updateConfig();
      }
      if (_waitAuthLogUpdateTs != 0 && ts > _waitAuthLogUpdateTs)
      {
        _waitAuthLogUpdateTs = 0;
        updateAuthData(true);
      }
      if (_waitKeypadUpdateTs != 0 && ts > _waitKeypadUpdateTs)
      {
        _waitKeypadUpdateTs = 0;
        updateKeypad(true);
      }
      if (_waitTimeControlUpdateTs != 0 && ts > _waitTimeControlUpdateTs)
      {
        _waitTimeControlUpdateTs = 0;
        updateTimeControl(true);
      }
      if (_waitAuthUpdateTs != 0 && ts > _waitAuthUpdateTs)
      {
        _waitAuthUpdateTs = 0;
        updateAuth(true);
      }
      if (_rssiPublishInterval > 0 && (_nextRssiTs == 0 || ts > _nextRssiTs))
      {
        _nextRssiTs = ts + _rssiPublishInterval;

        int rssi = _nukiLock.getRssi();
        if (rssi != _lastRssi)
        {
          _network->sendToHARssi(rssi);
          _lastRssi = rssi;
        }
      }

      if (_hasKeypad && _keypadEnabled && (_nextKeypadUpdateTs == 0 || ts > _nextKeypadUpdateTs || (queryCommands & QUERY_COMMAND_KEYPAD) > 0))
      {
        Log->println("Updating Lock keypad based on timer or query");
        _nextKeypadUpdateTs = ts + _intervalKeypad * 1000;
        updateKeypad(false);
      }
    }
    if (_clearAuthData)
    {
      Log->println("Clearing Lock auth data");
      _clearAuthData = false;
    }
    if (_checkKeypadCodes && _invalidCount > 0 && (ts - (120000 * _invalidCount)) > _lastCodeCheck)
    {
      _invalidCount--;
    }
    if (reboot && isPinValid())
    {
      Nuki::CmdResult cmdResult = _nukiLock.requestReboot();
    }
  }
  memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(NukiLock::KeyTurnerState));
}

bool NukiWrapper::updateKeyTurnerState()
{
  bool updateStatus = false;
  Nuki::CmdResult result = (Nuki::CmdResult)-1;
  int retryCount = 0;

  Log->print(F("Querying lock state: "));

  while (result != Nuki::CmdResult::Success && retryCount < _nrOfRetries + 1)
  {
    Log->print("Result (attempt ");
    Log->print(retryCount + 1);
    Log->print("): ");
    result = _nukiLock.requestKeyTurnerState(&_keyTurnerState);
    ++retryCount;
  }

  char resultStr[15];
  memset(&resultStr, 0, sizeof(resultStr));
  NukiLock::cmdResultToString(result, resultStr);
  _network->sendToHALockstateCommandResult(resultStr);

  if (result != Nuki::CmdResult::Success)
  {
    Log->println("Query lock state failed");
    _retryLockstateCount++;
    postponeBleWatchdog();
    if (_retryLockstateCount < _nrOfRetries + 1)
    {
      Log->print("Query lock state retrying in ");
      Log->print(_retryDelay);
      Log->println("ms");
      _nextLockStateUpdateTs = espMillis() + _retryDelay;
    }
    _network->sendToHAKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);
    return false;
  }
  _retryLockstateCount = 0;

  const NukiLock::LockState &lockState = _keyTurnerState.lockState;

  if (lockState != _lastKeyTurnerState.lockState)
  {
    _statusUpdatedTs = espMillis();
  }

  if (lockState == NukiLock::LockState::Locked ||
      lockState == NukiLock::LockState::Unlocked ||
      lockState == NukiLock::LockState::Calibration ||
      lockState == NukiLock::LockState::BootRun ||
      lockState == NukiLock::LockState::MotorBlocked)
  {
    if (_publishAuthData && (lockState == NukiLock::LockState::Locked || lockState == NukiLock::LockState::Unlocked))
    {
      Log->println("Publishing auth data");
      updateAuthData(false);
      Log->println("Done publishing auth data");
    }
  }
  else if (espMillis() < _statusUpdatedTs + 10000)
  {
    updateStatus = true;
    Log->println("Lock: Keep updating status on intermediate lock state");
  }
  else if (lockState == NukiLock::LockState::Undefined)
  {
    if (_nextLockStateUpdateTs > espMillis() + 60000)
    {
      _nextLockStateUpdateTs = espMillis() + 60000;
    }
  }
  _network->sendToHAKeyTurnerState(_keyTurnerState, _lastKeyTurnerState);

  char lockStateStr[20];
  lockstateToString(lockState, lockStateStr);
  Log->println(lockStateStr);

  postponeBleWatchdog();
  Log->println("Done querying lock state");
  return updateStatus;
}

void NukiWrapper::updateBatteryState()
{
  Nuki::CmdResult result = (Nuki::CmdResult)-1;
  int retryCount = 0;

  Log->println("Querying lock battery state");

  while (retryCount < _nrOfRetries + 1)
  {
    Log->print("Result (attempt ");
    Log->print(retryCount + 1);
    Log->print("): ");
    result = _nukiLock.requestBatteryReport(&_batteryReport);

    if (result != Nuki::CmdResult::Success)
    {
      ++retryCount;
    }
    else
    {
      break;
    }
  }

  printCmdResult(result);
  if (result == Nuki::CmdResult::Success)
  {
    _network->sendToHABatteryReport(_batteryReport);
  }
  postponeBleWatchdog();
  Log->println("Done querying lock battery state");
}

void NukiWrapper::updateConfig()
{
  bool expectedConfig = true;

  readConfig();

  if (_nukiConfigValid)
  {
    if (!_forceId && (_preferences->getUInt(preference_nuki_id_lock, 0) == 0 || _retryConfigCount == 10))
    {
      char uidString[20];
      itoa(_nukiConfig.nukiId, uidString, 16);
      Log->print("Saving Lock Nuki ID to preferences (");
      Log->print(_nukiConfig.nukiId);
      Log->print(" / ");
      Log->print(uidString);
      Log->println(")");
      _preferences->putUInt(preference_nuki_id_lock, _nukiConfig.nukiId);
    }

    if (_preferences->getUInt(preference_nuki_id_lock, 0) == _nukiConfig.nukiId)
    {
      _hasKeypad = _nukiConfig.hasKeypad == 1 || _nukiConfig.hasKeypadV2 == 1;
      _firmwareVersion = std::to_string(_nukiConfig.firmwareVersion[0]) + "." + std::to_string(_nukiConfig.firmwareVersion[1]) + "." + std::to_string(_nukiConfig.firmwareVersion[2]);
      _hardwareVersion = std::to_string(_nukiConfig.hardwareRevision[0]) + "." + std::to_string(_nukiConfig.hardwareRevision[1]);
      if (_preferences->getBool(preference_conf_info_enabled, true))
      {
        _network->sendToHAConfig(_nukiConfig);
      }
      if (_preferences->getBool(preference_timecontrol_info_enabled))
      {
        updateTimeControl(false);
      }
      if (_preferences->getBool(preference_auth_info_enabled))
      {
        updateAuth(false);
      }

      const int pinStatus = _preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured);

      Nuki::CmdResult result = (Nuki::CmdResult)-1;
      int retryCount = 0;

      while (retryCount < _nrOfRetries + 1)
      {
        result = _nukiLock.verifySecurityPin();
        if (result != Nuki::CmdResult::Success)
        {
          ++retryCount;
        }
        else
        {
          break;
        }
      }

      if (result != Nuki::CmdResult::Success)
      {
        Log->println("Nuki Lock PIN is invalid or not set");
        if (pinStatus != 2)
        {
          _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::Invalid);
        }
      }
      else
      {
        Log->println("Nuki Lock PIN is valid");
        if (pinStatus != 1)
        {
          _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::Valid);
        }
      }
    }
    else
    {
      Log->println("Invalid/Unexpected lock config received, ID does not matched saved ID");
      expectedConfig = false;
    }
  }
  else
  {
    Log->println("Invalid/Unexpected lock config received, Config is not valid");
    expectedConfig = false;
  }

  if (expectedConfig)
  {
    readAdvancedConfig();

    if (_nukiAdvancedConfigValid)
    {
      if (_preferences->getBool(preference_conf_info_enabled, true))
      {
        _network->sendToHAAdvancedConfig(_nukiAdvancedConfig);
      }
    }
    else
    {
      Log->println("Invalid/Unexpected lock advanced config received, Advanced config is not valid");
      expectedConfig = false;
    }
  }

  if (expectedConfig && _nukiConfigValid && _nukiAdvancedConfigValid)
  {
    _retryConfigCount = 0;
    Log->println("Done retrieving lock config and advanced config");
  }
  else
  {
    ++_retryConfigCount;
    Log->println("Invalid/Unexpected lock config and/or advanced config received, retrying in 10 seconds");
    int64_t ts = espMillis();
    _nextConfigUpdateTs = ts + 10000;
  }
}

void NukiWrapper::updateAuthData(bool retrieved)
{
  if(!isPinValid())
  {
      Log->println("No valid Nuki Lock PIN set");
      return;
  }

  if(!retrieved)
  {
      Nuki::CmdResult result = (Nuki::CmdResult)-1;
      int retryCount = 0;

      while(retryCount < _nrOfRetries + 1)
      {
          Log->print("Retrieve log entries: ");
          result = _nukiLock.retrieveLogEntries(0, _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 1, false);
          if(result != Nuki::CmdResult::Success)
          {
              ++retryCount;
          }
          else
          {
              break;
          }
      }

      printCmdResult(result);
      if(result == Nuki::CmdResult::Success)
      {
          _waitAuthLogUpdateTs = espMillis() + 5000;
          delay(100);

          std::list<NukiLock::LogEntry> log;
          _nukiLock.getLogEntries(&log);

          if(log.size() > _preferences->getInt(preference_authlog_max_entries, 3))
          {
              log.resize(_preferences->getInt(preference_authlog_max_entries, 3));
          }

          log.sort([](const NukiLock::LogEntry& a, const NukiLock::LogEntry& b)
          {
              return a.index < b.index;
          });

          if(log.size() > 0)
          {
              _network->sendToHAAuthorizationInfo(log, true);
          }
      }
  }
  else
  {
      std::list<NukiLock::LogEntry> log;
      _nukiLock.getLogEntries(&log);

      if(log.size() > _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG))
      {
          log.resize(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));
      }

      log.sort([](const NukiLock::LogEntry& a, const NukiLock::LogEntry& b)
      {
          return a.index < b.index;
      });

      Log->print("Log size: ");
      Log->println(log.size());

      if(log.size() > 0)
      {
          _network->sendToHAAuthorizationInfo(log, false);
      }
  }

  postponeBleWatchdog();
}

void NukiWrapper::updateKeypad(bool retrieved)
{
  Log->print(F("Querying lock keypad: "));
  Nuki::CmdResult result = _nukiLock.retrieveKeypadEntries(0, 0xffff);
  printCmdResult(result, "retrieveKeypadEntries result");
  if (result == Nuki::CmdResult::Success)
  {
    std::list<NukiLock::KeypadEntry> entries;
    _nukiLock.getKeypadEntries(&entries);

    entries.sort([](const NukiLock::KeypadEntry &a, const NukiLock::KeypadEntry &b)
                 { return a.codeId < b.codeId; });

    uint keypadCount = entries.size();
    if (keypadCount > _maxKeypadCodeCount)
    {
      _maxKeypadCodeCount = keypadCount;
      _preferences->putUInt(preference_lock_max_keypad_code_count, _maxKeypadCodeCount);
    }

    //_network->publishKeypad(entries, _maxKeypadCodeCount);

    _keypadCodeIds.clear();
    _keypadCodeIds.reserve(entries.size());
    for (const auto &entry : entries)
    {
      _keypadCodeIds.push_back(entry.codeId);
    }
  }

  postponeBleWatchdog();
}

void NukiWrapper::postponeBleWatchdog()
{
  _disableBleWatchdogTs = espMillis() + 15000;
}

NukiLock::LockAction NukiWrapper::lockActionToEnum(const char *str)
{
  if (strcmp(str, "unlock") == 0 || strcmp(str, "Unlock") == 0)
    return NukiLock::LockAction::Unlock;
  else if (strcmp(str, "lock") == 0 || strcmp(str, "Lock") == 0)
    return NukiLock::LockAction::Lock;
  else if (strcmp(str, "unlatch") == 0 || strcmp(str, "Unlatch") == 0)
    return NukiLock::LockAction::Unlatch;
  else if (strcmp(str, "lockNgo") == 0 || strcmp(str, "LockNgo") == 0)
    return NukiLock::LockAction::LockNgo;
  else if (strcmp(str, "lockNgoUnlatch") == 0 || strcmp(str, "LockNgoUnlatch") == 0)
    return NukiLock::LockAction::LockNgoUnlatch;
  else if (strcmp(str, "fullLock") == 0 || strcmp(str, "FullLock") == 0)
    return NukiLock::LockAction::FullLock;
  else if (strcmp(str, "fobAction2") == 0 || strcmp(str, "FobAction2") == 0)
    return NukiLock::LockAction::FobAction2;
  else if (strcmp(str, "fobAction1") == 0 || strcmp(str, "FobAction1") == 0)
    return NukiLock::LockAction::FobAction1;
  else if (strcmp(str, "fobAction3") == 0 || strcmp(str, "FobAction3") == 0)
    return NukiLock::LockAction::FobAction3;
  return (NukiLock::LockAction)0xff;
}

LockActionResult NukiWrapper::onLockActionReceivedCallback(const char *value)
{
  return nukiInst->onLockActionReceived(value);
}

void NukiWrapper::onConfigUpdateReceivedCallback(const char *value)
{
  nukiInst->onConfigUpdateReceived(value);
}

void NukiWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int &enabled)
{
  nukiInst->onKeypadCommandReceived(command, id, name, code, enabled);
}

void NukiWrapper::onTimeControlCommandReceivedCallback(const char *value)
{
  nukiInst->onTimeControlCommandReceived(value);
}

void NukiWrapper::onAuthCommandReceivedCallback(const char *value)
{
  nukiInst->onAuthCommandReceived(value);
}

LockActionResult NukiWrapper::onLockActionReceived(const char *value)
{
  NukiLock::LockAction action;

  if (value)
  {
    if (strlen(value) > 0)
    {
      action = nukiInst->lockActionToEnum(value);
      if ((int)action == 0xff)
      {
        return LockActionResult::UnknownAction;
      }
    }
    else
    {
      return LockActionResult::UnknownAction;
    }
  }
  else
  {
    return LockActionResult::UnknownAction;
  }

  uint32_t aclPrefs[17];
  _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

  if ((action == NukiLock::LockAction::Lock && (int)aclPrefs[0] == 1) || (action == NukiLock::LockAction::Unlock && (int)aclPrefs[1] == 1) || (action == NukiLock::LockAction::Unlatch && (int)aclPrefs[2] == 1) || (action == NukiLock::LockAction::LockNgo && (int)aclPrefs[3] == 1) || (action == NukiLock::LockAction::LockNgoUnlatch && (int)aclPrefs[4] == 1) || (action == NukiLock::LockAction::FullLock && (int)aclPrefs[5] == 1) || (action == NukiLock::LockAction::FobAction1 && (int)aclPrefs[6] == 1) || (action == NukiLock::LockAction::FobAction2 && (int)aclPrefs[7] == 1) || (action == NukiLock::LockAction::FobAction3 && (int)aclPrefs[8] == 1))
  {
    nukiInst->_nextLockAction = action;
    return LockActionResult::Success;
  }

  return LockActionResult::AccessDenied;
}

void NukiWrapper::onConfigUpdateReceived(const char *value)
{
  JsonDocument jsonResult;

  if (!_nukiConfigValid)
  {
    jsonResult[F("result")] = "configNotReady";
    _network->sendResponse(jsonResult, false, 412);
    return;
  }

  if (!isPinValid())
  {
    jsonResult["result"] = "noValidPinSet";
    _network->sendResponse(jsonResult, false, 412);
    return;
  }

  JsonDocument json;
  DeserializationError jsonError = deserializeJson(json, value);

  if (jsonError)
  {
    jsonResult["result"] = "invalidQuery";
    _network->sendResponse(jsonResult, false, 422);
    return;
  }

  Nuki::CmdResult cmdResult;
  const char *basicKeys[16] = {"name", "latitude", "longitude", "autoUnlatch", "pairingEnabled", "buttonEnabled", "ledEnabled", "ledBrightness", "timeZoneOffset", "dstMode", "fobAction1", "fobAction2", "fobAction3", "singleLock", "advertisingMode", "timeZone"};
  const char *advancedKeys[25] = {"unlockedPositionOffsetDegrees", "lockedPositionOffsetDegrees", "singleLockedPositionOffsetDegrees", "unlockedToLockedTransitionOffsetDegrees", "lockNgoTimeout", "singleButtonPressAction", "doubleButtonPressAction", "detachedCylinder", "batteryType", "automaticBatteryTypeDetection", "unlatchDuration", "autoLockTimeOut", "autoUnLockDisabled", "nightModeEnabled", "nightModeStartTime", "nightModeEndTime", "nightModeAutoLockEnabled", "nightModeAutoUnlockDisabled", "nightModeImmediateLockOnStart", "autoLockEnabled", "immediateAutoLockEnabled", "autoUpdateEnabled", "rebootNuki", "motorSpeed", "enableSlowSpeedDuringNightMode"};
  bool basicUpdated = false;
  bool advancedUpdated = false;

  for (int i = 0; i < 16; i++)
  {
    if (json[basicKeys[i]].is<JsonVariantConst>())
    {
      JsonVariantConst jsonKey = json[basicKeys[i]];
      char *jsonchar;

      if (jsonKey.is<float>())
      {
        itoa(jsonKey, jsonchar, 10);
      }
      else if (jsonKey.is<bool>())
      {
        if (jsonKey)
        {
          itoa(1, jsonchar, 10);
        }
        else
        {
          itoa(0, jsonchar, 10);
        }
      }
      else if (jsonKey.is<const char *>())
      {
        jsonchar = (char *)jsonKey.as<const char *>();
      }

      if (strlen(jsonchar) == 0)
      {
        jsonResult[basicKeys[i]] = "noValueSet";
        continue;
      }

      if ((int)_basicLockConfigaclPrefs[i] == 1)
      {
        cmdResult = Nuki::CmdResult::Error;
        int retryCount = 0;

        while (retryCount < _nrOfRetries + 1)
        {
          if (strcmp(basicKeys[i], "name") == 0)
          {
            if (strlen(jsonchar) <= 32)
            {
              if (strcmp((const char *)_nukiConfig.name, jsonchar) == 0)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setName(std::string(jsonchar));
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "valueTooLong";
            }
          }
          else if (strcmp(basicKeys[i], "latitude") == 0)
          {
            const float keyvalue = atof(jsonchar);

            if (keyvalue > 0)
            {
              if (_nukiConfig.latitude == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setLatitude(keyvalue);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "longitude") == 0)
          {
            const float keyvalue = atof(jsonchar);

            if (keyvalue > 0)
            {
              if (_nukiConfig.longitude == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setLongitude(keyvalue);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "autoUnlatch") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiConfig.autoUnlatch == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableAutoUnlatch((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "pairingEnabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiConfig.pairingEnabled == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enablePairing((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "buttonEnabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiConfig.buttonEnabled == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableButton((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "ledEnabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiConfig.ledEnabled == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableLedFlash((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "ledBrightness") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 0 && keyvalue <= 5)
            {
              if (_nukiConfig.ledBrightness == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setLedBrightness(keyvalue);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "timeZoneOffset") == 0)
          {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 0 && keyvalue <= 60)
            {
              if (_nukiConfig.timeZoneOffset == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setTimeZoneOffset(keyvalue);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "dstMode") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiConfig.dstMode == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableDst((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "fobAction1") == 0)
          {
            const uint8_t fobAct1 = nukiInst->fobActionToInt(jsonchar);

            if (fobAct1 != 99)
            {
              if (_nukiConfig.fobAction1 == fobAct1)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setFobAction(1, fobAct1);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "fobAction2") == 0)
          {
            const uint8_t fobAct2 = nukiInst->fobActionToInt(jsonchar);

            if (fobAct2 != 99)
            {
              if (_nukiConfig.fobAction2 == fobAct2)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setFobAction(2, fobAct2);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "fobAction3") == 0)
          {
            const uint8_t fobAct3 = nukiInst->fobActionToInt(jsonchar);

            if (fobAct3 != 99)
            {
              if (_nukiConfig.fobAction3 == fobAct3)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setFobAction(3, fobAct3);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "singleLock") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiConfig.singleLock == keyvalue)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableSingleLock((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "advertisingMode") == 0)
          {
            Nuki::AdvertisingMode advmode = nukiInst->advertisingModeToEnum(jsonchar);

            if ((int)advmode != 0xff)
            {
              if (_nukiConfig.advertisingMode == advmode)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setAdvertisingMode(advmode);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }
          else if (strcmp(basicKeys[i], "timeZone") == 0)
          {
            Nuki::TimeZoneId tzid = nukiInst->timeZoneToEnum(jsonchar);

            if ((int)tzid != 0xff)
            {
              if (_nukiConfig.timeZoneId == tzid)
              {
                jsonResult[basicKeys[i]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setTimeZoneId(tzid);
              }
            }
            else
            {
              jsonResult[basicKeys[i]] = "invalidValue";
            }
          }

          if (cmdResult != Nuki::CmdResult::Success)
          {
            ++retryCount;
          }
          else
          {
            break;
          }
        }

        if (cmdResult == Nuki::CmdResult::Success)
        {
          basicUpdated = true;
        }

        if (!jsonResult[basicKeys[i]])
        {
          char resultStr[15] = {0};
          NukiLock::cmdResultToString(cmdResult, resultStr);
          jsonResult[basicKeys[i]] = resultStr;
        }
      }
      else
      {
        jsonResult[basicKeys[i]] = "accessDenied";
      }
    }
  }

  for (int j = 0; j < 25; j++)
  {
    if (json[advancedKeys[j]].is<JsonVariantConst>())
    {
      JsonVariantConst jsonKey = json[advancedKeys[j]];
      char *jsonchar;

      if (jsonKey.is<float>())
      {
        itoa(jsonKey, jsonchar, 10);
      }
      else if (jsonKey.is<bool>())
      {
        if (jsonKey)
        {
          itoa(1, jsonchar, 10);
        }
        else
        {
          itoa(0, jsonchar, 10);
        }
      }
      else if (jsonKey.is<const char *>())
      {
        jsonchar = (char *)jsonKey.as<const char *>();
      }

      if (strlen(jsonchar) == 0)
      {
        jsonResult[advancedKeys[j]] = "noValueSet";
        continue;
      }

      if ((int)_advancedLockConfigaclPrefs[j] == 1)
      {
        cmdResult = Nuki::CmdResult::Error;
        int retryCount = 0;

        while (retryCount < _nrOfRetries + 1)
        {
          if (strcmp(advancedKeys[j], "unlockedPositionOffsetDegrees") == 0)
          {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= -90 && keyvalue <= 180)
            {
              if (_nukiAdvancedConfig.unlockedPositionOffsetDegrees == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setUnlockedPositionOffsetDegrees(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "lockedPositionOffsetDegrees") == 0)
          {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= -180 && keyvalue <= 90)
            {
              if (_nukiAdvancedConfig.lockedPositionOffsetDegrees == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setLockedPositionOffsetDegrees(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "singleLockedPositionOffsetDegrees") == 0)
          {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= -180 && keyvalue <= 180)
            {
              if (_nukiAdvancedConfig.singleLockedPositionOffsetDegrees == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setSingleLockedPositionOffsetDegrees(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "unlockedToLockedTransitionOffsetDegrees") == 0)
          {
            const int16_t keyvalue = atoi(jsonchar);

            if (keyvalue >= -180 && keyvalue <= 180)
            {
              if (_nukiAdvancedConfig.unlockedToLockedTransitionOffsetDegrees == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setUnlockedToLockedTransitionOffsetDegrees(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "lockNgoTimeout") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 5 && keyvalue <= 60)
            {
              if (_nukiAdvancedConfig.lockNgoTimeout == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setLockNgoTimeout(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "singleButtonPressAction") == 0)
          {
            NukiLock::ButtonPressAction sbpa = nukiInst->buttonPressActionToEnum(jsonchar);

            if ((int)sbpa != 0xff)
            {
              if (_nukiAdvancedConfig.singleButtonPressAction == sbpa)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setSingleButtonPressAction(sbpa);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "doubleButtonPressAction") == 0)
          {
            NukiLock::ButtonPressAction dbpa = nukiInst->buttonPressActionToEnum(jsonchar);

            if ((int)dbpa != 0xff)
            {
              if (_nukiAdvancedConfig.doubleButtonPressAction == dbpa)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setDoubleButtonPressAction(dbpa);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "detachedCylinder") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.detachedCylinder == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableDetachedCylinder((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "batteryType") == 0)
          {
            Nuki::BatteryType battype = nukiInst->batteryTypeToEnum(jsonchar);

            if ((int)battype != 0xff && !_isUltra)
            {
              if (_nukiAdvancedConfig.batteryType == battype)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setBatteryType(battype);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "automaticBatteryTypeDetection") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if ((keyvalue == 0 || keyvalue == 1) && !_isUltra)
            {
              if (_nukiAdvancedConfig.automaticBatteryTypeDetection == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableAutoBatteryTypeDetection((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "unlatchDuration") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 1 && keyvalue <= 30)
            {
              if (_nukiAdvancedConfig.unlatchDuration == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setUnlatchDuration(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "autoLockTimeOut") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue >= 30 && keyvalue <= 1800)
            {
              if (_nukiAdvancedConfig.autoLockTimeOut == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setAutoLockTimeOut(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "autoUnLockDisabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.autoUnLockDisabled == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.disableAutoUnlock((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "nightModeEnabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.nightModeEnabled == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableNightMode((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "nightModeStartTime") == 0)
          {
            String keystr = jsonchar;
            unsigned char keyvalue[2];
            keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
            keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
            if (keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
            {
              if (_nukiAdvancedConfig.nightModeStartTime == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setNightModeStartTime(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "nightModeEndTime") == 0)
          {
            String keystr = jsonchar;
            unsigned char keyvalue[2];
            keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
            keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
            if (keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
            {
              if (_nukiAdvancedConfig.nightModeEndTime == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setNightModeEndTime(keyvalue);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "nightModeAutoLockEnabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.nightModeAutoLockEnabled == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableNightModeAutoLock((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "nightModeAutoUnlockDisabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.nightModeAutoUnlockDisabled == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.disableNightModeAutoUnlock((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "nightModeImmediateLockOnStart") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.nightModeImmediateLockOnStart == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableNightModeImmediateLockOnStart((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "autoLockEnabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.autoLockEnabled == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableAutoLock((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "immediateAutoLockEnabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.immediateAutoLockEnabled == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableImmediateAutoLock((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "autoUpdateEnabled") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.autoUpdateEnabled == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableAutoUpdate((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "rebootNuki") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 1)
            {
              cmdResult = _nukiLock.requestReboot();
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "motorSpeed") == 0)
          {
            NukiLock::MotorSpeed motorSpeed = nukiInst->motorSpeedToEnum(jsonchar);

            if ((int)motorSpeed != 0xff)
            {
              if (_nukiAdvancedConfig.motorSpeed == motorSpeed)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.setMotorSpeed(motorSpeed);
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }
          else if (strcmp(advancedKeys[j], "enableSlowSpeedDuringNightMode") == 0)
          {
            const uint8_t keyvalue = atoi(jsonchar);

            if (keyvalue == 0 || keyvalue == 1)
            {
              if (_nukiAdvancedConfig.enableSlowSpeedDuringNightMode == keyvalue)
              {
                jsonResult[advancedKeys[j]] = "unchanged";
              }
              else
              {
                cmdResult = _nukiLock.enableSlowSpeedDuringNightMode((keyvalue > 0));
              }
            }
            else
            {
              jsonResult[advancedKeys[j]] = "invalidValue";
            }
          }

          if (cmdResult != Nuki::CmdResult::Success)
          {
            ++retryCount;
          }
          else
          {
            break;
          }
        }

        if (cmdResult == Nuki::CmdResult::Success)
        {
          advancedUpdated = true;
        }

        if (!jsonResult[advancedKeys[j]])
        {
          char resultStr[15] = {0};
          NukiLock::cmdResultToString(cmdResult, resultStr);
          jsonResult[advancedKeys[j]] = resultStr;
        }
      }
      else
      {
        jsonResult[advancedKeys[j]] = "accessDenied";
      }
    }
  }

  if (basicUpdated || advancedUpdated)
  {
    jsonResult["general"] = "success";
  }
  else
  {
    jsonResult["general"] = "noChange";
  }

  _nextConfigUpdateTs = espMillis() + 300;

  serializeJson(jsonResult, _buffer, _bufferSize);
  _network->publishConfigCommandResult(_buffer);

  return;
}

void NukiWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int &enabled)
{

  if (!_preferences->getBool(preference_keypad_control_enabled))
  {
    _network->publishKeypadCommandResult("KeypadControlDisabled");
    return;
  }

  if (!hasKeypad())
  {
    if (_nukiConfigValid)
    {
      _network->publishKeypadCommandResult("KeypadNotAvailable");
    }
    return;
  }
  if (!_keypadEnabled)
  {
    return;
  }

  bool idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), id) != _keypadCodeIds.end();
  int codeInt = code.toInt();
  bool codeValid = codeInt > 100000 && codeInt < 1000000 && (code.indexOf('0') == -1);
  Nuki::CmdResult result = (Nuki::CmdResult)-1;
  int retryCount = 0;

  while (retryCount < _nrOfRetries + 1)
  {
    if (strcmp(command, "add") == 0)
    {
      if (name == "")
      {
        _network->publishKeypadCommandResult("MissingParameterName");
        return;
      }
      if (codeInt == 0)
      {
        _network->publishKeypadCommandResult("MissingParameterCode");
        return;
      }
      if (!codeValid)
      {
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
    }
    else if (strcmp(command, "delete") == 0)
    {
      if (!idExists)
      {
        _network->publishKeypadCommandResult("UnknownId");
        return;
      }

      result = _nukiLock.deleteKeypadEntry(id);
      Log->print("Delete keypad code: ");
      Log->println((int)result);
      updateKeypad(false);
    }
    else if (strcmp(command, "update") == 0)
    {
      if (name == "")
      {
        _network->publishKeypadCommandResult("MissingParameterName");
        return;
      }
      if (codeInt == 0)
      {
        _network->publishKeypadCommandResult("MissingParameterCode");
        return;
      }
      if (!codeValid)
      {
        _network->publishKeypadCommandResult("CodeInvalid");
        return;
      }
      if (!idExists)
      {
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
    }
    else if (strcmp(command, "") == 0)
    {
      return;
    }
    else
    {
      _network->publishKeypadCommandResult("UnknownCommand");
      return;
    }

    if (result != Nuki::CmdResult::Success)
    {
      ++retryCount;
    }
    else
    {
      break;
    }
  }

  if ((int)result != -1)
  {
    char resultStr[15];
    memset(&resultStr, 0, sizeof(resultStr));
    NukiLock::cmdResultToString(result, resultStr);
    _network->publishKeypadCommandResult(resultStr);
  }
}

void NukiWrapper::onTimeControlCommandReceived(const char *value)
{
  if (!_nukiConfigValid)
  {
    _network->publishTimeControlCommandResult("configNotReady");
    return;
  }

  if (!isPinValid())
  {
    _network->publishTimeControlCommandResult("noValidPinSet");
    return;
  }

  if (!_preferences->getBool(preference_timecontrol_control_enabled))
  {
    _network->publishTimeControlCommandResult("timeControlControlDisabled");
    return;
  }

  JsonDocument json;
  DeserializationError jsonError = deserializeJson(json, value);

  if (jsonError)
  {
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

  if (json["enabled"].is<JsonVariant>())
  {
    enabled = json["enabled"].as<unsigned int>();
  }
  else
  {
    enabled = 2;
  }

  if (json["weekdays"].is<JsonVariant>())
  {
    weekdays = json["weekdays"].as<String>();
  }
  if (json["time"].is<JsonVariant>())
  {
    time = json["time"].as<String>();
  }
  if (json["lockAction"].is<JsonVariant>())
  {
    lockAction = json["lockAction"].as<String>();
  }

  if (lockAction.length() > 0)
  {
    timeControlLockAction = nukiInst->lockActionToEnum(lockAction.c_str());

    if ((int)timeControlLockAction == 0xff)
    {
      _network->publishTimeControlCommandResult("invalidLockAction");
      return;
    }
  }

  if (action)
  {
    bool idExists = false;

    if (entryId)
    {
      idExists = std::find(_timeControlIds.begin(), _timeControlIds.end(), entryId) != _timeControlIds.end();
    }

    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    while (retryCount < _nrOfRetries + 1)
    {
      if (strcmp(action, "delete") == 0)
      {
        if (idExists)
        {
          result = _nukiLock.removeTimeControlEntry(entryId);
          Log->print("Delete timecontrol: ");
          Log->println((int)result);
        }
        else
        {
          _network->publishTimeControlCommandResult("noExistingEntryIdSet");
          return;
        }
      }
      else if (strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
      {
        uint8_t timeHour;
        uint8_t timeMin;
        uint8_t weekdaysInt = 0;
        unsigned int timeAr[2];

        if (time.length() > 0)
        {
          if (time.length() == 5)
          {
            timeAr[0] = (uint8_t)time.substring(0, 2).toInt();
            timeAr[1] = (uint8_t)time.substring(3, 5).toInt();

            if (timeAr[0] < 0 || timeAr[0] > 23 || timeAr[1] < 0 || timeAr[1] > 59)
            {
              _network->publishTimeControlCommandResult("invalidTime");
              return;
            }
          }
          else
          {
            _network->publishTimeControlCommandResult("invalidTime");
            return;
          }
        }

        if (weekdays.indexOf("mon") >= 0)
        {
          weekdaysInt += 64;
        }
        if (weekdays.indexOf("tue") >= 0)
        {
          weekdaysInt += 32;
        }
        if (weekdays.indexOf("wed") >= 0)
        {
          weekdaysInt += 16;
        }
        if (weekdays.indexOf("thu") >= 0)
        {
          weekdaysInt += 8;
        }
        if (weekdays.indexOf("fri") >= 0)
        {
          weekdaysInt += 4;
        }
        if (weekdays.indexOf("sat") >= 0)
        {
          weekdaysInt += 2;
        }
        if (weekdays.indexOf("sun") >= 0)
        {
          weekdaysInt += 1;
        }

        if (strcmp(action, "add") == 0)
        {
          NukiLock::NewTimeControlEntry entry;
          memset(&entry, 0, sizeof(entry));
          entry.weekdays = weekdaysInt;

          if (time.length() > 0)
          {
            entry.timeHour = timeAr[0];
            entry.timeMin = timeAr[1];
          }

          entry.lockAction = timeControlLockAction;

          result = _nukiLock.addTimeControlEntry(entry);
          Log->print("Add timecontrol: ");
          Log->println((int)result);
        }
        else if (strcmp(action, "update") == 0)
        {
          if (!idExists)
          {
            _network->publishTimeControlCommandResult("noExistingEntryIdSet");
            return;
          }

          Nuki::CmdResult resultTc = _nukiLock.retrieveTimeControlEntries();
          bool foundExisting = false;

          if (resultTc == Nuki::CmdResult::Success)
          {
            delay(5000);
            std::list<NukiLock::TimeControlEntry> timeControlEntries;
            _nukiLock.getTimeControlEntries(&timeControlEntries);

            for (const auto &entry : timeControlEntries)
            {
              if (entryId != entry.entryId)
              {
                continue;
              }
              else
              {
                foundExisting = true;
              }

              if (enabled == 2)
              {
                enabled = entry.enabled;
              }
              if (weekdays.length() < 1)
              {
                weekdaysInt = entry.weekdays;
              }
              if (time.length() < 1)
              {
                time = "old";
                timeAr[0] = entry.timeHour;
                timeAr[1] = entry.timeMin;
              }
              if (lockAction.length() < 1)
              {
                timeControlLockAction = entry.lockAction;
              }
            }

            if (!foundExisting)
            {
              _network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
              return;
            }
          }
          else
          {
            _network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
            return;
          }

          NukiLock::TimeControlEntry entry;
          memset(&entry, 0, sizeof(entry));
          entry.entryId = entryId;
          entry.enabled = enabled;
          entry.weekdays = weekdaysInt;

          if (time.length() > 0)
          {
            entry.timeHour = timeAr[0];
            entry.timeMin = timeAr[1];
          }

          entry.lockAction = timeControlLockAction;

          result = _nukiLock.updateTimeControlEntry(entry);
          Log->print("Update timecontrol: ");
          Log->println((int)result);
        }
      }
      else
      {
        _network->publishTimeControlCommandResult("invalidAction");
        return;
      }

      if (result != Nuki::CmdResult::Success)
      {
        ++retryCount;
      }
      else
      {
        break;
      }
    }

    if ((int)result != -1)
    {
      char resultStr[15];
      memset(&resultStr, 0, sizeof(resultStr));
      NukiLock::cmdResultToString(result, resultStr);
      _network->publishTimeControlCommandResult(resultStr);
    }

    _nextConfigUpdateTs = espMillis() + 300;
  }
  else
  {
    _network->publishTimeControlCommandResult("noActionSet");
    return;
  }
}

void NukiWrapper::onAuthCommandReceived(const char *value)
{
  if (!_nukiConfigValid)
  {
    _network->publishAuthCommandResult("configNotReady");
    return;
  }

  if (!isPinValid())
  {
    _network->publishAuthCommandResult("noValidPinSet");
    return;
  }

  if (!_preferences->getBool(preference_auth_control_enabled))
  {
    _network->publishAuthCommandResult("keypadControlDisabled");
    return;
  }

  JsonDocument json;
  DeserializationError jsonError = deserializeJson(json, value);

  if (jsonError)
  {
    _network->publishAuthCommandResult("invalidJson");
    return;
  }

  char oldName[33];
  const char *action = json["action"].as<const char *>();
  uint32_t authId = json["authId"].as<unsigned int>();
  // uint8_t idType = json["idType"].as<unsigned int>();
  // unsigned char secretKeyK[32] = {0x00};
  uint8_t remoteAllowed;
  uint8_t enabled;
  uint8_t timeLimited;
  String name;
  // String sharedKey;
  String allowedFrom;
  String allowedUntil;
  String allowedWeekdays;
  String allowedFromTime;
  String allowedUntilTime;

  if (json["remoteAllowed"].is<JsonVariant>())
  {
    remoteAllowed = json["remoteAllowed"].as<unsigned int>();
  }
  else
  {
    remoteAllowed = 2;
  }

  if (json["enabled"].is<JsonVariant>())
  {
    enabled = json["enabled"].as<unsigned int>();
  }
  else
  {
    enabled = 2;
  }

  if (json["timeLimited"].is<JsonVariant>())
  {
    timeLimited = json["timeLimited"].as<unsigned int>();
  }
  else
  {
    timeLimited = 2;
  }

  if (json["name"].is<JsonVariant>())
  {
    name = json["name"].as<String>();
  }
  // if(json["sharedKey"].is<JsonVariant>()) sharedKey = json["sharedKey"].as<String>();
  if (json["allowedFrom"].is<JsonVariant>())
  {
    allowedFrom = json["allowedFrom"].as<String>();
  }
  if (json["allowedUntil"].is<JsonVariant>())
  {
    allowedUntil = json["allowedUntil"].as<String>();
  }
  if (json["allowedWeekdays"].is<JsonVariant>())
  {
    allowedWeekdays = json["allowedWeekdays"].as<String>();
  }
  if (json["allowedFromTime"].is<JsonVariant>())
  {
    allowedFromTime = json["allowedFromTime"].as<String>();
  }
  if (json["allowedUntilTime"].is<JsonVariant>())
  {
    allowedUntilTime = json["allowedUntilTime"].as<String>();
  }

  if (action)
  {
    bool idExists = false;

    if (authId)
    {
      idExists = std::find(_authIds.begin(), _authIds.end(), authId) != _authIds.end();
    }

    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    while (retryCount < _nrOfRetries)
    {
      if (strcmp(action, "delete") == 0)
      {
        if (idExists)
        {
          result = _nukiLock.deleteAuthorizationEntry(authId);
          delay(250);
          Log->print("Delete authorization: ");
          Log->println((int)result);
        }
        else
        {
          _network->publishAuthCommandResult("noExistingAuthIdSet");
          return;
        }
      }
      else if (strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
      {
        if (name.length() < 1)
        {
          if (strcmp(action, "update") != 0)
          {
            _network->publishAuthCommandResult("noNameSet");
            return;
          }
        }

        /*
        if(sharedKey.length() != 64)
        {
            if (strcmp(action, "update") != 0)
            {
                _network->publishAuthCommandResult("noSharedKeySet");
                return;
            }
        }
        else
        {
            for(int i=0; i<sharedKey.length();i+=2) secretKeyK[(i/2)] = std::stoi(sharedKey.substring(i, i+2).c_str(), nullptr, 16);
        }
        */

        unsigned int allowedFromAr[6];
        unsigned int allowedUntilAr[6];
        unsigned int allowedFromTimeAr[2];
        unsigned int allowedUntilTimeAr[2];
        uint8_t allowedWeekdaysInt = 0;

        if (timeLimited == 1)
        {
          if (allowedFrom.length() > 0)
          {
            if (allowedFrom.length() == 19)
            {
              allowedFromAr[0] = (uint16_t)allowedFrom.substring(0, 4).toInt();
              allowedFromAr[1] = (uint8_t)allowedFrom.substring(5, 7).toInt();
              allowedFromAr[2] = (uint8_t)allowedFrom.substring(8, 10).toInt();
              allowedFromAr[3] = (uint8_t)allowedFrom.substring(11, 13).toInt();
              allowedFromAr[4] = (uint8_t)allowedFrom.substring(14, 16).toInt();
              allowedFromAr[5] = (uint8_t)allowedFrom.substring(17, 19).toInt();

              if (allowedFromAr[0] < 2000 || allowedFromAr[0] > 3000 || allowedFromAr[1] < 1 || allowedFromAr[1] > 12 || allowedFromAr[2] < 1 || allowedFromAr[2] > 31 || allowedFromAr[3] < 0 || allowedFromAr[3] > 23 || allowedFromAr[4] < 0 || allowedFromAr[4] > 59 || allowedFromAr[5] < 0 || allowedFromAr[5] > 59)
              {
                _network->publishAuthCommandResult("invalidAllowedFrom");
                return;
              }
            }
            else
            {
              _network->publishAuthCommandResult("invalidAllowedFrom");
              return;
            }
          }

          if (allowedUntil.length() > 0)
          {
            if (allowedUntil.length() == 19)
            {
              allowedUntilAr[0] = (uint16_t)allowedUntil.substring(0, 4).toInt();
              allowedUntilAr[1] = (uint8_t)allowedUntil.substring(5, 7).toInt();
              allowedUntilAr[2] = (uint8_t)allowedUntil.substring(8, 10).toInt();
              allowedUntilAr[3] = (uint8_t)allowedUntil.substring(11, 13).toInt();
              allowedUntilAr[4] = (uint8_t)allowedUntil.substring(14, 16).toInt();
              allowedUntilAr[5] = (uint8_t)allowedUntil.substring(17, 19).toInt();

              if (allowedUntilAr[0] < 2000 || allowedUntilAr[0] > 3000 || allowedUntilAr[1] < 1 || allowedUntilAr[1] > 12 || allowedUntilAr[2] < 1 || allowedUntilAr[2] > 31 || allowedUntilAr[3] < 0 || allowedUntilAr[3] > 23 || allowedUntilAr[4] < 0 || allowedUntilAr[4] > 59 || allowedUntilAr[5] < 0 || allowedUntilAr[5] > 59)
              {
                _network->publishAuthCommandResult("invalidAllowedUntil");
                return;
              }
            }
            else
            {
              _network->publishAuthCommandResult("invalidAllowedUntil");
              return;
            }
          }

          if (allowedFromTime.length() > 0)
          {
            if (allowedFromTime.length() == 5)
            {
              allowedFromTimeAr[0] = (uint8_t)allowedFromTime.substring(0, 2).toInt();
              allowedFromTimeAr[1] = (uint8_t)allowedFromTime.substring(3, 5).toInt();

              if (allowedFromTimeAr[0] < 0 || allowedFromTimeAr[0] > 23 || allowedFromTimeAr[1] < 0 || allowedFromTimeAr[1] > 59)
              {
                _network->publishAuthCommandResult("invalidAllowedFromTime");
                return;
              }
            }
            else
            {
              _network->publishAuthCommandResult("invalidAllowedFromTime");
              return;
            }
          }

          if (allowedUntilTime.length() > 0)
          {
            if (allowedUntilTime.length() == 5)
            {
              allowedUntilTimeAr[0] = (uint8_t)allowedUntilTime.substring(0, 2).toInt();
              allowedUntilTimeAr[1] = (uint8_t)allowedUntilTime.substring(3, 5).toInt();

              if (allowedUntilTimeAr[0] < 0 || allowedUntilTimeAr[0] > 23 || allowedUntilTimeAr[1] < 0 || allowedUntilTimeAr[1] > 59)
              {
                _network->publishAuthCommandResult("invalidAllowedUntilTime");
                return;
              }
            }
            else
            {
              _network->publishAuthCommandResult("invalidAllowedUntilTime");
              return;
            }
          }

          if (allowedWeekdays.indexOf("mon") >= 0)
          {
            allowedWeekdaysInt += 64;
          }
          if (allowedWeekdays.indexOf("tue") >= 0)
          {
            allowedWeekdaysInt += 32;
          }
          if (allowedWeekdays.indexOf("wed") >= 0)
          {
            allowedWeekdaysInt += 16;
          }
          if (allowedWeekdays.indexOf("thu") >= 0)
          {
            allowedWeekdaysInt += 8;
          }
          if (allowedWeekdays.indexOf("fri") >= 0)
          {
            allowedWeekdaysInt += 4;
          }
          if (allowedWeekdays.indexOf("sat") >= 0)
          {
            allowedWeekdaysInt += 2;
          }
          if (allowedWeekdays.indexOf("sun") >= 0)
          {
            allowedWeekdaysInt += 1;
          }
        }

        if (strcmp(action, "add") == 0)
        {
          _network->publishAuthCommandResult("addActionNotSupported");
          return;

          NukiLock::NewAuthorizationEntry entry;
          memset(&entry, 0, sizeof(entry));
          size_t nameLen = name.length();
          memcpy(&entry.name, name.c_str(), nameLen > 32 ? 32 : nameLen);
          /*
          memcpy(&entry.sharedKey, secretKeyK, 32);

          if(idType != 1)
          {
              _network->publishAuthCommandResult("invalidIdType");
              return;
          }

          entry.idType = idType;
          */
          entry.remoteAllowed = remoteAllowed == 1 ? 1 : 0;
          entry.timeLimited = timeLimited == 1 ? 1 : 0;

          if (allowedFrom.length() > 0)
          {
            entry.allowedFromYear = allowedFromAr[0];
            entry.allowedFromMonth = allowedFromAr[1];
            entry.allowedFromDay = allowedFromAr[2];
            entry.allowedFromHour = allowedFromAr[3];
            entry.allowedFromMinute = allowedFromAr[4];
            entry.allowedFromSecond = allowedFromAr[5];
          }

          if (allowedUntil.length() > 0)
          {
            entry.allowedUntilYear = allowedUntilAr[0];
            entry.allowedUntilMonth = allowedUntilAr[1];
            entry.allowedUntilDay = allowedUntilAr[2];
            entry.allowedUntilHour = allowedUntilAr[3];
            entry.allowedUntilMinute = allowedUntilAr[4];
            entry.allowedUntilSecond = allowedUntilAr[5];
          }

          entry.allowedWeekdays = allowedWeekdaysInt;

          if (allowedFromTime.length() > 0)
          {
            entry.allowedFromTimeHour = allowedFromTimeAr[0];
            entry.allowedFromTimeMin = allowedFromTimeAr[1];
          }

          if (allowedUntilTime.length() > 0)
          {
            entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
            entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
          }

          result = _nukiLock.addAuthorizationEntry(entry);
          delay(250);
          Log->print("Add authorization: ");
          Log->println((int)result);
        }
        else if (strcmp(action, "update") == 0)
        {
          if (!authId)
          {
            _network->publishAuthCommandResult("noAuthIdSet");
            return;
          }

          if (!idExists)
          {
            _network->publishAuthCommandResult("noExistingAuthIdSet");
            return;
          }

          Nuki::CmdResult resultAuth = _nukiLock.retrieveAuthorizationEntries(0, _preferences->getInt(preference_auth_max_entries, MAX_AUTH));
          bool foundExisting = false;

          if (resultAuth == Nuki::CmdResult::Success)
          {
            delay(5000);
            std::list<NukiLock::AuthorizationEntry> entries;
            _nukiLock.getAuthorizationEntries(&entries);

            for (const auto &entry : entries)
            {
              if (authId != entry.authId)
              {
                continue;
              }
              else
              {
                foundExisting = true;
              }

              if (name.length() < 1)
              {
                memset(oldName, 0, sizeof(oldName));
                memcpy(oldName, entry.name, sizeof(entry.name));
              }
              if (remoteAllowed == 2)
              {
                remoteAllowed = entry.remoteAllowed;
              }
              if (enabled == 2)
              {
                enabled = entry.enabled;
              }
              if (timeLimited == 2)
              {
                timeLimited = entry.timeLimited;
              }
              if (allowedFrom.length() < 1)
              {
                allowedFrom = "old";
                allowedFromAr[0] = entry.allowedFromYear;
                allowedFromAr[1] = entry.allowedFromMonth;
                allowedFromAr[2] = entry.allowedFromDay;
                allowedFromAr[3] = entry.allowedFromHour;
                allowedFromAr[4] = entry.allowedFromMinute;
                allowedFromAr[5] = entry.allowedFromSecond;
              }
              if (allowedUntil.length() < 1)
              {
                allowedUntil = "old";
                allowedUntilAr[0] = entry.allowedUntilYear;
                allowedUntilAr[1] = entry.allowedUntilMonth;
                allowedUntilAr[2] = entry.allowedUntilDay;
                allowedUntilAr[3] = entry.allowedUntilHour;
                allowedUntilAr[4] = entry.allowedUntilMinute;
                allowedUntilAr[5] = entry.allowedUntilSecond;
              }
              if (allowedWeekdays.length() < 1)
              {
                allowedWeekdaysInt = entry.allowedWeekdays;
              }
              if (allowedFromTime.length() < 1)
              {
                allowedFromTime = "old";
                allowedFromTimeAr[0] = entry.allowedFromTimeHour;
                allowedFromTimeAr[1] = entry.allowedFromTimeMin;
              }

              if (allowedUntilTime.length() < 1)
              {
                allowedUntilTime = "old";
                allowedUntilTimeAr[0] = entry.allowedUntilTimeHour;
                allowedUntilTimeAr[1] = entry.allowedUntilTimeMin;
              }
            }

            if (!foundExisting)
            {
              _network->publishAuthCommandResult("failedToRetrieveExistingAuthorizationEntry");
              return;
            }
          }
          else
          {
            _network->publishAuthCommandResult("failedToRetrieveExistingAuthorizationEntry");
            return;
          }

          NukiLock::UpdatedAuthorizationEntry entry;

          memset(&entry, 0, sizeof(entry));
          entry.authId = authId;

          if (name.length() < 1)
          {
            size_t nameLen = strlen(oldName);
            memcpy(&entry.name, oldName, nameLen > 20 ? 20 : nameLen);
          }
          else
          {
            size_t nameLen = name.length();
            memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
          }
          entry.remoteAllowed = remoteAllowed;
          entry.enabled = enabled;
          entry.timeLimited = timeLimited;

          if (enabled == 1)
          {
            if (timeLimited == 1)
            {
              if (allowedFrom.length() > 0)
              {
                entry.allowedFromYear = allowedFromAr[0];
                entry.allowedFromMonth = allowedFromAr[1];
                entry.allowedFromDay = allowedFromAr[2];
                entry.allowedFromHour = allowedFromAr[3];
                entry.allowedFromMinute = allowedFromAr[4];
                entry.allowedFromSecond = allowedFromAr[5];
              }

              if (allowedUntil.length() > 0)
              {
                entry.allowedUntilYear = allowedUntilAr[0];
                entry.allowedUntilMonth = allowedUntilAr[1];
                entry.allowedUntilDay = allowedUntilAr[2];
                entry.allowedUntilHour = allowedUntilAr[3];
                entry.allowedUntilMinute = allowedUntilAr[4];
                entry.allowedUntilSecond = allowedUntilAr[5];
              }

              entry.allowedWeekdays = allowedWeekdaysInt;

              if (allowedFromTime.length() > 0)
              {
                entry.allowedFromTimeHour = allowedFromTimeAr[0];
                entry.allowedFromTimeMin = allowedFromTimeAr[1];
              }

              if (allowedUntilTime.length() > 0)
              {
                entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
              }
            }
          }

          result = _nukiLock.updateAuthorizationEntry(entry);
          delay(250);
          Log->print("Update authorization: ");
          Log->println((int)result);
        }
      }
      else
      {
        _network->publishAuthCommandResult("invalidAction");
        return;
      }

      if (result != Nuki::CmdResult::Success)
      {
        ++retryCount;
      }
      else
      {
        break;
      }
    }

    updateAuth(false);

    if ((int)result != -1)
    {
      char resultStr[15];
      memset(&resultStr, 0, sizeof(resultStr));
      NukiLock::cmdResultToString(result, resultStr);
      _network->publishAuthCommandResult(resultStr);
    }
  }
  else
  {
    _network->publishAuthCommandResult("noActionSet");
    return;
  }
}

const NukiLock::KeyTurnerState &NukiWrapper::keyTurnerState()
{
  return _keyTurnerState;
}

const NukiLock::Config &NukiWrapper::Config() const
{
  return _nukiConfig;
}

bool NukiWrapper::isPaired() const
{
  return _paired;
}

bool NukiWrapper::hasKeypad() const
{
  return _hasKeypad;
}

void NukiWrapper::notify(Nuki::EventType eventType)
{
  if (eventType == Nuki::EventType::KeyTurnerStatusUpdated)
  {
    _statusUpdated = true;
  }
}

void NukiWrapper::readConfig()
{
  Log->print(F("Reading config. Result: "));
  Nuki::CmdResult result = _nukiLock.requestConfig(&_nukiConfig);
  _nukiConfigValid = result == Nuki::CmdResult::Success;
  CommandResultToString(result, "requestConfig result");
}

void NukiWrapper::readAdvancedConfig()
{
  Log->print(F("Reading advanced config. Result: "));
  Nuki::CmdResult result = _nukiLock.requestAdvancedConfig(&_nukiAdvancedConfig);
  _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;
  CommandResultToString(result, "requestAdvancedConfig result");
}

const BLEAddress NukiWrapper::getBleAddress() const
{
  return _nukiLock.getBleAddress();
}

std::string NukiWrapper::CommandResultToString(Nuki::CmdResult result, const char *logDescription)
{
  char resultStr[20] = {0};
  NukiLock::cmdResultToString(result, resultStr);
  if (logDescription && *logDescription)
  {
    Log->print(logDescription);
    Log->print(": ");
    Log->println(resultStr);
  }

  return std::string(resultStr);
}

std::string NukiWrapper::firmwareVersion() const
{
  return _firmwareVersion;
}

std::string NukiWrapper::hardwareVersion() const
{
  return _hardwareVersion;
}

void NukiWrapper::disableWatchdog()
{
  _restartBeaconTimeout = -1;
}