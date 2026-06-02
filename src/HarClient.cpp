#include "HarClient.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include "RestartReason.h"
#include "EspMillis.h"
#include "ESP32Ping.h"

// -----------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------

HarClient::HarClient(Preferences* preferences)
    : _preferences(preferences)
{
}

HarClient::~HarClient()
{
    if (_httpClient)
    {
        _httpClient->end();
        delete _httpClient;
        _httpClient = nullptr;
    }
    if (_udpClient)
    {
        delete _udpClient;
        _udpClient = nullptr;
    }
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

void HarClient::initialize()
{
    _enabled  = _preferences->getBool(preference_har_enabled, false);
    _address  = _preferences->getString(preference_har_address, "");
    _mode     = _preferences->getInt(preference_har_mode, 0);       // 0=UDP, 1=REST
    _restMode = _preferences->getInt(preference_har_rest_mode, 0);  // 0=GET, 1=POST

    _user     = _preferences->getString(preference_har_user, "");
    _password = _preferences->getString(preference_har_password, "");

    _port = _preferences->getInt(preference_har_port, 0);
    if (_port == 0)
    {
        _port = 80;
        _preferences->putInt(preference_har_port, _port);
    }

    readSettings();
    start();
}

void HarClient::start()
{
    if (!_enabled)
        return;

    Log->println(F("[INFO] Starting Home Automation Reporting Service"));

    if (_mode == 1 && _httpClient == nullptr) // REST
    {
        _httpClient = new HTTPClient();
        if (_httpClient)
            Log->println(F("[INFO] HAR: HTTPClient created"));
        else
            Log->println(F("[ERROR] HAR: Failed to create HTTPClient"));
    }
    else if (_mode == 0 && _udpClient == nullptr) // UDP
    {
        _udpClient = new NetworkUDP();
        if (_udpClient)
            Log->println(F("[INFO] HAR: NetworkUDP client created"));
        else
            Log->println(F("[ERROR] HAR: Failed to create NetworkUDP client"));
    }
}

bool HarClient::test()
{
    if (!_enabled)
        return true; // not enabled = not a failure

    if (_mode == 1) // REST
    {
        if (_httpClient == nullptr)
        {
            Log->println(F("[DEBUG] HAR: _httpClient is NULL"));
            return false;
        }

        if (!_address.isEmpty())
        {
            if (!Ping.ping(_address.c_str()))
            {
                Log->println(F("[ERROR] HAR: Ping to Home Automation server failed"));
                return false;
            }
            Log->println(F("[DEBUG] HAR: Ping to Home Automation server successful"));
        }

        // Optional connectivity GET using the configured state path
        String strPath = _preferences->getString(preference_har_key_state, "");
        if (!strPath.isEmpty())
        {
            String url = "http://" + _address + ":" + String(_port) + "/" + strPath;
            Log->println("[DEBUG] HAR: Test GET to: " + url);

            HTTPClient http;
            http.begin(url);
            int httpCode = http.GET();
            http.end();

            if (httpCode > 0)
            {
                Log->println("[DEBUG] HAR: Test GET successful, code: " + String(httpCode));
            }
            else
            {
                Log->println(F("[ERROR] HAR: Test GET failed"));
                return false;
            }
        }
        return true;
    }
    else if (_mode == 0) // UDP
    {
        if (_udpClient == nullptr)
        {
            Log->println(F("[DEBUG] HAR: _udpClient is NULL"));
            return false;
        }

        if (!_address.isEmpty())
        {
            if (!Ping.ping(_address.c_str()))
            {
                Log->println(F("[ERROR] HAR: Ping to UDP Home Automation server failed"));
                return false;
            }
            Log->println(F("[DEBUG] HAR: Ping to UDP Home Automation server successful"));
        }
        return true;
    }

    return false; // unknown mode
}

void HarClient::restart()
{
    Log->println(F("[INFO] HAR: Reinitializing transport client"));
 
    // Phase 1: Clean up only if an existing client is allocated
    if (_mode == 1 && _httpClient)
    {
        _httpClient->end();
        delete _httpClient;
        _httpClient = nullptr;
        Log->println(F("[INFO] HAR: Old HTTPClient deleted"));
    }
    else if (_mode == 0 && _udpClient)
    {
        delete _udpClient;
        _udpClient = nullptr;
        Log->println(F("[INFO] HAR: Old UDP client deleted"));
    }
 
    // Phase 2: Always recreate — even if the pointer was already null.
    if (_mode == 1)
    {
        _httpClient = new HTTPClient();
        if (_httpClient)
        {
            Log->println(F("[INFO] HAR: HTTPClient successfully reinitialized"));
            _isOk = true;
        }
        else
        {
            Log->println(F("[ERROR] HAR: Failed to reinitialize HTTPClient"));
        }
    }
    else if (_mode == 0)
    {
        _udpClient = new NetworkUDP();
        if (_udpClient)
        {
            Log->println(F("[INFO] HAR: UDP client successfully reinitialized"));
            _isOk = true;
        }
        else
        {
            Log->println(F("[ERROR] HAR: Failed to reinitialize UDP client"));
        }
    }
}

void HarClient::disable()
{
    _enabled = false;
}

void HarClient::readSettings()
{
    _rssiInterval = _preferences->getInt(preference_rssi_send_interval, 0) * 1000;
    if (_rssiInterval == 0)
    {
        _rssiInterval = 60000; // Default: 60 seconds
        _preferences->putInt(preference_rssi_send_interval, 60);
    }

    _maintenanceInterval = _preferences->getInt(preference_Maintenance_send_interval, 0) * 1000;
    _sendDebugInfo = _preferences->getBool(preference_send_debug_info, false);
}

// -----------------------------------------------------------------------
// Status
// -----------------------------------------------------------------------

bool HarClient::isEnabled() const
{
    return _enabled;
}

bool HarClient::isOk() const
{
    return _isOk;
}

void HarClient::setOk(bool ok)
{
    _isOk = ok;
}

// -----------------------------------------------------------------------
// Periodic update
// -----------------------------------------------------------------------

void HarClient::update(int64_t ts, int8_t wifiRssi)
{
    if (!_enabled || !_isOk)
        return;

    // --- WiFi RSSI ---
    if (wifiRssi != 127 && _rssiInterval > 0 && ts - _lastRssiTs > _rssiInterval)
    {
        _lastRssiTs = ts;

        if (wifiRssi != _lastWifiRssi)
        {
            String key   = _preferences->getString(preference_har_key_wifi_rssi);
            String param = _preferences->getString(preference_har_param_wifi_rssi);

            if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
                sendInt(key.c_str(), param.c_str(), wifiRssi);

            _lastWifiRssi = wifiRssi;
        }
    }

    // --- Maintenance telemetry ---
    if (_lastMaintenanceTs == 0 || (ts - _lastMaintenanceTs) > _maintenanceInterval)
    {
        // Uptime (minutes) — only if changed
        int64_t curUptime = ts / 1000 / 60;
        if (curUptime > _publishedUpTime)
        {
            String key   = _preferences->getString(preference_har_key_uptime);
            String param = _preferences->getString(preference_har_param_uptime);

            if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
                sendULong(key.c_str(), param.c_str(), (unsigned long)curUptime);

            _publishedUpTime = curUptime;
        }

        // One-shot boot info (sent only on first maintenance cycle)
        if (_lastMaintenanceTs == 0)
        {
            {
                String key   = _preferences->getString(preference_har_key_restart_reason_fw);
                String param = _preferences->getString(preference_har_param_restart_reason_fw);
                if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
                    sendString(key.c_str(), param.c_str(), getRestartReason().c_str());
            }
            {
                String key   = _preferences->getString(preference_har_key_restart_reason_esp);
                String param = _preferences->getString(preference_har_param_restart_reason_esp);
                if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
                    sendString(key.c_str(), param.c_str(), getEspRestartReason().c_str());
            }
            {
                String key   = _preferences->getString(preference_har_key_info_nuki_bridge_version);
                String param = _preferences->getString(preference_har_param_info_nuki_bridge_version);
                if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
                    sendString(key.c_str(), param.c_str(), NUKI_REST_BRIDGE_VERSION);
            }
            {
                String key   = _preferences->getString(preference_har_key_info_nuki_bridge_build);
                String param = _preferences->getString(preference_har_param_info_nuki_bridge_build);
                if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
                    sendString(key.c_str(), param.c_str(), NUKI_REST_BRIDGE_BUILD);
            }
        }

        // Free heap (only when debug reporting is enabled)
        if (_sendDebugInfo)
        {
            String key   = _preferences->getString(preference_har_key_freeheap);
            String param = _preferences->getString(preference_har_param_freeheap);
            if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
                sendUInt(key.c_str(), param.c_str(), esp_get_free_heap_size());
        }

        _lastMaintenanceTs = ts;
    }
}

// -----------------------------------------------------------------------
// Typed send overloads
// -----------------------------------------------------------------------

void HarClient::sendFloat(const char* key, const char* param,
                          float value, uint8_t precision)
{
    char buf[30];
    dtostrf(value, 0, precision, buf);
    sendData(key, param, buf);
}

void HarClient::sendInt(const char* key, const char* param, int value)
{
    char buf[30];
    snprintf(buf, sizeof(buf), "%d", value);
    sendData(key, param, buf);
}

void HarClient::sendUInt(const char* key, const char* param, unsigned int value)
{
    char buf[30];
    snprintf(buf, sizeof(buf), "%u", value);
    sendData(key, param, buf);
}

void HarClient::sendULong(const char* key, const char* param, unsigned long value)
{
    char buf[30];
    snprintf(buf, sizeof(buf), "%lu", value);
    sendData(key, param, buf);
}

void HarClient::sendLongLong(const char* key, const char* param, int64_t value)
{
    char buf[30];
    snprintf(buf, sizeof(buf), "%lld", value);
    sendData(key, param, buf);
}

void HarClient::sendBool(const char* key, const char* param, bool value)
{
    char buf[2] = {0};
    buf[0] = value ? '1' : '0';
    sendData(key, param, buf);
}

void HarClient::sendString(const char* key, const char* param, const char* value)
{
    sendData(key, param, value);
}

// -----------------------------------------------------------------------
// Higher-level send methods (read key/param from preferences)
// -----------------------------------------------------------------------

void HarClient::sendLockBleAddress(const std::string& address)
{
    if (!_enabled || !_isOk)
        return;

    String key   = _preferences->getString(preference_har_key_ble_address);
    String param = _preferences->getString(preference_har_param_ble_address);

    if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
        sendData(key.c_str(), param.c_str(), address.c_str());
}

void HarClient::sendKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState,
                                   const NukiLock::KeyTurnerState& lastKeyTurnerState)
{
    if (!_enabled || !_isOk)
        return;

    String key;
    key.reserve(384);
    String param;
    param.reserve(128);

    // Lock state
    key   = _preferences->getString(preference_har_key_lock_state);
    param = _preferences->getString(preference_har_param_lock_state);
    if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
        sendInt(key.c_str(), param.c_str(), (int)keyTurnerState.lockState);

    // LockNgo timer
    key   = _preferences->getString(preference_har_key_lockngo_state);
    param = _preferences->getString(preference_har_param_lockngo_state);
    if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
        sendInt(key.c_str(), param.c_str(),
                (int)(keyTurnerState.lockNgoTimer != 255 ? keyTurnerState.lockNgoTimer : 0));

    // Trigger (first send or on change)
    if (_firstTunerStateSent || keyTurnerState.trigger != lastKeyTurnerState.trigger)
    {
        key   = _preferences->getString(preference_har_key_lock_trigger);
        param = _preferences->getString(preference_har_param_lock_trigger);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(), (int)keyTurnerState.trigger);
    }

    // Night mode
    key   = _preferences->getString(preference_har_key_lock_night_mode);
    param = _preferences->getString(preference_har_param_lock_night_mode);
    if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
        sendInt(key.c_str(), param.c_str(),
                (int)(keyTurnerState.nightModeActive != 255 ? keyTurnerState.nightModeActive : 0));

    // Completion status (first send or on change)
    if (_firstTunerStateSent ||
        keyTurnerState.lastLockActionCompletionStatus !=
        lastKeyTurnerState.lastLockActionCompletionStatus)
    {
        key   = _preferences->getString(preference_har_key_lock_completionStatus);
        param = _preferences->getString(preference_har_param_lock_completionStatus);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(),
                    (int)keyTurnerState.lastLockActionCompletionStatus);
    }

    // Door sensor state (first send or on change)
    if (_firstTunerStateSent ||
        keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
    {
        key   = _preferences->getString(preference_har_key_doorsensor_state);
        param = _preferences->getString(preference_har_param_doorsensor_state);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(), (int)keyTurnerState.doorSensorState);
    }

    // Battery critical / level / charging — derived from criticalBatteryState
    // Bit 0: critical, Bit 1: charging, Bits 2–7: level (shift right by 1)
    bool    critical = (keyTurnerState.criticalBatteryState & 1) == 1;
    bool    charging = (keyTurnerState.criticalBatteryState & 2) == 2;
    uint8_t level    = ((keyTurnerState.criticalBatteryState & 0b11111100) >> 1);

    if (_firstTunerStateSent ||
        keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState)
    {
        key   = _preferences->getString(preference_har_key_lock_battery_critical);
        param = _preferences->getString(preference_har_param_lock_battery_critical);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(), (int)critical);

        key   = _preferences->getString(preference_har_key_lock_battery_level);
        param = _preferences->getString(preference_har_param_lock_battery_level);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(), (int)level);

        key   = _preferences->getString(preference_har_key_lock_battery_charging);
        param = _preferences->getString(preference_har_param_lock_battery_charging);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(), (int)charging);
    }

    // Keypad critical — derived from accessoryBatteryState
    // Present (bit 0 set) AND critical (bit 1 set) → keypadCritical = true
    bool keypadCritical = keyTurnerState.accessoryBatteryState != 255
        ? ((keyTurnerState.accessoryBatteryState & 1) == 1
               ? (keyTurnerState.accessoryBatteryState & 3) == 3
               : false)
        : false;

    if (_firstTunerStateSent ||
        keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState)
    {
        key   = _preferences->getString(preference_har_key_keypad_critical);
        param = _preferences->getString(preference_har_param_keypad_critical);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(), (int)keypadCritical);
    }

    // Door sensor critical — also derived from accessoryBatteryState
    // Present (bit 2 set) AND critical (bit 3 set) → doorSensorCritical = true
    bool doorSensorCritical = keyTurnerState.accessoryBatteryState != 255
        ? ((keyTurnerState.accessoryBatteryState & 4) == 4
               ? (keyTurnerState.accessoryBatteryState & 12) == 12
               : false)
        : false;

    if (_firstTunerStateSent ||
        keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState)
    {
        key   = _preferences->getString(preference_har_key_doorsensor_critical);
        param = _preferences->getString(preference_har_param_doorsensor_critical);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(), (int)doorSensorCritical);
    }

    // Remote access state
    key   = _preferences->getString(preference_har_key_remote_access_state);
    param = _preferences->getString(preference_har_param_remote_access_state);
    if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
        sendInt(key.c_str(), param.c_str(), (int)keyTurnerState.remoteAccessStatus);

    // BLE connection strength (only if valid, i.e. != 1)
    if (keyTurnerState.bleConnectionStrength != 1)
    {
        key   = _preferences->getString(preference_har_key_ble_strength);
        param = _preferences->getString(preference_har_param_ble_strength);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendInt(key.c_str(), param.c_str(), (int)keyTurnerState.bleConnectionStrength);
    }

    _firstTunerStateSent = false;
}

void HarClient::sendBatteryReport(const NukiLock::BatteryReport& batteryReport)
{
    if (!_enabled || !_isOk)
        return;

    {
        String key   = _preferences->getString(preference_har_key_battery_voltage);
        String param = _preferences->getString(preference_har_param_battery_voltage);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendFloat(key.c_str(), param.c_str(),
                      (float)batteryReport.batteryVoltage / 1000.0f);
    }
    {
        String key   = _preferences->getString(preference_har_key_battery_drain);
        String param = _preferences->getString(preference_har_param_battery_drain);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendFloat(key.c_str(), param.c_str(), batteryReport.batteryDrain);
    }
    {
        String key   = _preferences->getString(preference_har_key_battery_max_turn_current);
        String param = _preferences->getString(preference_har_param_battery_max_turn_current);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendFloat(key.c_str(), param.c_str(),
                      (float)batteryReport.maxTurnCurrent / 1000.0f);
    }
    {
        String key   = _preferences->getString(preference_har_key_battery_lock_distance);
        String param = _preferences->getString(preference_har_param_battery_lock_distance);
        if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
            sendFloat(key.c_str(), param.c_str(), batteryReport.lockDistance);
    }
}

void HarClient::sendBleRssi(const int& rssi)
{
    if (!_enabled || !_isOk)
        return;

    String key   = _preferences->getString(preference_har_key_ble_rssi);
    String param = _preferences->getString(preference_har_param_ble_rssi);

    if ((_mode == 1 && key.length() > 0) || (_mode == 0 && param.length() > 0))
        sendInt(key.c_str(), param.c_str(), rssi);
}

// -----------------------------------------------------------------------
// Core transport
// -----------------------------------------------------------------------

void HarClient::sendData(const char* key, const char* param, const char* value)
{
    if (!_enabled || !_isOk)
        return;

    // --- UDP Mode ---
    if (_mode == 0)
    {
        if (!param || !*param)
            return;

        char message[384];
        snprintf(message, sizeof(message), "%s=%s", param, value ? value : "");

        _udpClient->beginPacket(_address.c_str(), _port);
        _udpClient->write(reinterpret_cast<const uint8_t*>(message), strlen(message));
        _udpClient->endPacket();
        return;
    }

    // --- REST Mode ---
    if (_mode == 1)
    {
        if (!key || !*key)
            return;

        const size_t BUFFER_SIZE = 256;
        char url[BUFFER_SIZE]      = {0};
        char postData[BUFFER_SIZE] = {0};

        // Build base URL: http://[user:pass@]host[:port]/key
        snprintf(url, BUFFER_SIZE, "http://");

        // Optional Basic Auth
        if (_user.length() > 0 && _password.length() > 0)
        {
            strncat(url, _user.c_str(),     BUFFER_SIZE - strlen(url) - 1);
            strncat(url, ":",               BUFFER_SIZE - strlen(url) - 1);
            strncat(url, _password.c_str(), BUFFER_SIZE - strlen(url) - 1);
            strncat(url, "@",               BUFFER_SIZE - strlen(url) - 1);
        }

        // Add host + port
        strncat(url, _address.c_str(), BUFFER_SIZE - strlen(url) - 1);

        if (_port > 0)
        {
            char portStr[6];
            snprintf(portStr, sizeof(portStr), ":%d", _port);
            strncat(url, portStr, BUFFER_SIZE - strlen(url) - 1);
        }

        // Add Path
        strncat(url, "/",  BUFFER_SIZE - strlen(url) - 1);
        strncat(url, key,  BUFFER_SIZE - strlen(url) - 1);

        int httpCode = -1;

        if (_restMode == 0) // GET — append /param/value to URL
        {
            if (param && *param)
            {
                strncat(url, "/",   BUFFER_SIZE - strlen(url) - 1);
                strncat(url, param, BUFFER_SIZE - strlen(url) - 1);
            }
            if (value && *value)
            {
                strncat(url, value, BUFFER_SIZE - strlen(url) - 1);
            }

            // Send HTTP request
            _httpClient->begin(url);
            _httpClient->addHeader("Content-Type",
                                   "application/x-www-form-urlencoded");
            httpCode = _httpClient->GET();
        }
        else // POST — param=value in body
        {
            if (param && *param)
                strncat(postData, param, BUFFER_SIZE - strlen(postData) - 1);
            if (value && *value)
                strncat(postData, value, BUFFER_SIZE - strlen(postData) - 1);

            // Send HTTP request    
            _httpClient->begin(url);
            _httpClient->addHeader("Content-Type",
                                   "application/x-www-form-urlencoded");
            httpCode = _httpClient->POST(postData);
        }

        if (httpCode > 0)
        {
            Log->println(_httpClient->getString());
        }
        else
        {
            Log->printf(F("[ERROR] HAR: HTTP request failed: %s\n"),
                        _httpClient->errorToString(httpCode).c_str());
        }

        _httpClient->end();
    }
}