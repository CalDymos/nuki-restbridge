#include "NukiNetwork.h"
#include "PreferencesKeys.h"
#include "Logger.hpp"
#include "Config.h"
#include "RestartReason.h"
#include "hal/wdt_hal.h"

NukiNetwork *NukiNetwork::_inst = nullptr;

// Globale oder externe Variablen
extern bool ethCriticalFailure;
extern bool wifiFallback;
extern bool disableNetwork;
extern bool forceEnableWebCfgServer;

NukiNetwork::NukiNetwork(Preferences *preferences, char *buffer, size_t bufferSize)
    : _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize)
{
    _inst = this;
    _webEnabled = _preferences->getBool(preference_webcfgserver_enabled, true);
    _apiPort = _preferences->getInt(preference_api_port, REST_SERVER_PORT);
    _apitoken = new BridgeApiToken(_preferences, preference_api_token);
    _apiEnabled = _preferences->getBool(preference_api_enabled);
    _lockEnabled = preferences->getBool(preference_lock_enabled);
    setupDevice();
}

NukiNetwork::~NukiNetwork()
{
    // Aufräumen, Webserver stoppen, etc.
    if (_server)
    {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
    if (_httpClient)
    {
        _httpClient->end();
        delete _httpClient;
    }
}

void NukiNetwork::setupDevice()
{

    _ipConfiguration = new IPConfiguration(_preferences);
    int selhardware = _preferences->getInt(preference_network_hardware, 0);

    _firstBootAfterDeviceChange = _preferences->getBool(preference_ntw_reconfigure, false);

    if (wifiFallback == true)
    {
        if (!_firstBootAfterDeviceChange)
        {
            Log->println(F("[ERROR] Failed to connect to network. Wi-Fi fallback is disabled, rebooting."));
            wifiFallback = false;
            sleep(5);
            restartEsp(RestartReason::NetworkDeviceCriticalFailureNoWifiFallback);
        }

        Log->println(F("[INFO] Switching to Wi-Fi device as fallback."));
        _networkDeviceType = NetworkDeviceType::WiFi;
    }
    else
    {
        if (selhardware == 0)
        {
#ifndef CONFIG_IDF_TARGET_ESP32H2
            selhardware = 1;
#else
            selhardware = 2;
#endif
            _preferences->putInt(preference_network_hardware, selhardware);
        }
        if (selhardware == 1)
            _networkDeviceType = NetworkDeviceType::WiFi;
        else
            _networkDeviceType = NetworkDeviceType::ETH;
    }
}

void NukiNetwork::initialize()
{
    if (!disableNetwork)
    {
        strncpy(_apiLockPath, api_path_lock, sizeof(_apiLockPath) - 1);
        strncpy(_apiBridgePath, api_path_bridge, sizeof(_apiBridgePath) - 1);

        _homeAutomationEnabled = _preferences->getBool(preference_ha_enabled, false);
        _homeAutomationAdress = _preferences->getString(preference_ha_address, "");

        _hostname = _preferences->getString(preference_hostname, "");

        if (_hostname == "")
        {
            _hostname = "nukibridge";
            _preferences->putString(preference_hostname, _hostname);
        }

        _homeAutomationPort = _preferences->getInt(preference_ha_port, 0);

        if (_homeAutomationPort == 0)
        {
            _homeAutomationPort = 80;
            _preferences->putInt(preference_ha_port, _homeAutomationPort);
        }

        switch (_networkDeviceType)
        {
        case NetworkDeviceType::WiFi:
            initializeWiFi();
            break;
        case NetworkDeviceType::ETH:
            initializeEthernet();
            break;
        case NetworkDeviceType::UNDEFINED:
            break;
        }

        Log->print("[DEBUG] Host name: ");
        Log->println(_hostname);

        String _homeAutomationUser = _preferences->getString(preference_ha_user);

        String _homeAutomationPass = _preferences->getString(preference_ha_password);

        readSettings();

        startNetworkServices();
    }
    _initialized = true;
}

bool NukiNetwork::update()
{
    if (!_initialized)
        return false;

    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    int64_t ts = espMillis();

    // update device
    switch (_networkDeviceType)
    {
    case NetworkDeviceType::WiFi:
        break;
    case NetworkDeviceType::ETH:
        if (_checkIpTs != -1 && _checkIpTs < espMillis())
        {
            if (_ipConfiguration->ipAddress() != ETH.localIP())
            {
                Log->println("[INFO] ETH Set static IP");
                ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
                _checkIpTs = espMillis() + 5000;
            }

            _checkIpTs = -1;
        }
        break;
    }

    if (disableNetwork || isApOpen())
    {
        return false;
    }

    if (!isConnected() || (_networkServicesConnectCounter > 15))
    {
        _networkServicesConnectCounter = 0;

        if (_restartOnDisconnect && espMillis() > 60000)
        {
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
        else if (_disableNetworkIfNotConnected && espMillis() > 60000)
        {
            disableNetwork = true;
            restartEsp(RestartReason::DisableNetworkIfNotConnected);
        }
    }

    if (isConnected())
    {
        if (ts - _lastNetworkServiceTs > 30000)
        { // test all 30 seconds
            _lastNetworkServiceTs = ts;
            _networkServicesState = testNetworkServices();
            if (_networkServicesState != NetworkServiceStates::OK)
            { // error in networ Services
                restartNetworkServices(_networkServicesState);
                delay(1000);
                _networkServicesState = testNetworkServices(); // test network services again
                if (_networkServicesState != NetworkServiceStates::OK)
                {
                    _networkServicesConnectCounter++;
                    return false;
                }
            }
        }

        _networkServicesConnectCounter = 0;
        if (forceEnableWebCfgServer && !_webEnabled)
        {
            forceEnableWebCfgServer = false;
            delay(200);
            restartEsp(RestartReason::ReconfigureWebCfgServer);
        }
        else if (!_webEnabled)
        {
            forceEnableWebCfgServer = false;
        }
        delay(2000);
    }

    if (!isConnected())
    {
        if (_networkTimeout > 0 && (ts - _lastConnectedTs > _networkTimeout * 1000) && ts > 60000)
        {
            if (!_webEnabled)
            {
                forceEnableWebCfgServer = true;
            }
            Log->println(F("[WARNING] Network timeout has been reached, restarting ..."));
            delay(200);
            restartEsp(RestartReason::NetworkTimeoutWatchdog);
        }
        delay(2000);
        return false;
    }

    _lastConnectedTs = ts;

    // send nuki Bridge WLAN rssi to HA
    if (_homeAutomationEnabled && (signalStrength() != 127 && _rssiSendInterval > 0 && ts - _lastRssiTs > _rssiSendInterval))
    {
        _lastRssiTs = ts;
        int8_t rssi = signalStrength();

        if (rssi != _lastRssi)
        {
            String path = _preferences->getString(preference_ha_path_wifi_rssi);
            String query = _preferences->getString(preference_ha_query_wifi_rssi);

            if (path && query)
                sendToHAInt(path.c_str(), query.c_str(), signalStrength());
            _lastRssi = rssi;
        }
    }

    if (_homeAutomationEnabled && (_lastMaintenanceTs == 0 || (ts - _lastMaintenanceTs) > _MaintenanceSendIntervall))
    {
        int64_t curUptime = ts / 1000 / 60;
        if (curUptime > _publishedUpTime)
        {
            String path = _preferences->getString(preference_ha_path_uptime);
            String query = _preferences->getString(preference_ha_query_uptime);

            if (path && query)
                sendToHAULong(path.c_str(), query.c_str(), curUptime);
            _publishedUpTime = curUptime;
        }

        if (_lastMaintenanceTs == 0)
        {
            String path = _preferences->getString(preference_ha_path_restart_reason_fw);
            String query = _preferences->getString(preference_ha_query_restart_reason_fw);

            if (path && query)
                sendToHAString(path.c_str(), query.c_str(), getRestartReason().c_str());

            path = _preferences->getString(preference_ha_path_restart_reason_esp);
            query = _preferences->getString(preference_ha_query_restart_reason_esp);

            if (path && query)
                sendToHAString(path.c_str(), query.c_str(), getEspRestartReason().c_str());

            path = _preferences->getString(preference_ha_path_info_nuki_bridge_version);
            query = _preferences->getString(preference_ha_query_info_nuki_bridge_version);

            if (path && query)
                sendToHAString(path.c_str(), query.c_str(), NUKI_REST_BRIDGE_VERSION);

            path = _preferences->getString(preference_ha_path_info_nuki_bridge_build);
            query = _preferences->getString(preference_ha_query_info_nuki_bridge_build);

            if (path && query)
                sendToHAString(path.c_str(), query.c_str(), NUKI_REST_BRIDGE_BUILD);
        }
        if (_publishDebugInfo)
        {
            String path = _preferences->getString(preference_ha_path_freeheap);
            String query = _preferences->getString(preference_ha_query_freeheap);

            if (path && query)
                sendToHAUInt(path.c_str(), query.c_str(), esp_get_free_heap_size());
        }
        _lastMaintenanceTs = ts;
    }

    return true;
}

void NukiNetwork::reconfigure()
{
    switch (_networkDeviceType)
    {
    case NetworkDeviceType::WiFi:
        _preferences->putString(preference_wifi_ssid, "");
        _preferences->putString(preference_wifi_pass, "");
        delay(200);
        restartEsp(RestartReason::ReconfigureWifi);
        break;
    case NetworkDeviceType::ETH:
        delay(200);
        restartEsp(RestartReason::ReconfigureETH);
        break;
    }
}

void NukiNetwork::scan(bool passive, bool async)
{
    if (_networkDeviceType == NetworkDeviceType::WiFi)
    {
        if (!_openAP)
        {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_STA);
            WiFi.disconnect();
        }

        WiFi.scanDelete();
        WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

        if (async)
        {
            Log->println(F("[DEBUG] Wi-Fi async scan started"));
        }
        else
        {
            Log->println(F("[DEBUG] Wi-Fi sync scan started"));
        }
        if (passive)
        {
            WiFi.scanNetworks(async, false, true, 75U);
        }
        else
        {
            WiFi.scanNetworks(async);
        }
    }
}

bool NukiNetwork::isApOpen() const
{
    return (_networkDeviceType == NetworkDeviceType::WiFi ? _APisReady : false);
}

bool NukiNetwork::isConnected() const
{
    return (_networkDeviceType == NetworkDeviceType::WiFi ? WiFi.isConnected() : _connected);
}

bool NukiNetwork::isWifiConnected()
{
    return (_networkDeviceType != NetworkDeviceType::WiFi ? true : isConnected());
}

bool NukiNetwork::isWifiConfigured() const
{
    return _WiFissid.length() > 0 && _WiFipass.length() > 0;
}

String NukiNetwork::localIP() const
{
    return (_networkDeviceType == NetworkDeviceType::ETH ? ETH.localIP().toString() : WiFi.localIP().toString());
}

String NukiNetwork::networkBSSID() const
{
    return (_networkDeviceType == NetworkDeviceType::WiFi ? WiFi.BSSIDstr() : String(""));
}

const NetworkDeviceType NukiNetwork::networkDeviceType()
{
    return _networkDeviceType;
}

int8_t NukiNetwork::signalStrength()
{
    return (_networkDeviceType == NetworkDeviceType::ETH ? -1 : WiFi.RSSI());
}

void NukiNetwork::clearWifiFallback()
{
    wifiFallback = false;
}

void NukiNetwork::disableAutoRestarts()
{
    _networkTimeout = 0;
    _restartOnDisconnect = false;
}

void NukiNetwork::disableAPI()
{
    _apiEnabled = false;
}
void NukiNetwork::disableHAR()
{
    _homeAutomationEnabled = false;
}

NetworkServiceStates NukiNetwork::networkServicesState()
{
    return _networkServicesState;
}

uint8_t NukiNetwork::queryCommands()
{
    uint8_t qc = _queryCommands;
    _queryCommands = 0;
    return qc;
}

void NukiNetwork::sendToHAFloat(const char *path, const char *query, const float value, uint8_t precision)
{
    if (_homeAutomationEnabled)
    {
        char buffer[30];
        dtostrf(value, 0, precision, buffer);
        sendRequestToHA(path, query, buffer);
    }
}

void NukiNetwork::sendToHAInt(const char *path, const char *query, const int value)
{
    if (_homeAutomationEnabled)
    {
        char buffer[30];
        itoa(value, buffer, 10);
        sendRequestToHA(path, query, buffer);
    }
}

void NukiNetwork::sendToHAUInt(const char *path, const char *query, const unsigned int value)
{
    if (_homeAutomationEnabled)
    {
        char buffer[30];
        utoa(value, buffer, 10);
        sendRequestToHA(path, query, buffer);
    }
}

void NukiNetwork::sendToHAULong(const char *path, const char *query, const unsigned long value)
{
    if (_homeAutomationEnabled)
    {
        char buffer[30];
        ultoa(value, buffer, 10);
        sendRequestToHA(path, query, buffer);
    }
}

void NukiNetwork::sendToHALongLong(const char *path, const char *query, const int64_t value)
{
    if (_homeAutomationEnabled)
    {
        char buffer[30];
        lltoa(value, buffer, 10);
        sendRequestToHA(path, query, buffer);
    }
}

void NukiNetwork::sendToHABool(const char *path, const char *query, const bool value)
{
    if (_homeAutomationEnabled)
    {
        char buffer[2] = {0};
        buffer[0] = value ? '1' : '0';
        sendRequestToHA(path, query, buffer);
    }
}

void NukiNetwork::sendToHAString(const char *path, const char *query, const char *value)
{
    if (_homeAutomationEnabled)
    {
        sendRequestToHA(path, query, value);
    }
}

void NukiNetwork::sendToHALockBleAddress(const std::string &address)
{
    if (_homeAutomationEnabled)
    {

        String path = _preferences->getString(preference_ha_path_ble_address);
        String query = _preferences->getString(preference_ha_query_ble_address);

        if (path && query)
            sendRequestToHA(path.c_str(), query.c_str(), address.c_str());
    }
}

void NukiNetwork::sendToHABatteryReport(const NukiLock::BatteryReport &batteryReport)
{
    if (_homeAutomationEnabled)
    {
        String path;
        path.reserve(384);
        String query;
        query.reserve(128);

        path = _preferences->getString(preference_ha_path_battery_voltage);
        query = _preferences->getString(preference_ha_query_battery_voltage);
        if (path && query)
        {
            sendToHAFloat(path.c_str(), query.c_str(), (float)batteryReport.batteryVoltage / 1000.0, true);
        }
        path = _preferences->getString(preference_ha_path_battery_drain);
        query = _preferences->getString(preference_ha_query_battery_drain);
        if (path && query)
        {
            sendToHAFloat(path.c_str(), query.c_str(), batteryReport.batteryDrain, true); // milliwatt seconds
        }
        path = _preferences->getString(preference_ha_path_battery_max_turn_current);
        query = _preferences->getString(preference_ha_query_battery_max_turn_current);
        if (path && query)
        {
            sendToHAFloat(path.c_str(), query.c_str(), (float)batteryReport.maxTurnCurrent / 1000.0, true);
        }
        path = _preferences->getString(preference_ha_path_battery_lock_distance);
        query = _preferences->getString(preference_ha_query_battery_lock_distance);
        if (path && query)
        {
            sendToHAFloat(path.c_str(), query.c_str(), batteryReport.lockDistance, true); // degrees
        }
    }
}

void NukiNetwork::sendToHABleRssi(const int &rssi)
{
    if (_homeAutomationEnabled)
    {
        String path = _preferences->getString(preference_ha_path_ble_rssi);
        String query = _preferences->getString(preference_ha_query_ble_rssi);

        if (path && query)
            sendToHAInt(path.c_str(), query.c_str(), rssi);
    }
}

void NukiNetwork::sendToHAKeyTurnerState(const NukiLock::KeyTurnerState &keyTurnerState, const NukiLock::KeyTurnerState &lastKeyTurnerState)
{
    if (_homeAutomationEnabled)
    {
        char str[50];
        memset(&str, 0, sizeof(str));

        String path;
        path.reserve(384);
        String query;
        query.reserve(128);

        if (_homeAutomationEnabled)
        {
            lockstateToString(keyTurnerState.lockState, str);

            path = _preferences->getString(preference_ha_path_lock_state);
            query = _preferences->getString(preference_ha_query_lock_state);

            if (path && query)
            {
                sendToHAInt(path.c_str(), query.c_str(), (int)keyTurnerState.lockState);
            }

            path = _preferences->getString(preference_ha_path_lockngo_state);
            query = _preferences->getString(preference_ha_query_lockngo_state);

            if (path && query)
            {
                sendToHAInt(path.c_str(), query.c_str(), (int)keyTurnerState.lockNgoTimer);
            }

            memset(&str, 0, sizeof(str));

            triggerToString(keyTurnerState.trigger, str);

            if (_firstTunerStatePublish || keyTurnerState.trigger != lastKeyTurnerState.trigger)
            {
                path = _preferences->getString(preference_ha_path_lock_trigger);
                query = _preferences->getString(preference_ha_query_lock_trigger);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)keyTurnerState.trigger);
                }
            }

            path = _preferences->getString(preference_ha_path_lock_night_mode);
            query = _preferences->getString(preference_ha_query_lock_night_mode);

            if (path && query)
            {
                sendToHAInt(path.c_str(), query.c_str(), (int)keyTurnerState.nightModeActive);
            }

            memset(&str, 0, sizeof(str));
            NukiLock::completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);

            if (_firstTunerStatePublish || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
            {
                path = _preferences->getString(preference_ha_path_lock_completionStatus);
                query = _preferences->getString(preference_ha_query_lock_completionStatus);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)keyTurnerState.lastLockActionCompletionStatus);
                }
            }

            memset(&str, 0, sizeof(str));

            NukiLock::doorSensorStateToString(keyTurnerState.doorSensorState, str);

            if (_firstTunerStatePublish || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
            {
                path = _preferences->getString(preference_ha_path_doorsensor_state);
                query = _preferences->getString(preference_ha_query_doorsensor_state);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)keyTurnerState.doorSensorState);
                }
            }

            bool critical = (keyTurnerState.criticalBatteryState & 1) == 1;
            bool charging = (keyTurnerState.criticalBatteryState & 2) == 2;
            uint8_t level = ((keyTurnerState.criticalBatteryState & 0b11111100) >> 1);
            bool keypadCritical = keyTurnerState.accessoryBatteryState != 255 ? ((keyTurnerState.accessoryBatteryState & 1) == 1 ? (keyTurnerState.accessoryBatteryState & 3) == 3 : false) : false;

            if ((_firstTunerStatePublish || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState))
            {
                path = _preferences->getString(preference_ha_path_lock_battery_critical);
                query = _preferences->getString(preference_ha_query_lock_battery_critical);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)critical);
                }

                path = _preferences->getString(preference_ha_path_lock_battery_level);
                query = _preferences->getString(preference_ha_query_lock_battery_level);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)level);
                }

                path = _preferences->getString(preference_ha_path_lock_battery_charging);
                query = _preferences->getString(preference_ha_query_lock_battery_charging);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)charging);
                }
            }

            if ((_firstTunerStatePublish || keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState))
            {
                path = _preferences->getString(preference_ha_path_keypad_critical);
                query = _preferences->getString(preference_ha_query_keypad_critical);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)keypadCritical);
                }
            }

            bool doorSensorCritical = keyTurnerState.accessoryBatteryState != 255 ? ((keyTurnerState.accessoryBatteryState & 4) == 4 ? (keyTurnerState.accessoryBatteryState & 12) == 12 : false) : false;

            if ((_firstTunerStatePublish || keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState))
            {
                path = _preferences->getString(preference_ha_path_doorsensor_critical);
                query = _preferences->getString(preference_ha_query_doorsensor_critical);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)doorSensorCritical);
                }
            }

            path = _preferences->getString(preference_ha_path_remote_access_state);
            query = _preferences->getString(preference_ha_query_remote_access_state);

            if (path && query)
            {
                sendToHAInt(path.c_str(), query.c_str(), (int)keyTurnerState.remoteAccessStatus);
            }

            if (keyTurnerState.bleConnectionStrength != 1)
            {
                path = _preferences->getString(preference_ha_path_ble_strength);
                query = _preferences->getString(preference_ha_query_ble_strength);

                if (path && query)
                {
                    sendToHAInt(path.c_str(), query.c_str(), (int)keyTurnerState.bleConnectionStrength);
                }
            }

            _firstTunerStatePublish = false;
        }
    }
}

void NukiNetwork::sendRequestToHA(const char *path, const char *query, const char *value)
{
    const size_t BUFFER_SIZE = 385;
    char url[BUFFER_SIZE];

    // Build base URL
    snprintf(url, BUFFER_SIZE, "http://");

    // If user name and password are available, add authentication
    if (_homeAutomationUser && _homeAutomationPassword)
    {
        strncat(url, _homeAutomationUser.c_str(), BUFFER_SIZE - strlen(url) - 1);
        strncat(url, ":", BUFFER_SIZE - strlen(url) - 1);
        strncat(url, _homeAutomationPassword.c_str(), BUFFER_SIZE - strlen(url) - 1);
        strncat(url, "@", BUFFER_SIZE - strlen(url) - 1);
    }

    // Add host + port
    strncat(url, _homeAutomationAdress.c_str(), BUFFER_SIZE - strlen(url) - 1);
    if (_homeAutomationPort)
    {
        char portStr[6]; // Max. 5 digits + zero termination
        snprintf(portStr, sizeof(portStr), ":%d", _homeAutomationPort);
        strncat(url, portStr, BUFFER_SIZE - strlen(url) - 1);
    }

    // Add Path, Query & Value (if available)
    if (path && *path)
    {
        strncat(url, "/", BUFFER_SIZE - strlen(url) - 1);
        strncat(url, path, BUFFER_SIZE - strlen(url) - 1);
    }
    if (query && *query)
    {
        strncat(url, "/", BUFFER_SIZE - strlen(url) - 1);
        strncat(url, query, BUFFER_SIZE - strlen(url) - 1);
    }
    if (value && *value)
    {
        strncat(url, value, BUFFER_SIZE - strlen(url) - 1);
    }

    // Send HTTP request
    _httpClient->begin(url);
    int httpCode = _httpClient->GET();

    if (httpCode > 0)
    {
        Log->println(_httpClient->getString());
    }
    else
    {
        Log->printf(F("[ERROR] HTTP request failed: %s\n"), _httpClient->errorToString(httpCode).c_str());
    }

    _httpClient->end();
}

void NukiNetwork::sendResponse(JsonDocument &jsonResult, bool success, int httpCode)
{
    jsonResult[F("success")] = success ? 1 : 0;
    jsonResult[F("error")] = success ? 0 : httpCode;

    serializeJson(jsonResult, _buffer, _bufferSize);
    _server->send(httpCode, F("application/json"), _buffer);
}

void NukiNetwork::sendResponse(const char *jsonResultStr)
{
    _server->send(200, F("application/json"), jsonResultStr);
}

void NukiNetwork::readSettings()
{
    _disableNetworkIfNotConnected = _preferences->getBool(preference_disable_network_not_connected, false);
    _restartOnDisconnect = _preferences->getBool(preference_restart_on_disconnect, false);
    _rssiSendInterval = _preferences->getInt(preference_rssi_send_interval, 0) * 1000;
    _MaintenanceSendIntervall = _preferences->getInt(preference_Maintenance_send_interval, 0) * 1000;

    if (_rssiSendInterval == 0)
    {
        _rssiSendInterval = 60000;
        _preferences->putInt(preference_rssi_send_interval, 60);
    }

    _networkTimeout = _preferences->getInt(preference_network_timeout, 0);
    if (_networkTimeout == 0)
    {
        _networkTimeout = -1;
        _preferences->putInt(preference_network_timeout, _networkTimeout);
    }
    _publishDebugInfo = _preferences->getBool(preference_send_debug_info, false);
}

// -----------------------------------------------------------------------------
//  PRIVATE METHODEN
// -----------------------------------------------------------------------------

void NukiNetwork::initializeWiFi()
{
    _WiFissid = _preferences->getString(preference_wifi_ssid, "");
    _WiFissid.trim();
    _WiFipass = _preferences->getString(preference_wifi_pass, "");
    _WiFipass.trim();
    WiFi.setHostname(_hostname.c_str());

    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info)
                 { this->onNetworkEvent(event, info); });

    if (isWifiConfigured())
    {
        Log->println(String("[INFO] Attempting to connect to saved SSID ") + String(_WiFissid));
        _openAP = false;
    }
    else
    {
        Log->println("[INFO] No SSID or Wifi password saved, opening AP");
        _openAP = true;
    }

    scan(false, true);

    // If AP mode has been started, wait until AP is ready
    if (_openAP)
    {
        int retries = 10;

        while (!_APisReady && retries > 0)
        {
            Log->println("[DEBUG] Waiting for AP to be ready...");
            delay(1000);
            retries--;
        }

        if (_APisReady)
        {
            Log->println("[DEBUG] AP is active and ready");
        }
        else
        {
            Log->println("[ERROR] AP did not start correctly!");
        }
    }
    return;
}

void NukiNetwork::initializeEthernet()
{
    delay(250);
    if (ethCriticalFailure)
    {
        ethCriticalFailure = false;
        Log->println(F("[ERROR] Failed to initialize ethernet hardware"));
        Log->println(F("[ERROR] Network device has a critical failure, enable fallback to Wi-Fi and reboot."));
        wifiFallback = true;
        delay(200);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }

    Log->println(F("[INFO] Init Ethernet"));

    ethCriticalFailure = true;
    _hardwareInitialized = ETH.begin();
    ethCriticalFailure = false;

    if (_hardwareInitialized)
    {
        Log->println(F("[INFO] Ethernet hardware Initialized"));
        wifiFallback = false;

        if (!_ipConfiguration->dhcpEnabled())
        {
            ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
        }

        WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info)
                     { this->onNetworkEvent(event, info); });
    }
    else
    {
        Log->println(F("[ERROR] Failed to initialize ethernet hardware"));
        Log->println(F("[ERROR] Network device has a critical failure, enable fallback to Wi-Fi and reboot."));
        wifiFallback = true;
        delay(200);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }
}

void NukiNetwork::startNetworkServices()
{

    _httpClient = new HTTPClient();
    _server = new WebServer(_apiPort);
    if (_server)
    {
        _server->onNotFound([this]()
                            { onRestDataReceivedCallback(this->_server->uri().c_str(), *this->_server); });
        _server->begin();
    }
}

void NukiNetwork::onRestDataReceivedCallback(const char *path, WebServer &server)
{

    if (_inst)
    {
        if (!_inst->_apiEnabled)
            return;

        if ((_inst->_networkServicesState == NetworkServiceStates::WEBSERVER_NOT_REACHABLE) || (_inst->_networkServicesState == NetworkServiceStates::BOTH_NOT_REACHABLE))
        {
            return;
        }

        if (!server.hasArg("token") || server.arg("token") != _inst->_apitoken->get())
        {
            server.send(401, F("text/html"), "");
            return;
        }

        _inst->onRestDataReceived(path, server);
    }
}

char *NukiNetwork::getArgs(WebServer &server)
{
    JsonDocument doc;

    if (server.args() == 1 && server.argName(0) == "val")
    {
        // If only one parameter with the name "val" exists, save only the value
        strncpy(_buffer, server.arg(0).c_str(), _bufferSize - 1);
    }
    else if (server.args() == 1)
    {
        // If only one parameter exists, save only the name
        strncpy(_buffer, server.argName(0).c_str(), _bufferSize - 1);
    }
    else if (server.args() > 1)
    {
        for (uint8_t i = 0; i < server.args(); i++)
        {
            doc[server.argName(i)] = server.arg(i); // Add parameters to the JSON document
        }
        // Serialization of the JSON to _buffer
        serializeJson(doc, _buffer, _bufferSize);
    }
    else
    {
        _buffer[0] = '\0';
    }

    return _buffer; // Returns the global buffer as char*
}

void NukiNetwork::onRestDataReceived(const char *path, WebServer &server)
{
    JsonDocument json;

    char *data = getArgs(server);

    // Bridge Rest API Requests
    if (comparePrefixedPath(path, api_path_bridge_enable_api))
    {
        if (!data || !*data)
        {
            json[F("result")] = "missing data";
            sendResponse(json, false, 400);
            return;
        }

        if (atoi(data) == 0)
        {
            Log->println(F("[INFO] Disable REST API"));
            _apiEnabled = false;
        }
        else
        {
            Log->println(F("[INFO] ]Enable REST API"));
            _apiEnabled = true;
        }
        _preferences->putBool(preference_api_enabled, _apiEnabled);
        sendResponse(json);
    }
    else if (comparePrefixedPath(path, api_path_bridge_reboot))
    {
        Log->println(F("[INFO] Reboot requested via REST API"));
        delay(200);
        sendResponse(json);
        delay(500);
        restartEsp(RestartReason::RequestedViaApi);
    }
    else if (comparePrefixedPath(path, api_path_bridge_enable_web_server))
    {
        if (!data || !*data)
        {
            json[F("result")] = "missing data";
            sendResponse(json, false, 400);
            return;
        }

        if (atoi(data) == 0)
        {
            if (!_preferences->getBool(preference_webcfgserver_enabled, true) && !forceEnableWebCfgServer)
            {
                return;
            }
            Log->println(F("[INFO] Disable Config Web Server, restarting"));
            _preferences->putBool(preference_webcfgserver_enabled, false);
        }
        else
        {
            if (_preferences->getBool(preference_webcfgserver_enabled, true) || forceEnableWebCfgServer)
            {
                return;
            }
            Log->println(F("[INFO] Enable Config Web Server, restarting"));
            _preferences->putBool(preference_webcfgserver_enabled, true);
        }
        sendResponse(json);

        clearWifiFallback();
        delay(200);
        restartEsp(RestartReason::ReconfigureWebCfgServer);

        // "Lock" Rest API Requests
    }
    else if (_lockEnabled)
    {
        if (comparePrefixedPath(path, api_path_lock_action))
        {

            if (!data || !*data)
            {
                json[F("result")] = "missing data";
                sendResponse(json, false, 400);
            }
            return;

            Log->println(F("[INFO] Lock action received: "));
            Log->printf(F("[INFO] %s\n"), data);

            LockActionResult lockActionResult = LockActionResult::Failed;
            if (_lockActionReceivedCallback != NULL)
            {
                lockActionResult = _lockActionReceivedCallback(data);
            }

            switch (lockActionResult)
            {
            case LockActionResult::Success:
                sendResponse(json);
                break;
            case LockActionResult::UnknownAction:
                json[F("result")] = "unknown_action";
                sendResponse(json, false, 404);
                break;
            case LockActionResult::AccessDenied:
                json[F("result")] = "denied";
                sendResponse(json, false, 403);
                break;
            case LockActionResult::Failed:
                json[F("result")] = "error";
                sendResponse(json, false, 500);
                break;
            }
            return;
        }

        if (comparePrefixedPath(path, api_path_keypad_command_action))
        {
            if (_keypadCommandReceivedReceivedCallback != nullptr)
            {
                if (!data || !*data)
                {
                    json[F("result")] = "missing data";
                    sendResponse(json, false, 400);
                    return;
                }

                _keypadCommandReceivedReceivedCallback(data, _keypadCommandId, _keypadCommandName, _keypadCommandCode, _keypadCommandEnabled);

                _keypadCommandId = 0;
                _keypadCommandName = "";
                _keypadCommandCode = "000000";
                _keypadCommandEnabled = 1;

                return;
            }
        }
        else if (comparePrefixedPath(path, api_path_keypad_command_id))
        {
            _keypadCommandId = atoi(data);
            return;
        }
        else if (comparePrefixedPath(path, api_path_keypad_command_name))
        {
            _keypadCommandName = data;
            return;
        }
        else if (comparePrefixedPath(path, api_path_keypad_command_code))
        {
            _keypadCommandCode = data;
            return;
        }
        else if (comparePrefixedPath(path, api_path_keypad_command_enabled))
        {
            _keypadCommandEnabled = atoi(data);
            return;
        }

        bool queryCmdSet = false;
        if (strcmp(data, "1") == 0)
        {
            if (comparePrefixedPath(path, api_path_query_config))
            {
                _queryCommands = _queryCommands | QUERY_COMMAND_CONFIG;
                queryCmdSet = true;
            }
            else if (comparePrefixedPath(path, api_path_query_lockstate))
            {
                _queryCommands = _queryCommands | QUERY_COMMAND_LOCKSTATE;
                queryCmdSet = true;
            }
            else if (comparePrefixedPath(path, api_path_query_keypad))
            {
                _queryCommands = _queryCommands | QUERY_COMMAND_KEYPAD;
                queryCmdSet = true;
            }
            else if (comparePrefixedPath(path, api_path_query_battery))
            {
                _queryCommands = _queryCommands | QUERY_COMMAND_BATTERY;
                queryCmdSet = true;
            }
            if (queryCmdSet)
            {
                sendResponse(json);
                return;
            }
        }

        if (comparePrefixedPath(path, api_path_config_action))
        {

            if (!data || !*data)
            {
                json[F("result")] = "missing data";
                sendResponse(json, false, 400);
                return;
            }

            if (_configUpdateReceivedCallback != NULL)
            {
                _configUpdateReceivedCallback(data);
            }
            return;
        }

        if (comparePrefixedPath(path, api_path_timecontrol_action))
        {
            if (!data || !*data)
            {
                json[F("result")] = "missing data";
                sendResponse(json, false, 400);
                return;
            }

            if (_timeControlCommandReceivedReceivedCallback != NULL)
            {
                _timeControlCommandReceivedReceivedCallback(data);
            }
            return;
        }

        if (comparePrefixedPath(path, api_path_auth_action))
        {
            if (!data || !*data)
            {
                json[F("result")] = "missing data";
                sendResponse(json, false, 400);
                return;
            }

            if (_authCommandReceivedReceivedCallback != NULL)
            {
                _authCommandReceivedReceivedCallback(data);
            }
            return;
        }
    }
}

NetworkServiceStates NukiNetwork::testNetworkServices()
{
    bool httpClientOk = true;
    bool webServerOk = true;

    // 1. check whether _httpClient exists
    if (_httpClient == nullptr)
    {
        Log->println(F("[ERROR] _httpClient is NULL!"));
        httpClientOk = false;
    }
    if (_homeAutomationEnabled)
    {
        // 2. ping test for _homeAutomationAdress
        if (!_homeAutomationAdress.isEmpty() && httpClientOk)
        {
            if (!Ping.ping(_homeAutomationAdress.c_str()))
            {
                Log->println(F("[ERROR] Ping to Home Automation Server failed!"));
                httpClientOk = false;
            }
            else
            {
                Log->println(F("[DEBUG] Ping to Home Automation Server successful."));
            }
        }

        // 3. if Home Automation API path exists, execute GET request
        String strPath = _preferences->getString(preference_ha_path_state, "");
        if (!strPath.isEmpty() && httpClientOk)
        {
            String url = "http://" + _homeAutomationAdress + ":" + String(_homeAutomationPort) + "/" + strPath;
            Log->println("[DEBUG] Performing GET request to: " + url);

            HTTPClient http;
            http.begin(url);
            int httpCode = http.GET();
            http.end();

            if (httpCode > 0)
            {
                Log->println("[DEBUG] HTTP GET successful, response code: " + String(httpCode));
            }
            else
            {
                Log->println(F("[ERROR] HTTP GET failed!"));
                httpClientOk = false;
            }
        }
    }

    // 4. check whether Rest Server (_server) exists
    if (_server == nullptr)
    {
        Log->println(F("[ERROR] _server is NULL!"));
        webServerOk = false;
    }

    // 5. test whether the local REST web server can be reached on the port
    WiFiClient client;
    if (!client.connect(WiFi.localIP(), _apiPort))
    {
        Log->println(F("[ERROR] WebServer is not responding!"));
        webServerOk = false;
    }
    else
    {
        Log->println(F("[DEBUG] WebServer is responding."));
        client.stop();
    }

    // 6. return error code
    if (webServerOk && httpClientOk)
        return NetworkServiceStates::OK; // all OK
    if (!webServerOk && httpClientOk)
        return NetworkServiceStates::WEBSERVER_NOT_REACHABLE; // _server not reachable
    if (webServerOk && !httpClientOk)
        return NetworkServiceStates::HTTPCLIENT_NOT_REACHABLE; // _httpClient / Home Automation not reachable
    return NetworkServiceStates::BOTH_NOT_REACHABLE;           // Both _server and _httpClient not reachable
}

void NukiNetwork::restartNetworkServices(NetworkServiceStates status)
{
    if (status == NetworkServiceStates::UNDEFINED)
    {
        status = testNetworkServices();
    }

    if (status == NetworkServiceStates::OK)
    {
        Log->println(F("[DEBUG] Network services are running."));
        return; // No restart required
    }

    // If _httpClient is not reachable (-2 or -3), reinitialize
    if (status == NetworkServiceStates::HTTPCLIENT_NOT_REACHABLE || status == NetworkServiceStates::BOTH_NOT_REACHABLE)
    {
        Log->println(F("[INFO] Reinitialization of HTTP client..."));
        if (_httpClient)
        {
            delete _httpClient;
            _httpClient = nullptr;
        }
        _httpClient = new HTTPClient();
        if (_httpClient)
        {
            Log->println(F("[INFO] HTTP client successfully reinitialized."));
        }
        else
        {
            Log->println(F("[ERROR] HTTP client cannot be initialized."));
        }
    }

    // If the REST web server cannot be reached (-1 or -3), restart it
    if (status == NetworkServiceStates::WEBSERVER_NOT_REACHABLE || status == NetworkServiceStates::BOTH_NOT_REACHABLE)
    {
        Log->println(F("[INFO] Restarting the REST WebServer..."));
        if (_server)
        {
            _server->stop();
            delete _server;
            _server = nullptr;
        }
        _server = new WebServer(_apiPort);
        if (_server)
        {
            _server->onNotFound([this]()
                                { onRestDataReceivedCallback(this->_server->uri().c_str(), *this->_server); });
            _server->begin();
            Log->println(F("[INFO] REST WebServer successfully restarted."));
        }
        else
        {
            Log->println(F("[ERROR] REST Web Server cannot be initialized."));
        }
    }

    Log->println(F("[DEBUG] Network services have been checked and reinit/restarted if necessary."));
}

void NukiNetwork::onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    Log->printf("[DEBUG] (LAN Event) event: %d\n", event);

    switch (event)
    {
    // --- Ethernet Events ---
    case ARDUINO_EVENT_ETH_START:
        Log->println(F("[DEBUG] ETH Started"));
        ETH.setHostname(_hostname.c_str());
        break;

    case ARDUINO_EVENT_ETH_CONNECTED:
        Log->println(F("[INFO] ETH Connected"));
        if (!localIP().equals("0.0.0.0"))
        {
            _connected = true;
        }
        break;

    case ARDUINO_EVENT_ETH_GOT_IP:
        Log->printf("[DEBUG] ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif));
        Log->println(ETH.localIP().toString());

        _connected = true;
        if (_preferences->getBool(preference_ntw_reconfigure, false))
        {
            _preferences->putBool(preference_ntw_reconfigure, false);
        }
        break;

    case ARDUINO_EVENT_ETH_LOST_IP:
        Log->println(F("[WARNING] ETH Lost IP"));
        _connected = false;
        onDisconnected();
        break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Log->println(F("[WARNING] ETH Disconnected"));
        _connected = false;
        onDisconnected();
        break;

    case ARDUINO_EVENT_ETH_STOP:
        Log->println(F("[WARNING] ETH Stopped"));
        _connected = false;
        onDisconnected();
        break;

    // --- WiFi Events ---
    case ARDUINO_EVENT_WIFI_READY:
        Log->println(F("[DEBUG] WiFi interface ready"));
        break;

    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        Log->println(F("[DEBUG] Completed scan for access points"));
        _foundNetworks = WiFi.scanComplete();

        for (int i = 0; i < _foundNetworks; i++)
        {
            Log->println("[DEBUG] " + String("SSID ") + WiFi.SSID(i) + String(" found with RSSI: ") + String(WiFi.RSSI(i)) + String(("(")) + String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) + String(" %) and BSSID: ") + WiFi.BSSIDstr(i) + String(" and channel: ") + String(WiFi.channel(i)));
        }

        if (_openAP)
        {
            openAP();
        }
        else if (_foundNetworks > 0 || _preferences->getBool(preference_find_best_rssi, false))
        {
            esp_wifi_scan_stop();
            connect();
        }
        else
        {
            Log->println(F("[DEBUG] No networks found, restarting scan"));
            scan(false, true);
        }
        break;

    case ARDUINO_EVENT_WIFI_STA_START:
        Log->println(F("[DEBUG] WiFi client started"));
        break;

    case ARDUINO_EVENT_WIFI_STA_STOP:
        Log->println(F("[DEBUG] WiFi clients stopped"));
        if (!_openAP)
        {
            onDisconnected();
        }
        break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Log->println(F("[DEBUG] Connected to access point"));
        if (!_openAP)
        {
            onConnected();
        }
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Log->println(F("[DEBUG] Disconnected from WiFi access point"));
        if (!_openAP)
        {
            onDisconnected();
        }
        break;

    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
        Log->println(F("[DEBUG] Authentication mode of access point has changed"));
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Log->print(F("[DEBUG] Obtained IP address: "));
        Log->println(WiFi.localIP());
        if (!_openAP)
        {
            onConnected();
        }
        break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        Log->println(F("[WARNING] Lost IP address and IP address is reset to 0"));
        if (!_openAP)
        {
            onDisconnected();
        }
        break;

    case ARDUINO_EVENT_WIFI_AP_START:
        Log->println(F("[DEBUG] WiFi access point started"));
        _APisReady = true;
        break;

    case ARDUINO_EVENT_WIFI_AP_STOP:
        Log->println(F("[DEBUG] WiFi access point stopped"));
        _APisReady = false;
        break;

    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Log->println(F("[DEBUG] Client connected"));
        break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Log->println(F("[DEBUG] Client disconnected"));
        break;

    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
        Log->println(F("[DEBUG] Assigned IP address to client"));
        break;

    case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
        Log->println(F("[DEBUG] Received probe request"));
        break;

    case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
        Log->println(F("[DEBUG] AP IPv6 is preferred"));
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
        Log->println(F("[DEBUG] STA IPv6 is preferred"));
        break;

    default:
        Log->print(F("[DEBUG] Unknown LAN Event: "));
        Log->println(event);
        break;
    }
}

void NukiNetwork::onConnected()
{
    if (_networkDeviceType == NetworkDeviceType::WiFi)
    {
        Log->println(F("[INFO] Wi-Fi connected"));
        _connected = true;
    }
}

bool NukiNetwork::connect()
{
    if (_networkDeviceType == NetworkDeviceType::WiFi)
    {
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(_hostname.c_str());
        delay(500);

        int bestConnection = -1;

        if (_preferences->getBool(preference_find_best_rssi, false))
        {
            for (int i = 0; i < _foundNetworks; i++)
            {
                if (_WiFissid == WiFi.SSID(i))
                {
                    Log->println("[INFO] " + String("Saved SSID ") + _WiFissid + String(" found with RSSI: ") + String(WiFi.RSSI(i)) + String(("(")) + String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) + String(" %) and BSSID: ") + WiFi.BSSIDstr(i) + String(" and channel: ") + String(WiFi.channel(i)));
                    if (bestConnection == -1)
                    {
                        bestConnection = i;
                    }
                    else
                    {
                        if (WiFi.RSSI(i) > WiFi.RSSI(bestConnection))
                        {
                            bestConnection = i;
                        }
                    }
                }
            }

            if (bestConnection == -1)
            {
                Log->print(F("[WARNING] No network found with SSID: "));
                Log->println(_WiFissid);
            }
            else
            {
                Log->println("[INFO] " + String("Trying to connect to SSID ") + _WiFissid + String(" found with RSSI: ") + String(WiFi.RSSI(bestConnection)) + String(("(")) + String(constrain((100.0 + WiFi.RSSI(bestConnection)) * 2, 0, 100)) + String(" %) and BSSID: ") + WiFi.BSSIDstr(bestConnection) + String(" and channel: ") + String(WiFi.channel(bestConnection)));
            }
        }

        if (!_ipConfiguration->dhcpEnabled())
        {
            WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
        }

        WiFi.begin(_WiFissid, _WiFipass);

        Log->print("[DEBUG] WiFi connecting");
        int loop = 0;
        while (!isConnected() && loop < 150)
        {
            Log->print(".");
            delay(100);
            loop++;
        }
        Log->println("");

        if (!isConnected())
        {
            Log->println(F("[ERROR] Failed to connect within 15 seconds"));

            if (_preferences->getBool(preference_restart_on_disconnect, false) && (espMillis() > 60000))
            {
                Log->println(F("[INFO] Restart on disconnect watchdog triggered, rebooting"));
                delay(100);
                restartEsp(RestartReason::RestartOnDisconnectWatchdog);
            }
            else
            {
                Log->println(F("[INFO] Retrying WiFi connection"));
                scan(false, true);
            }

            return false;
        }

        return true;
    }

    return false;
}

void NukiNetwork::openAP()
{
    if (_startAP)
    {
        Log->println(F("[INFO] Starting AP with SSID NukiBridge and Password NukiBridgeESP32"));
        _startAP = false;
        WiFi.mode(WIFI_AP);
        delay(500);
        WiFi.softAPsetHostname(_hostname.c_str());
        delay(500);
        WiFi.softAP(F("NukiBridge"), F("NukiBridgeESP32"));
    }
}

void NukiNetwork::onDisconnected()
{
    switch (_networkDeviceType)
    {
    case NetworkDeviceType::WiFi:
        if (!_connected)
        {
            return;
        }
        _connected = false;

        Log->println(F("[INFO] Wi-Fi disconnected"));
        connect();
        break;
    case NetworkDeviceType::ETH:
        if (_preferences->getBool(preference_restart_on_disconnect, false) && (espMillis() > 60000))
        {
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
        break;
    }
}

bool NukiNetwork::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[385];
    buildApiPath(subPath, prefixedPath);
    return strcmp(fullPath, prefixedPath) == 0;
}

void NukiNetwork::buildApiPath(const char *path, char *outPath)
{
    // Copy (_apiBridgePath) to outPath
    strncpy(outPath, _apiBridgePath, sizeof(_apiBridgePath) - 1);

    // Append the (path) zo outPath
    strncat(outPath, path, 384 - strlen(outPath)); // Sicherheitsgrenze beachten
}

void NukiNetwork::assignNewApiToken()
{
    _apitoken->assignNewToken();
}

char *NukiNetwork::getApiToken()
{

    return _apitoken->get();
}
