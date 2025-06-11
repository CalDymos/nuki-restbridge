#include "NukiWrapper.h"
#include "PreferencesKeys.h"
#include "Logger.h"
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
    Log->print(F("[DEBUG] Device id lock: "));
    Log->println(_deviceId->get());

    nukiInst = this;

    // KeyTurnerState und BatteryReport initialisieren
    memset(&_lastKeyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_lastBatteryReport, sizeof(NukiLock::BatteryReport), 0);
    memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
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

    _firmwareVersion.reserve(12);
    _hardwareVersion.reserve(8);

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
    _keypadInfoEnabled = _preferences->getBool(preference_keypad_info_enabled);
    _maxKeypadCodeCount = _preferences->getUInt(preference_lock_max_keypad_code_count);
    _keypadCodeEncryptionEnabled = _preferences->getBool(preference_keypad_code_encryption);
    _keypadCodeMultiplier = _preferences->getUInt(preference_keypad_code_multiplier, 73);
    _keypadCodeOffset = _preferences->getUInt(preference_keypad_code_offset, 12345);
    _keypadCodeModulus = _preferences->getUInt(preference_keypad_code_modulus, 1000000);
    _keypadCodeInverse = calcKeypadCodeInverse();
    _timeCtrlInfoEnabled = _preferences->getBool(preference_timecontrol_info_enabled);
    _maxTimeControlEntryCount = _preferences->getUInt(preference_lock_max_timecontrol_entry_count);
    _authInfoEnabled = _preferences->getBool(preference_auth_info_enabled);
    _maxAuthEntryCount = _preferences->getUInt(preference_lock_max_auth_entry_count);
    _restartBeaconTimeout = _preferences->getInt(preference_restart_ble_beacon_lost);
    _nrOfRetries = _preferences->getInt(preference_command_nr_of_retries, 200);
    _retryDelay = _preferences->getInt(preference_command_retry_delay);
    _rssiPublishInterval = _preferences->getInt(preference_rssi_send_interval) * 1000;
    _checkKeypadCodes = _preferences->getBool(preference_keypad_check_code_enabled, false);
    _forceDoorsensor = _preferences->getBool(preference_lock_force_doorsensor, false);
    _forceKeypad = _preferences->getBool(preference_lock_force_keypad, false);
    _forceId = _preferences->getBool(preference_lock_force_id, false);

    _preferences->getBytes(preference_conf_lock_basic_acl, &_basicLockConfigaclPrefs, sizeof(_basicLockConfigaclPrefs));
    _preferences->getBytes(preference_conf_lock_advanced_acl, &_advancedLockConfigaclPrefs, sizeof(_advancedLockConfigaclPrefs));

    if (_nrOfRetries < 0 || _nrOfRetries == 200)
    {
        Log->println(F("[WARNING] Invalid nrOfRetries, revert to default (3)"));
        _nrOfRetries = 3;
        _preferences->putInt(preference_command_nr_of_retries, _nrOfRetries);
    }
    if (_retryDelay < 100)
    {
        Log->println(F("[WARNING] Invalid retryDelay, revert to default (100)"));
        _retryDelay = 100;
        _preferences->putInt(preference_command_retry_delay, _retryDelay);
    }
    if (_intervalLockstate == 0)
    {
        Log->println(F("[WARNING] Invalid intervalLockstate, revert to default (1800)"));
        _intervalLockstate = 60 * 30;
        _preferences->putInt(preference_query_interval_lockstate, _intervalLockstate);
    }
    if (_intervalConfig == 0)
    {
        Log->println(F("[WARNING] Invalid intervalConfig, revert to default (3600)"));
        _intervalConfig = 60 * 60;
        _preferences->putInt(preference_query_interval_configuration, _intervalConfig);
    }
    if (_intervalBattery == 0)
    {
        Log->println(F("[WARNING] Invalid intervalBattery, revert to default (1800)"));
        _intervalBattery = 60 * 30;
        _preferences->putInt(preference_query_interval_battery, _intervalBattery);
    }
    if (_intervalKeypad == 0)
    {
        Log->println(F("WARNING] Invalid intervalKeypad, revert to default (1800)"));
        _intervalKeypad = 60 * 30;
        _preferences->putInt(preference_query_interval_keypad, _intervalKeypad);
    }
    if (_restartBeaconTimeout != -1 && _restartBeaconTimeout < 10)
    {
        Log->println(F("[WARNING] Invalid restartBeaconTimeout, revert to default (-1)"));
        _restartBeaconTimeout = -1;
        _preferences->putInt(preference_restart_ble_beacon_lost, _restartBeaconTimeout);
    }

    Log->print(F("[DEBUG] Lock state interval: "));
    Log->print(_intervalLockstate);
    Log->print(F(" | Battery interval: "));
    Log->println(_intervalBattery);
}

void NukiWrapper::update(bool reboot)
{
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);

    if (!_paired)
    {
        // first pairing log msg
        if (!_pairingMsgShown)
        {
            Log->println(F("[DEBUG] Nuki lock start pairing"));
            _pairingMsgShown = true;
            _lastPairingLogTs = espMillis();
        }

        // log pairing msg again every 60 seconds (only if pairing still fails)
        else if (espMillis() - _lastPairingLogTs > 60000)
        {
            Log->println(F("[DEBUG] Nuki lock start pairing (retrying...)"));
            _lastPairingLogTs = espMillis();
        }

        Nuki::AuthorizationIdType idType = Nuki::AuthorizationIdType::Bridge;

        if (_nukiLock.pairNuki(idType) == Nuki::PairingResult::Success)
        {
            Log->println(F("[INFO] Nuki paired"));
            _paired = true;
            _network->sendToHALockBleAddress(_nukiLock.getBleAddress().toString());
            _pairingMsgShown = false;
            _lastPairingLogTs = 0;
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
        Log->print(F("[ERROR] No BLE beacon received from the lock for "));
        Log->print((ts - lastReceivedBeaconTs) / 1000);
        Log->println(F(" seconds, restarting device."));
        Log->disableFileLog();
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

            Log->printf(F("[INFO] Lock action result: %s\r\n"), resultStr);

            if (cmdResult != Nuki::CmdResult::Success)
            {
                Log->print(F("[WARNING] Lock: Last command failed, retrying after "));
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

            Log->println(F("[DEBUG] Lock: updating status after action"));
            _statusUpdatedTs = ts;
            if (_intervalLockstate > 10)
            {
                _nextLockStateUpdateTs = ts + 10 * 1000;
            }
        }
        else
        {
            Log->println(F("[WARNING] Lock: Maximum number of retries exceeded, aborting."));

            retryCount = 0;
            _nextLockAction = (NukiLock::LockAction)0xff;
        }
    }
    if (_statusUpdated || _nextLockStateUpdateTs == 0 || ts >= _nextLockStateUpdateTs || (queryCommands & QUERY_COMMAND_LOCKSTATE) > 0)
    {
        Log->println(F("[DEBUG] Updating Lock state based on status, timer or query"));
        _statusUpdated = updateKeyTurnerState();
        _nextLockStateUpdateTs = ts + _intervalLockstate * 1000;
    }
    if (_network->networkServicesState() == NetworkServiceState::OK || _network->networkServicesState() == NetworkServiceState::ERROR_REST_API_SERVER)
    {
        if (!_statusUpdated)
        {
            if (_nextBatteryReportTs == 0 || ts > _nextBatteryReportTs || (queryCommands & QUERY_COMMAND_BATTERY) > 0)
            {
                Log->println(F("[DEBUG] Updating Lock battery state based on timer or query"));
                _nextBatteryReportTs = ts + _intervalBattery * 1000;
                updateBatteryState();
            }
            if (_nextConfigUpdateTs == 0 || ts > _nextConfigUpdateTs || (queryCommands & QUERY_COMMAND_CONFIG) > 0)
            {
                Log->println(F("[DEBUG] Updating Lock config based on timer or query"));
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
                    _network->sendToHABleRssi(rssi); // send BLE Rssi to HA
                    _lastRssi = rssi;
                }
            }
            if (hasKeypad() && _keypadInfoEnabled && (_nextKeypadUpdateTs == 0 || ts > _nextKeypadUpdateTs || (queryCommands & QUERY_COMMAND_KEYPAD) > 0))
            {
                Log->println("[DEBUG] Updating Lock keypad based on timer or query");
                _nextKeypadUpdateTs = ts + _intervalKeypad * 1000;
                updateKeypad(false);
            }
            if (_preferences->getBool(preference_update_time, false) && ts > (120 * 1000) && ts > _nextTimeUpdateTs)
            {
                _nextTimeUpdateTs = ts + (12 * 60 * 60 * 1000);
                updateTime();
            }
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

void NukiWrapper::lock()
{

    _nextLockAction = NukiLock::LockAction::Lock;
}

void NukiWrapper::unlock()
{

    _nextLockAction = NukiLock::LockAction::Unlock;
}

void NukiWrapper::unlatch()
{

    _nextLockAction = NukiLock::LockAction::Unlatch;
}

void NukiWrapper::lockngo()
{

    _nextLockAction = NukiLock::LockAction::LockNgo;
}

void NukiWrapper::lockngounlatch()
{

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

void NukiWrapper::unpair()
{
    _nukiLock.unPairNuki();
    Preferences nukiBlePref;
    nukiBlePref.begin("NukiBridge", false);
    nukiBlePref.clear();
    nukiBlePref.end();
    _deviceId->assignNewId();
    if (!_forceId)
    {
        _preferences->remove(preference_nuki_id_lock);
    }
    _paired = false;
}

void NukiWrapper::updateAuthData(bool retrieved)
{
    if (!isPinValid())
    {
        Log->println(F("[DEBUG] No valid Nuki Lock PIN set"));
        return;
    }

    if (!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while (retryCount < _nrOfRetries + 1)
        {
            Log->print(F("[TRACE] Retrieve log entries: "));
            result = _nukiLock.retrieveLogEntries(0, _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 1, false);
            if (result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        printCommandResult(result);
        if (result == Nuki::CmdResult::Success)
        {
            _waitAuthLogUpdateTs = espMillis() + 5000;
            delay(100);

            std::list<NukiLock::LogEntry> log;
            _nukiLock.getLogEntries(&log);

            if (log.size() > _preferences->getInt(preference_authlog_max_entries, 3))
            {
                log.resize(_preferences->getInt(preference_authlog_max_entries, 3));
            }

            log.sort([](const NukiLock::LogEntry &a, const NukiLock::LogEntry &b)
                     { return a.index < b.index; });

            if (log.size() > 0)
            {
                //_network->sendToHAAuthorizationInfo(log, true); // TODO:
            }
        }
    }
    else
    {
        std::list<NukiLock::LogEntry> log;
        _nukiLock.getLogEntries(&log);

        if (log.size() > _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG))
        {
            log.resize(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));
        }

        log.sort([](const NukiLock::LogEntry &a, const NukiLock::LogEntry &b)
                 { return a.index < b.index; });

        Log->print(F("[DEBUG] Log size: "));
        Log->println(log.size());

        if (log.size() > 0)
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

    Log->println(F("[TRACE] Querying lock state"));

    char resultStr[15];
    while (result != Nuki::CmdResult::Success && retryCount < _nrOfRetries + 1)
    {
        result = _nukiLock.requestKeyTurnerState(&_keyTurnerState);
        memset(&resultStr, 0, sizeof(resultStr));
        NukiLock::cmdResultToString(result, resultStr);
        Log->printf(F("[DEBUG] Result (attempt %d): %s"), retryCount + 1, resultStr);
        ++retryCount;
    }

    if (result != Nuki::CmdResult::Success)
    {
        Log->println(F("[WARNING] Query lock state failed"));
        _retryLockstateCount++;
        postponeBleWatchdog();
        if (_retryLockstateCount < _nrOfRetries + 1)
        {
            Log->print(F("[DEBUG] Query lock state retrying in "));
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
    }
    else if (espMillis() < _statusUpdatedTs + 10000)
    {
        updateStatus = true;
        Log->println("[DEBUG] Lock: Keep updating status on intermediate lock state");
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
    Log->print(F("[INFO] LockState: "));
    Log->println(lockStateStr);

    postponeBleWatchdog();
    Log->println("[TRACE] Done querying lock state");
    return updateStatus;
}

bool NukiWrapper::updateBatteryState()
{

    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    Log->println("[TRACE] Querying lock battery state");

    while (retryCount < _nrOfRetries + 1)
    {
        Log->print("[DEBUG] Result (attempt ");
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

    printCommandResult(result);
    if (result == Nuki::CmdResult::Success)
    {
        _network->sendToHABatteryReport(_batteryReport);
    }
    else
    {
        Log->println(F("[WARNING] Query battery state failed"));
    }
    postponeBleWatchdog();
    Log->println("[TRACE] Done querying lock battery state");
    return true;
}

bool NukiWrapper::updateConfig()
{
    bool expectedConfig = true;

    if (!readConfig())
    {
        return false;
    }

    if (_nukiConfigValid)
    {
        if (!_forceId && (_preferences->getUInt(preference_nuki_id_lock, 0) == 0 || _retryConfigCount == 10))
        {
            char uidString[20];
            itoa(_nukiConfig.nukiId, uidString, 16);
            Log->print("[DEBUG] Saving Lock Nuki ID to preferences (");
            Log->print(_nukiConfig.nukiId);
            Log->print(" / ");
            Log->print(uidString);
            Log->println(")");
            _preferences->putUInt(preference_nuki_id_lock, _nukiConfig.nukiId);
        }

        if (_preferences->getUInt(preference_nuki_id_lock, 0) == _nukiConfig.nukiId)
        {
            _hasKeypad = _nukiConfig.hasKeypad == 1 || _nukiConfig.hasKeypadV2 == 1;
            _firmwareVersion = String(_nukiConfig.firmwareVersion[0]) + "." + String(_nukiConfig.firmwareVersion[1]) + "." + String(_nukiConfig.firmwareVersion[2]);
            _hardwareVersion = String(_nukiConfig.hardwareRevision[0]) + "." + String(_nukiConfig.hardwareRevision[1]);

            if (_timeCtrlInfoEnabled)
            {
                updateTimeControl(false);
            }
            if (_authInfoEnabled)
            {
                updateAuth(false);
            }

            const int pinStatus = _preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured);

            Nuki::CmdResult result = (Nuki::CmdResult)-1;
            int retryCount = 0;

            // If the pin is 000000, it does not need to be checked.
            if (_nukiLock.getSecurityPincode() > 0 || _nukiLock.getUltraPincode() > 0)
            {
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
            }
            else
            {
                result = Nuki::CmdResult::Failed;
            }

            if (result != Nuki::CmdResult::Success)
            {
                Log->println(F("[INFO] Nuki Lock PIN is invalid or not set"));
                if (pinStatus != 2)
                {
                    _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::Invalid);
                }
            }
            else
            {
                Log->println(F("[INFO] Nuki Lock PIN is valid"));
                if (pinStatus != 1)
                {
                    _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::Valid);
                }
            }
        }
        else
        {
            Log->println(F("[WARNING] Invalid/Unexpected lock config received, ID does not matched saved ID"));
            expectedConfig = false;
        }
    }
    else
    {
        Log->println(F("[WARNING] Invalid/Unexpected lock config received, Config is not valid"));
        expectedConfig = false;
    }

    if (expectedConfig)
    {
        readAdvancedConfig();

        if (_nukiAdvancedConfigValid)
        {
        }
        else
        {
            Log->println(F("[WARNING] Invalid/Unexpected lock advanced config received, Advanced config is not valid"));
            expectedConfig = false;
        }
    }

    if (expectedConfig && _nukiConfigValid && _nukiAdvancedConfigValid)
    {
        _retryConfigCount = 0;
        Log->println(F("[DEBUG] Done retrieving lock config and advanced config"));
    }
    else
    {
        ++_retryConfigCount;
        Log->println(F("[WARNING] Invalid/Unexpected lock config and/or advanced config received, retrying in 10 seconds"));
        int64_t ts = espMillis();
        _nextConfigUpdateTs = ts + 10000;
    }
    return true;
}

void NukiWrapper::updateTimeControl(bool retrieved)
{
    if (!_timeCtrlInfoEnabled)
    {
        return;
    }

    if (!isPinValid())
    {
        Log->println(F("[DEBUG] No valid Nuki Lock PIN set"));
        return;
    }

    if (!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while (retryCount < _nrOfRetries + 1)
        {
            Log->print(F("[TRACE] Querying lock timecontrol: "));
            result = _nukiLock.retrieveTimeControlEntries();
            if (result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        printCommandResult(result);
        if (result == Nuki::CmdResult::Success)
        {
            _waitTimeControlUpdateTs = espMillis() + 5000;
        }
        else
        {
            Log->println(F("[WARNING] Query time control entries failed"));
        }
    }
    else
    {
        std::list<NukiLock::TimeControlEntry> timeControlEntries;
        _nukiLock.getTimeControlEntries(&timeControlEntries);

        Log->print(F("[INFO] Lock timecontrol entries: "));
        Log->println(timeControlEntries.size());

        timeControlEntries.sort([](const NukiLock::TimeControlEntry &a, const NukiLock::TimeControlEntry &b)
                                { return a.entryId < b.entryId; });

        if (timeControlEntries.size() > _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL))
        {
            timeControlEntries.resize(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL));
        }

        uint timeControlCount = timeControlEntries.size();
        if (timeControlCount > _maxTimeControlEntryCount)
        {
            _maxTimeControlEntryCount = timeControlCount;
            _preferences->putUInt(preference_lock_max_timecontrol_entry_count, _maxTimeControlEntryCount);
        }

        //_network->sendToHATimeControl(timeControlEntries, _maxTimeControlEntryCount);

        _timeControlIds.clear();
        _timeControlIds.reserve(timeControlEntries.size());
        for (const auto &entry : timeControlEntries)
        {
            _timeControlIds.push_back(entry.entryId);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::updateAuth(bool retrieved)
{
    if (!isPinValid())
    {
        Log->println(F("[DEBUG] No valid Nuki Lock PIN set"));
        return;
    }

    if (!_authInfoEnabled)
    {
        return;
    }

    if (!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while (retryCount < _nrOfRetries)
        {
            Log->println(F("[TRACE] Querying lock authorization: "));
            result = _nukiLock.retrieveAuthorizationEntries(0, _preferences->getInt(preference_auth_max_entries, MAX_AUTH));
            delay(250);
            if (result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        printCommandResult(result);
        if (result == Nuki::CmdResult::Success)
        {
            _waitAuthUpdateTs = millis() + 5000;
        }
        else
        {
            Log->println(F("[WARNING] Query authorization entries failed"));
        }
    }
    else
    {
        std::list<NukiLock::AuthorizationEntry> authEntries;
        _nukiLock.getAuthorizationEntries(&authEntries);

        Log->print(F("[INFO] Lock authorization entries: "));
        Log->println(authEntries.size());

        authEntries.sort([](const NukiLock::AuthorizationEntry &a, const NukiLock::AuthorizationEntry &b)
                         { return a.authId < b.authId; });

        if (authEntries.size() > _preferences->getInt(preference_auth_max_entries, MAX_AUTH))
        {
            authEntries.resize(_preferences->getInt(preference_auth_max_entries, MAX_AUTH));
        }

        uint authCount = authEntries.size();
        if (authCount > _maxAuthEntryCount)
        {
            _maxAuthEntryCount = authCount;
            _preferences->putUInt(preference_lock_max_auth_entry_count, _maxAuthEntryCount);
        }

        //_network->sendToHAAuth(authEntries, _maxAuthEntryCount);

        _authIds.clear();
        _authIds.reserve(authEntries.size());
        for (const auto &entry : authEntries)
        {
            _authIds.push_back(entry.authId);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::updateKeypad(bool retrieved)
{
    if (!_keypadInfoEnabled)
    {
        return;
    }

    if (!isPinValid())
    {
        Log->println(F("[DEBUG] No valid Nuki Lock PIN set"));
        return;
    }

    if (!retrieved)
    {
        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while (retryCount < _nrOfRetries + 1)
        {
            Log->println(F("[TRACE] Querying lock keypad: "));
            result = _nukiLock.retrieveKeypadEntries(0, _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
            if (result != Nuki::CmdResult::Success)
            {
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        printCommandResult(result);
        if (result == Nuki::CmdResult::Success)
        {
            _waitKeypadUpdateTs = espMillis() + 5000;
        }
        else
        {
            Log->println(F("[WARNING] Query keypad entries failed"));
        }
    }
    else
    {
        std::list<NukiLock::KeypadEntry> entries;
        _nukiLock.getKeypadEntries(&entries);

        Log->print(F("[INFO] Lock keypad codes: "));
        Log->println(entries.size());

        entries.sort([](const NukiLock::KeypadEntry &a, const NukiLock::KeypadEntry &b)
                     { return a.codeId < b.codeId; });

        if (entries.size() > _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD))
        {
            entries.resize(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
        }

        uint keypadCount = entries.size();
        if (keypadCount > _maxKeypadCodeCount)
        {
            _maxKeypadCodeCount = keypadCount;
            _preferences->putUInt(preference_lock_max_keypad_code_count, _maxKeypadCodeCount);
        }

        //_network->sendToHAKeypad(entries, _maxKeypadCodeCount);

        _keypadCodeIds.clear();
        _keypadCodes.clear();
        _keypadCodeIds.reserve(entries.size());
        _keypadCodes.reserve(entries.size());
        for (const auto &entry : entries)
        {
            _keypadCodeIds.push_back(entry.codeId);
            _keypadCodes.push_back(entry.code);
        }
    }

    postponeBleWatchdog();
}

void NukiWrapper::updateTime()
{
    if (!isPinValid())
    {
        Log->println(F("[DEBUG] No valid PIN set"));
        return;
    }

    time_t now;
    tm tm;
    time(&now);
    localtime_r(&now, &tm);

    if (int(tm.tm_year + 1900) < int(2025))
    {
        Log->println(F("[INFO] NTP Time not valid, not updating Nuki device"));
        return;
    }

    Nuki::TimeValue nukiTime;
    nukiTime.year = tm.tm_year + 1900;
    nukiTime.month = tm.tm_mon + 1;
    nukiTime.day = tm.tm_mday;
    nukiTime.hour = tm.tm_hour;
    nukiTime.minute = tm.tm_min;
    nukiTime.second = tm.tm_sec;

    Nuki::CmdResult cmdResult = _nukiLock.updateTime(nukiTime);

    char resultStr[15] = {0};
    NukiLock::cmdResultToString(cmdResult, resultStr);

    Log->print(F("[INFO] Lock time update result: "));
    Log->println(resultStr);
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

bool NukiWrapper::hasDoorSensor() const
{
    return (_forceDoorsensor ||
            _keyTurnerState.doorSensorState == Nuki::DoorSensorState::DoorClosed ||
            _keyTurnerState.doorSensorState == Nuki::DoorSensorState::DoorOpened ||
            _keyTurnerState.doorSensorState == Nuki::DoorSensorState::Calibrating);
}

const bool NukiWrapper::hasKeypad() const
{
    return (_forceKeypad || _hasKeypad);
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
    Log->printf(F("[DEBUG] Nuki::cmdResult = %s\n"), resultStr);
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
    if (strcmp(str, "unlock") == 0 || strcmp(str, "Unlock") == 0)
    {
        return NukiLock::LockAction::Unlock;
    }
    else if (strcmp(str, "lock") == 0 || strcmp(str, "Lock") == 0)
    {
        return NukiLock::LockAction::Lock;
    }
    else if (strcmp(str, "unlatch") == 0 || strcmp(str, "Unlatch") == 0)
    {
        return NukiLock::LockAction::Unlatch;
    }
    else if (strcmp(str, "lockNgo") == 0 || strcmp(str, "LockNgo") == 0)
    {
        return NukiLock::LockAction::LockNgo;
    }
    else if (strcmp(str, "lockNgoUnlatch") == 0 || strcmp(str, "LockNgoUnlatch") == 0)
    {
        return NukiLock::LockAction::LockNgoUnlatch;
    }
    else if (strcmp(str, "fullLock") == 0 || strcmp(str, "FullLock") == 0)
    {
        return NukiLock::LockAction::FullLock;
    }
    else if (strcmp(str, "fobAction2") == 0 || strcmp(str, "FobAction2") == 0)
    {
        return NukiLock::LockAction::FobAction2;
    }
    else if (strcmp(str, "fobAction1") == 0 || strcmp(str, "FobAction1") == 0)
    {
        return NukiLock::LockAction::FobAction1;
    }
    else if (strcmp(str, "fobAction3") == 0 || strcmp(str, "FobAction3") == 0)
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

void NukiWrapper::onConfigUpdateReceivedCallback(const char *value)
{
    nukiInst->onConfigUpdateReceived(value);
}

void NukiWrapper::onConfigUpdateReceived(const char *value)
{
    JsonDocument jsonResult;

    if (!_nukiConfigValid)
    {
        _network->sendResponse(jsonResult, "config not ready", 400);
        return;
    }

    if (!isPinValid())
    {
        _network->sendResponse(jsonResult, "No valid pin set", 400);
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if (jsonError)
    {
        _network->sendResponse(jsonResult, "Invalid json", 400);
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

                        if ((int)battype != 0xff)
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

                        if (keyvalue == 0 || keyvalue == 1)
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

    String msgStr;
    if (basicUpdated || advancedUpdated)
    {
        msgStr = "success";
    }
    else
    {
        msgStr = "noChange";
    }

    _nextConfigUpdateTs = espMillis() + 300;

    _network->sendResponse(jsonResult, msgStr.c_str());

    return;
}

void NukiWrapper::onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int &enabled)
{
    nukiInst->onKeypadCommandReceived(command, id, name, code, enabled);
}

void NukiWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int &enabled)
{
    JsonDocument jsonResult;
    char resultStr[15] = {0};

    if (!_preferences->getBool(preference_keypad_control_enabled))
    {
        _network->sendResponse(jsonResult, "KeypadControlDisabled", 403);
        return;
    }

    if (!hasKeypad())
    {
        if (_nukiConfigValid)
        {
            _network->sendResponse(jsonResult, "KeypadNotAvailable", 404);
        }
        return;
    }

    bool idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), id) != _keypadCodeIds.end();
    int codeInt;
    if (_keypadCodeEncryptionEnabled)
        codeInt = decryptKeypadCode((uint)code.toInt());
    else
        codeInt = code.toInt();
    bool codeValid = codeInt > 100000 && codeInt < 1000000 && (code.indexOf('0') == -1);
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    Log->printf(F("[INFO] Keypad command received: %s\r\n"), command);

    while (retryCount < _nrOfRetries + 1)
    {
        if (strcmp(command, "add") == 0)
        {
            if (name == "")
            {
                _network->sendResponse(jsonResult, "MissingParameterName", 400);
                return;
            }
            if (codeInt == 0)
            {
                _network->sendResponse(jsonResult, "MissingParameterCode", 400);
                return;
            }
            if (!codeValid)
            {
                _network->sendResponse(jsonResult, "CodeInvalid", 422);
                return;
            }

            NukiLock::NewKeypadEntry entry;
            memset(&entry, 0, sizeof(entry));
            size_t nameLen = name.length();
            memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
            entry.code = codeInt;
            result = _nukiLock.addKeypadEntry(entry);
            NukiLock::cmdResultToString(result, resultStr);
            Log->print("[INFO] Add keypad code: ");
            Log->println(resultStr);
            updateKeypad(false);
        }
        else if (strcmp(command, "delete") == 0)
        {
            if (!idExists)
            {
                _network->sendResponse(jsonResult, "UnknownId", 404);
                return;
            }

            result = _nukiLock.deleteKeypadEntry(id);
            NukiLock::cmdResultToString(result, resultStr);
            Log->print("[INFO] Delete keypad code: ");
            Log->println(resultStr);
            updateKeypad(false);
        }
        else if (strcmp(command, "update") == 0)
        {
            if (name == "" || name == "--")
            {
                _network->sendResponse(jsonResult, "MissingParameterName", 400);
                return;
            }
            if (codeInt == 0)
            {
                _network->sendResponse(jsonResult, "MissingParameterCode", 400);
                return;
            }
            if (!codeValid)
            {
                _network->sendResponse(jsonResult, "CodeInvalid", 422);
                return;
            }
            if (!idExists)
            {
                _network->sendResponse(jsonResult, "UnknownId", 404);
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
            NukiLock::cmdResultToString(result, resultStr);
            Log->print("[INFO] Update keypad code: ");
            Log->println(resultStr);
            updateKeypad(false);
        }
        else if (strcmp(command, "") != 0)
        {
            _network->sendResponse(jsonResult, "UnknownCommand", 400);
            return;
        }
        else
        {
            return;
        }

        if (result != Nuki::CmdResult::Success)
        {
            Log->printf(F("[WARNING] Keypad command failed: %s\r\n"), resultStr);
            ++retryCount;
        }
        else
        {
            break;
        }
    }

    if ((int)result != -1)
    {
        _network->sendResponse(jsonResult, resultStr, 200);
    }
}

void NukiWrapper::onTimeControlCommandReceivedCallback(const char *value)
{
    nukiInst->onTimeControlCommandReceived(value);
}

void NukiWrapper::onTimeControlCommandReceived(const char *value)
{
    JsonDocument jsonResult;
    char resultStr[15] = {0};

    if (!_nukiConfigValid)
    {
        _network->sendResponse(jsonResult, "configNotReady", 503);
        return;
    }

    if (!isPinValid())
    {
        _network->sendResponse(jsonResult, "noValidPinSet", 400);
        return;
    }

    if (!_preferences->getBool(preference_timecontrol_control_enabled))
    {
        _network->sendResponse(jsonResult, "timeControlControlDisabled", 403);
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if (jsonError)
    {
        _network->sendResponse(jsonResult, "invalidJson", 400);
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
            _network->sendResponse(jsonResult, "invalidLockAction", 400);
            return;
        }
    }

    if (action)
    {
        bool idExists = false;

        Log->printf(F("[INFO] TimeControl command received: %s\r\n"), action);

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
                    NukiLock::cmdResultToString(result, resultStr);
                    Log->print("[INFO] Delete timecontrol: ");
                    Log->println(resultStr);
                }
                else
                {
                    _network->sendResponse(jsonResult, "noExistingEntryIdSet", 404);
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
                            _network->sendResponse(jsonResult, "invalidTime", 400);
                            return;
                        }
                    }
                    else
                    {
                        _network->sendResponse(jsonResult, "invalidTime", 400);
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
                    NukiLock::cmdResultToString(result, resultStr);
                    Log->print("[INFO] Add timecontrol: ");
                    Log->println(resultStr);
                }
                else if (strcmp(action, "update") == 0)
                {
                    if (!idExists)
                    {
                        _network->sendResponse(jsonResult, "noExistingEntryIdSet"), 404;
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
                            _network->sendResponse(jsonResult, "failedToRetrieveExistingTimeControlEntry", 500);
                            return;
                        }
                    }
                    else
                    {
                        _network->sendResponse(jsonResult, "failedToRetrieveExistingTimeControlEntry", 500);
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
                    NukiLock::cmdResultToString(result, resultStr);
                    Log->print("[INFO] Update timecontrol: ");
                    Log->println(resultStr);
                }
            }
            else
            {
                _network->sendResponse(jsonResult, "invalidAction", 400);
                return;
            }

            if (result != Nuki::CmdResult::Success)
            {
                Log->printf(F("[WARNING] TimeControl command failed: %s\r\n"), resultStr);
                ++retryCount;
            }
            else
            {
                break;
            }
        }

        if ((int)result != -1)
        {
            _network->sendResponse(jsonResult, resultStr);
        }

        _nextConfigUpdateTs = espMillis() + 300;
    }
    else
    {
        _network->sendResponse(jsonResult, "noActionSet", 400);
        return;
    }
}

void NukiWrapper::onAuthCommandReceivedCallback(const char *value)
{
    nukiInst->onAuthCommandReceived(value);
}

void NukiWrapper::onAuthCommandReceived(const char *value)
{
    JsonDocument jsonResult;
    char resultStr[15] = {0};

    if (!_nukiConfigValid)
    {
        _network->sendResponse(jsonResult, "configNotReady", 503);
        return;
    }

    if (!isPinValid())
    {
        _network->sendResponse(jsonResult, "noValidPinSet", 401);
        return;
    }

    if (!_preferences->getBool(preference_auth_control_enabled))
    {
        _network->sendResponse(jsonResult, "keypadControlDisabled", 403);
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if (jsonError)
    {
        _network->sendResponse(jsonResult, "invalidJson", 400);
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

        Log->printf(F("[INFO] Authorization command received: %s\r\n"), action);

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
                    NukiLock::cmdResultToString(result, resultStr);
                    delay(250);
                    Log->print("[INFO] Delete authorization: ");
                    Log->println(resultStr);
                }
                else
                {
                    _network->sendResponse(jsonResult, "noExistingAuthIdSet", 404);
                    return;
                }
            }
            else if (strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
            {
                if (name.length() < 1)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        _network->sendResponse(jsonResult, "noNameSet", 400);
                        return;
                    }
                }

                /*
                if(sharedKey.length() != 64)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        _network->sendResponse(jsonResult, "noSharedKeySet", 400);
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
                                _network->sendResponse(jsonResult, "invalidAllowedFrom", 422);
                                return;
                            }
                        }
                        else
                        {
                            _network->sendResponse(jsonResult, "invalidAllowedFrom", 422);
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
                                _network->sendResponse(jsonResult, "invalidAllowedUntil", 422);
                                return;
                            }
                        }
                        else
                        {
                            _network->sendResponse(jsonResult, "invalidAllowedUntil", 422);
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
                                _network->sendResponse(jsonResult, "invalidAllowedFromTime", 422);
                                return;
                            }
                        }
                        else
                        {
                            _network->sendResponse(jsonResult, "invalidAllowedFromTime", 422);
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
                                _network->sendResponse(jsonResult, "invalidAllowedUntilTime", 422);
                                return;
                            }
                        }
                        else
                        {
                            _network->sendResponse(jsonResult, "invalidAllowedUntilTime", 422);
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
                    _network->sendResponse(jsonResult, "addActionNotSupported", 501);
                    return;

                    NukiLock::NewAuthorizationEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    size_t nameLen = name.length();
                    memcpy(&entry.name, name.c_str(), nameLen > 32 ? 32 : nameLen);
                    /*
                    memcpy(&entry.sharedKey, secretKeyK, 32);

                    if(idType != 1)
                    {
                        _network->sendResponse(jsonResult, "invalidIdType");
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
                    NukiLock::cmdResultToString(result, resultStr);
                    delay(250);
                    Log->print("[INFO] Add authorization: ");
                    Log->println(resultStr);
                }
                else if (strcmp(action, "update") == 0)
                {
                    if (!authId)
                    {
                        _network->sendResponse(jsonResult, "noAuthIdSet", 400);
                        return;
                    }

                    if (!idExists)
                    {
                        _network->sendResponse(jsonResult, "noExistingAuthIdSet", 404);
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
                            _network->sendResponse(jsonResult, "failedToRetrieveExistingAuthorizationEntry", 500);
                            return;
                        }
                    }
                    else
                    {
                        _network->sendResponse(jsonResult, "failedToRetrieveExistingAuthorizationEntry", 500);
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
                    NukiLock::cmdResultToString(result, resultStr);
                    delay(250);
                    Log->print("[INFO] Update authorization: ");
                    Log->println(resultStr);
                }
            }
            else
            {
                _network->sendResponse(jsonResult, "invalidAction", 400);
                return;
            }

            if (result != Nuki::CmdResult::Success)
            {
                Log->printf(F("[WARNING] Authorization command failed: %s\r\n"), resultStr);
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
            _network->sendResponse(jsonResult, resultStr);
        }
    }
    else
    {
        _network->sendResponse(jsonResult, "noActionSet", 400);
        return;
    }
}

Nuki::AdvertisingMode NukiWrapper::advertisingModeToEnum(const char *str)
{
    if (strcmp(str, "Automatic") == 0)
    {
        return Nuki::AdvertisingMode::Automatic;
    }
    else if (strcmp(str, "Normal") == 0)
    {
        return Nuki::AdvertisingMode::Normal;
    }
    else if (strcmp(str, "Slow") == 0)
    {
        return Nuki::AdvertisingMode::Slow;
    }
    else if (strcmp(str, "Slowest") == 0)
    {
        return Nuki::AdvertisingMode::Slowest;
    }
    return (Nuki::AdvertisingMode)0xff;
}

Nuki::TimeZoneId NukiWrapper::timeZoneToEnum(const char *str)
{
    if (strcmp(str, "Africa/Cairo") == 0)
    {
        return Nuki::TimeZoneId::Africa_Cairo;
    }
    else if (strcmp(str, "Africa/Lagos") == 0)
    {
        return Nuki::TimeZoneId::Africa_Lagos;
    }
    else if (strcmp(str, "Africa/Maputo") == 0)
    {
        return Nuki::TimeZoneId::Africa_Maputo;
    }
    else if (strcmp(str, "Africa/Nairobi") == 0)
    {
        return Nuki::TimeZoneId::Africa_Nairobi;
    }
    else if (strcmp(str, "America/Anchorage") == 0)
    {
        return Nuki::TimeZoneId::America_Anchorage;
    }
    else if (strcmp(str, "America/Argentina/Buenos_Aires") == 0)
    {
        return Nuki::TimeZoneId::America_Argentina_Buenos_Aires;
    }
    else if (strcmp(str, "America/Chicago") == 0)
    {
        return Nuki::TimeZoneId::America_Chicago;
    }
    else if (strcmp(str, "America/Denver") == 0)
    {
        return Nuki::TimeZoneId::America_Denver;
    }
    else if (strcmp(str, "America/Halifax") == 0)
    {
        return Nuki::TimeZoneId::America_Halifax;
    }
    else if (strcmp(str, "America/Los_Angeles") == 0)
    {
        return Nuki::TimeZoneId::America_Los_Angeles;
    }
    else if (strcmp(str, "America/Manaus") == 0)
    {
        return Nuki::TimeZoneId::America_Manaus;
    }
    else if (strcmp(str, "America/Mexico_City") == 0)
    {
        return Nuki::TimeZoneId::America_Mexico_City;
    }
    else if (strcmp(str, "America/New_York") == 0)
    {
        return Nuki::TimeZoneId::America_New_York;
    }
    else if (strcmp(str, "America/Phoenix") == 0)
    {
        return Nuki::TimeZoneId::America_Phoenix;
    }
    else if (strcmp(str, "America/Regina") == 0)
    {
        return Nuki::TimeZoneId::America_Regina;
    }
    else if (strcmp(str, "America/Santiago") == 0)
    {
        return Nuki::TimeZoneId::America_Santiago;
    }
    else if (strcmp(str, "America/Sao_Paulo") == 0)
    {
        return Nuki::TimeZoneId::America_Sao_Paulo;
    }
    else if (strcmp(str, "America/St_Johns") == 0)
    {
        return Nuki::TimeZoneId::America_St_Johns;
    }
    else if (strcmp(str, "Asia/Bangkok") == 0)
    {
        return Nuki::TimeZoneId::Asia_Bangkok;
    }
    else if (strcmp(str, "Asia/Dubai") == 0)
    {
        return Nuki::TimeZoneId::Asia_Dubai;
    }
    else if (strcmp(str, "Asia/Hong_Kong") == 0)
    {
        return Nuki::TimeZoneId::Asia_Hong_Kong;
    }
    else if (strcmp(str, "Asia/Jerusalem") == 0)
    {
        return Nuki::TimeZoneId::Asia_Jerusalem;
    }
    else if (strcmp(str, "Asia/Karachi") == 0)
    {
        return Nuki::TimeZoneId::Asia_Karachi;
    }
    else if (strcmp(str, "Asia/Kathmandu") == 0)
    {
        return Nuki::TimeZoneId::Asia_Kathmandu;
    }
    else if (strcmp(str, "Asia/Kolkata") == 0)
    {
        return Nuki::TimeZoneId::Asia_Kolkata;
    }
    else if (strcmp(str, "Asia/Riyadh") == 0)
    {
        return Nuki::TimeZoneId::Asia_Riyadh;
    }
    else if (strcmp(str, "Asia/Seoul") == 0)
    {
        return Nuki::TimeZoneId::Asia_Seoul;
    }
    else if (strcmp(str, "Asia/Shanghai") == 0)
    {
        return Nuki::TimeZoneId::Asia_Shanghai;
    }
    else if (strcmp(str, "Asia/Tehran") == 0)
    {
        return Nuki::TimeZoneId::Asia_Tehran;
    }
    else if (strcmp(str, "Asia/Tokyo") == 0)
    {
        return Nuki::TimeZoneId::Asia_Tokyo;
    }
    else if (strcmp(str, "Asia/Yangon") == 0)
    {
        return Nuki::TimeZoneId::Asia_Yangon;
    }
    else if (strcmp(str, "Australia/Adelaide") == 0)
    {
        return Nuki::TimeZoneId::Australia_Adelaide;
    }
    else if (strcmp(str, "Australia/Brisbane") == 0)
    {
        return Nuki::TimeZoneId::Australia_Brisbane;
    }
    else if (strcmp(str, "Australia/Darwin") == 0)
    {
        return Nuki::TimeZoneId::Australia_Darwin;
    }
    else if (strcmp(str, "Australia/Hobart") == 0)
    {
        return Nuki::TimeZoneId::Australia_Hobart;
    }
    else if (strcmp(str, "Australia/Perth") == 0)
    {
        return Nuki::TimeZoneId::Australia_Perth;
    }
    else if (strcmp(str, "Australia/Sydney") == 0)
    {
        return Nuki::TimeZoneId::Australia_Sydney;
    }
    else if (strcmp(str, "Europe/Berlin") == 0)
    {
        return Nuki::TimeZoneId::Europe_Berlin;
    }
    else if (strcmp(str, "Europe/Helsinki") == 0)
    {
        return Nuki::TimeZoneId::Europe_Helsinki;
    }
    else if (strcmp(str, "Europe/Istanbul") == 0)
    {
        return Nuki::TimeZoneId::Europe_Istanbul;
    }
    else if (strcmp(str, "Europe/London") == 0)
    {
        return Nuki::TimeZoneId::Europe_London;
    }
    else if (strcmp(str, "Europe/Moscow") == 0)
    {
        return Nuki::TimeZoneId::Europe_Moscow;
    }
    else if (strcmp(str, "Pacific/Auckland") == 0)
    {
        return Nuki::TimeZoneId::Pacific_Auckland;
    }
    else if (strcmp(str, "Pacific/Guam") == 0)
    {
        return Nuki::TimeZoneId::Pacific_Guam;
    }
    else if (strcmp(str, "Pacific/Honolulu") == 0)
    {
        return Nuki::TimeZoneId::Pacific_Honolulu;
    }
    else if (strcmp(str, "Pacific/Pago_Pago") == 0)
    {
        return Nuki::TimeZoneId::Pacific_Pago_Pago;
    }
    else if (strcmp(str, "None") == 0)
    {
        return Nuki::TimeZoneId::None;
    }
    return (Nuki::TimeZoneId)0xff;
}

uint8_t NukiWrapper::fobActionToInt(const char *str)
{
    if (strcmp(str, "No Action") == 0)
    {
        return 0;
    }
    else if (strcmp(str, "Unlock") == 0)
    {
        return 1;
    }
    else if (strcmp(str, "Lock") == 0)
    {
        return 2;
    }
    else if (strcmp(str, "Lock n Go") == 0)
    {
        return 3;
    }
    else if (strcmp(str, "Intelligent") == 0)
    {
        return 4;
    }
    return 99;
}

NukiLock::ButtonPressAction NukiWrapper::buttonPressActionToEnum(const char *str)
{
    if (strcmp(str, "No Action") == 0)
    {
        return NukiLock::ButtonPressAction::NoAction;
    }
    else if (strcmp(str, "Intelligent") == 0)
    {
        return NukiLock::ButtonPressAction::Intelligent;
    }
    else if (strcmp(str, "Unlock") == 0)
    {
        return NukiLock::ButtonPressAction::Unlock;
    }
    else if (strcmp(str, "Lock") == 0)
    {
        return NukiLock::ButtonPressAction::Lock;
    }
    else if (strcmp(str, "Unlatch") == 0)
    {
        return NukiLock::ButtonPressAction::Unlatch;
    }
    else if (strcmp(str, "Lock n Go") == 0)
    {
        return NukiLock::ButtonPressAction::LockNgo;
    }
    else if (strcmp(str, "Show Status") == 0)
    {
        return NukiLock::ButtonPressAction::ShowStatus;
    }
    return (NukiLock::ButtonPressAction)0xff;
}

Nuki::BatteryType NukiWrapper::batteryTypeToEnum(const char *str)
{
    if (strcmp(str, "Alkali") == 0)
    {
        return Nuki::BatteryType::Alkali;
    }
    else if (strcmp(str, "Accumulators") == 0)
    {
        return Nuki::BatteryType::Accumulators;
    }
    else if (strcmp(str, "Lithium") == 0)
    {
        return Nuki::BatteryType::Lithium;
    }
    return (Nuki::BatteryType)0xff;
}

NukiLock::MotorSpeed NukiWrapper::motorSpeedToEnum(const char *str)
{
    if (strcmp(str, "Standard") == 0)
    {
        return NukiLock::MotorSpeed::Standard;
    }
    else if (strcmp(str, "Insane") == 0)
    {
        return NukiLock::MotorSpeed::Insane;
    }
    else if (strcmp(str, "Gentle") == 0)
    {
        return NukiLock::MotorSpeed::Gentle;
    }
    return (NukiLock::MotorSpeed)0xff;
}

void NukiWrapper::postponeBleWatchdog()
{
    _disableBleWatchdogTs = espMillis() + 15000;
}

void NukiWrapper::notify(Nuki::EventType eventType)
{
    if (eventType == Nuki::EventType::KeyTurnerStatusUpdated)
    {
        _statusUpdated = true;
    }
}

bool NukiWrapper::readConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    while (retryCount < _nrOfRetries + 1)
    {
        result = _nukiLock.requestConfig(&_nukiConfig);
        _nukiConfigValid = result == Nuki::CmdResult::Success;

        char resultStr[20];
        NukiLock::cmdResultToString(result, resultStr);
        Log->print(F("[DEBUG] Lock config result: "));
        Log->println(resultStr);

        if (result != Nuki::CmdResult::Success)
        {
            if (retryCount == 0)
                Log->println(F("[WARNING] Failed to retrieve lock config, retrying..."));
            else
                Log->println(F("[DEBUG] Failed to retrieve lock config, retrying in 1s"));
            ++retryCount;
            delay(1000);
        }
        else
        {
            return true;
        }
    }
    Log->printf("[ERROR] Could not retrieve lock config after %d retries\n", retryCount - 1);
    return false;
}

uint32_t NukiWrapper::calcKeypadCodeInverse()
{

    uint32_t inverse = 0;
    for (uint32_t i = 1; i < _keypadCodeModulus; ++i)
    {
        if ((_keypadCodeMultiplier * i) % _keypadCodeModulus == 1)
        {
            inverse = i;
            break;
        }
    }
    return inverse;
}

uint32_t NukiWrapper::encryptKeypadCode(uint32_t code)
{
    return ((code * _keypadCodeMultiplier + _keypadCodeOffset) % _keypadCodeModulus) + _keypadCodeModulus;
}

uint32_t NukiWrapper::decryptKeypadCode(uint32_t encryptedCode)
{

    if (_keypadCodeInverse == 0)
    {
        // Error: no inverse
        return 0;
    }

    return ((encryptedCode - _keypadCodeOffset) * _keypadCodeInverse) % _keypadCodeModulus;
}

void NukiWrapper::readAdvancedConfig()
{
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    while (retryCount < _nrOfRetries + 1)
    {
        result = _nukiLock.requestAdvancedConfig(&_nukiAdvancedConfig);
        _nukiAdvancedConfigValid = result == Nuki::CmdResult::Success;

        char resultStr[20];
        NukiLock::cmdResultToString(result, resultStr);
        Log->print(F("[DEBUG] Lock advanced config result: "));
        Log->println(resultStr);

        if (result != Nuki::CmdResult::Success)
        {
            ++retryCount;
            Log->println(F("[WARNING] Failed to retrieve lock advanced config, retrying in 1s"));
            delay(1000);
        }
        else
        {
            break;
        }
    }
}

void NukiWrapper::setTimeCtrlInfoEnabled(bool enabled) { _timeCtrlInfoEnabled = enabled; }
void NukiWrapper::setAuthInfoEnabled(bool enabled) { _authInfoEnabled = enabled; }
void NukiWrapper::setkeypadInfoEnabled(bool enabled) { _keypadInfoEnabled = enabled; }
