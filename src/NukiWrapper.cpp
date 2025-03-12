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
    memset(&_keyTurnerState, sizeof(NukiLock::KeyTurnerState), 0);
    memset(&_batteryReport, sizeof(NukiLock::BatteryReport), 0);
    _keyTurnerState.lockState = NukiLock::LockState::Undefined;
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

    // TODO:
}

void NukiWrapper::lock()
{
    if (!_initialized)
        return;

    // Lock-Befehl an das Nuki-Schloss senden
    _nukiLock.lockAction(NukiLock::LockAction::Lock);
}

void NukiWrapper::unlock()
{
    if (!_initialized)
        return;

    // Unlock-Befehl an das Nuki-Schloss senden
    _nukiLock.lockAction(NukiLock::LockAction::Unlock);
}

void NukiWrapper::unlatch()
{
    if (!_initialized)
        return;

    _nukiLock.lockAction(NukiLock::LockAction::Unlatch);
}

void NukiWrapper::lockngo()
{
    if (!_initialized)
        return;

    _nukiLock.lockAction(NukiLock::LockAction::LockNgo);
}

void NukiWrapper::lockngounlatch()
{
    if (!_initialized)
        return;

    _nukiLock.lockAction(NukiLock::LockAction::LockNgoUnlatch);
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

bool NukiWrapper::updateKeyTurnerState()
{
    if (!_initialized)
        return false;

    // Holt den aktuellen Status vom Schloss
    Nuki::CmdResult result = _nukiLock.requestKeyTurnerState(&_keyTurnerState);

    return (result == Nuki::CmdResult::Success);
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

const bool NukiWrapper::isPaired() const
{
    return _paired;
}

const BLEAddress NukiWrapper::getBleAddress() const
{
    return _nukiLock.getBleAddress();
}

void NukiWrapper::notify(Nuki::EventType eventType)
{
    if (eventType == Nuki::EventType::KeyTurnerStatusUpdated)
    {
        _statusUpdated = true;
    }
}
