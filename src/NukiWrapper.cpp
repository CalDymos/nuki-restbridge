#include "NukiWrapper.h"
#include "PreferencesKeys.h"
#include "Logger.hpp"
#include "RestartReason.h"
#include "Config.h"
#include "NukiPinState.h"
#include "hal/wdt_hal.h"
#include <time.h>

NukiWrapper *nukiInst = nullptr;

NukiWrapper::NukiWrapper(const std::string &deviceName, NukiDeviceId *deviceId, BleScanner::Scanner *scanner, NukiNetwork *network, Preferences *preferences, char *buffer, size_t bufferSize)
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

    // KeyTurnerState und BatteryReport initialisieren
    memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
    _keyTurnerState.lockState = NukiLock::LockState::Undefined;

    network->setLockActionReceivedCallback(nukiInst->onLockActionReceivedCallback);
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

    _initialized = true;
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
    _forceDoorsensor = _preferences->getBool(preference_lock_force_doorsensor, false);
    _forceKeypad = _preferences->getBool(preference_lock_force_keypad, false);
    _forceId = _preferences->getBool(preference_lock_force_id, false);

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
    if (!_initialized)
        return;

    if (!_paired)
    {
        Log->println(F("Nuki lock start pairing"));

        Nuki::AuthorizationIdType idType = Nuki::AuthorizationIdType::Bridge;

        if (_nukiLock.pairNuki(idType) == Nuki::PairingResult::Success)
        {
            Log->println(F("Nuki paired"));
            _paired = true;
            _network->sendToHALockBleAddress(_nukiLock.getBleAddress().toString());
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

    if (_restartBeaconTimeout > 0 &&
        ts > 60000 &&
        lastReceivedBeaconTs > 0 &&
        _disableBleWatchdogTs < ts &&
        (ts - lastReceivedBeaconTs > _restartBeaconTimeout * 1000))
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

                delay(_retryDelay);

                ++retryCount;
            }
            postponeBleWatchdog();
        }

        if (cmdResult == Nuki::CmdResult::Success)
        {
            _nextLockAction = (NukiLock::LockAction)0xff;

            retryCount = 0;
            _statusUpdated = true;

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

            retryCount = 0;
            _nextLockAction = (NukiLock::LockAction)0xff;
        }
    }
    if (_statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
    {
        Log->println("Updating Lock state based on status, timer or query");
        _statusUpdated = updateKeyTurnerState();
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
    }
    // TODO:
}

void NukiWrapper::lock()
{
    if (!_initialized)
        return;

    _nextLockAction = NukiLock::LockAction::Lock;
}

void NukiWrapper::unlock()
{
    if (!_initialized)
        return;

    _nextLockAction = NukiLock::LockAction::Unlock;
}

void NukiWrapper::unlatch()
{
    if (!_initialized)
        return;

    _nextLockAction = NukiLock::LockAction::Unlatch;
}

void NukiWrapper::lockngo()
{
    if (!_initialized)
        return;

    _nextLockAction = NukiLock::LockAction::LockNgo;
}

void NukiWrapper::lockngounlatch()
{
    if (!_initialized)
        return;

    _nextLockAction = NukiLock::LockAction::LockNgoUnlatch;
}

void NukiWrapper::setPin(uint16_t pin)
{
    _nukiLock.saveSecurityPincode(pin);
}

bool NukiWrapper::isPinValid() const
{
    return _preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured) == (int)NukiPinState::Valid;
}

uint16_t NukiWrapper::getPin()
{
    return _nukiLock.getSecurityPincode();
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

        printCommandResult(result);
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
                //_network->sendToHAAuthorizationInfo(log, true); // TODO:
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
            //_network->sendToHAAuthorizationInfo(log, false); // TODO:
        }
    }

    postponeBleWatchdog();
}

bool NukiWrapper::updateKeyTurnerState()
{
    bool updateStatus = false;
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    Log->println("Querying lock state");

    while(result != Nuki::CmdResult::Success && retryCount < _nrOfRetries + 1)
    {
        Log->print("Result (attempt ");
        Log->print(retryCount + 1);
        Log->print("): ");
        result =_nukiLock.requestKeyTurnerState(&_keyTurnerState);
        ++retryCount;
    }

    char resultStr[15];
    memset(&resultStr, 0, sizeof(resultStr));
    NukiLock::cmdResultToString(result, resultStr);

    if(result != Nuki::CmdResult::Success)
    {
        Log->println("Query lock state failed");
        _retryLockstateCount++;
        postponeBleWatchdog();
        if(_retryLockstateCount < _nrOfRetries + 1)
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

    const NukiLock::LockState& lockState = _keyTurnerState.lockState;

    if(lockState != _lastKeyTurnerState.lockState)
    {
        _statusUpdatedTs = espMillis();
    }

    if(lockState == NukiLock::LockState::Locked ||
            lockState == NukiLock::LockState::Unlocked ||
            lockState == NukiLock::LockState::Calibration ||
            lockState == NukiLock::LockState::BootRun ||
            lockState == NukiLock::LockState::MotorBlocked)
    {
        if(_publishAuthData && (lockState == NukiLock::LockState::Locked || lockState == NukiLock::LockState::Unlocked))
        {
            Log->println("Publishing auth data");
            updateAuthData(false);
            Log->println("Done publishing auth data");
        }

    }
    else if(espMillis() < _statusUpdatedTs + 10000)
    {
        updateStatus = true;
        Log->println("Lock: Keep updating status on intermediate lock state");
    }
    else if(lockState == NukiLock::LockState::Undefined)
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

bool NukiWrapper::updateBatteryState()
{
    if (!_initialized)
        return false;

    Nuki::CmdResult result = _nukiLock.requestBatteryReport(&_batteryReport);
    return (result == Nuki::CmdResult::Success);
}

NukiLock::LockState NukiWrapper::getLockState() const
{
    return _keyTurnerState.lockState;
}

const NukiLock::KeyTurnerState &NukiWrapper::keyTurnerState()
{
    return _keyTurnerState;
}

const bool NukiWrapper::isPaired() const
{
    return _paired;
}

const BLEAddress NukiWrapper::getBleAddress() const
{
    return _nukiLock.getBleAddress();
}

String NukiWrapper::firmwareVersion() const
{
    return _firmwareVersion;
}

void NukiWrapper::printCommandResult(Nuki::CmdResult result)
{
    char resultStr[15];
    NukiLock::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}

String NukiWrapper::hardwareVersion() const
{
    return _hardwareVersion;
}

void NukiWrapper::disableWatchdog()
{
    _restartBeaconTimeout = -1;
}

NukiLock::LockAction NukiWrapper::lockActionToEnum(const char *str)
{
    if(strcmp(str, "unlock") == 0 || strcmp(str, "Unlock") == 0)
    {
        return NukiLock::LockAction::Unlock;
    }
    else if(strcmp(str, "lock") == 0 || strcmp(str, "Lock") == 0)
    {
        return NukiLock::LockAction::Lock;
    }
    else if(strcmp(str, "unlatch") == 0 || strcmp(str, "Unlatch") == 0)
    {
        return NukiLock::LockAction::Unlatch;
    }
    else if(strcmp(str, "lockNgo") == 0 || strcmp(str, "LockNgo") == 0)
    {
        return NukiLock::LockAction::LockNgo;
    }
    else if(strcmp(str, "lockNgoUnlatch") == 0 || strcmp(str, "LockNgoUnlatch") == 0)
    {
        return NukiLock::LockAction::LockNgoUnlatch;
    }
    else if(strcmp(str, "fullLock") == 0 || strcmp(str, "FullLock") == 0)
    {
        return NukiLock::LockAction::FullLock;
    }
    else if(strcmp(str, "fobAction2") == 0 || strcmp(str, "FobAction2") == 0)
    {
        return NukiLock::LockAction::FobAction2;
    }
    else if(strcmp(str, "fobAction1") == 0 || strcmp(str, "FobAction1") == 0)
    {
        return NukiLock::LockAction::FobAction1;
    }
    else if(strcmp(str, "fobAction3") == 0 || strcmp(str, "FobAction3") == 0)
    {
        return NukiLock::LockAction::FobAction3;
    }
    return (NukiLock::LockAction)0xff;
}

LockActionResult NukiWrapper::onLockActionReceivedCallback(const char *value)
{
    return nukiInst->onLockActionReceived(value);
}

LockActionResult NukiWrapper::onLockActionReceived(const char *value)
{
    NukiLock::LockAction action;

    if(value)
    {
        if(strlen(value) > 0)
        {
            action = nukiInst->lockActionToEnum(value);
            if((int)action == 0xff)
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

    if((action == NukiLock::LockAction::Lock && (int)aclPrefs[0] == 1) || (action == NukiLock::LockAction::Unlock && (int)aclPrefs[1] == 1) || (action == NukiLock::LockAction::Unlatch && (int)aclPrefs[2] == 1) || (action == NukiLock::LockAction::LockNgo && (int)aclPrefs[3] == 1) || (action == NukiLock::LockAction::LockNgoUnlatch && (int)aclPrefs[4] == 1) || (action == NukiLock::LockAction::FullLock && (int)aclPrefs[5] == 1) || (action == NukiLock::LockAction::FobAction1 && (int)aclPrefs[6] == 1) || (action == NukiLock::LockAction::FobAction2 && (int)aclPrefs[7] == 1) || (action == NukiLock::LockAction::FobAction3 && (int)aclPrefs[8] == 1))
    {
            nukiInst->_nextLockAction = action;

        return LockActionResult::Success;
    }

    return LockActionResult::AccessDenied;
}

void NukiWrapper::postponeBleWatchdog()
{
    _disableBleWatchdogTs = espMillis() + 15000;
}

void NukiNetwork::setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NukiWrapper::notify(Nuki::EventType eventType)
{
    if (eventType == Nuki::EventType::KeyTurnerStatusUpdated)
    {
        _statusUpdated = true;
    }
}
