#include "NukiNetwork.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include "RestartReason.h"
#include "hal/wdt_hal.h"

NukiNetwork *NukiNetwork::_inst = nullptr;

NukiNetwork::NukiNetwork(Preferences *preferences, char *buffer, size_t bufferSize, ImportExport* importExport)
    : _preferences(preferences),
      _buffer(buffer),
      _bufferSize(bufferSize),
      _importExport(importExport)
{
    _inst = this;
    _webCfgEnabled = _preferences->getBool(preference_webcfgserver_enabled, true);
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

    if (_udpClient)
    {
        delete _udpClient;
        _udpClient = nullptr;
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
            Log->disableFileLog();
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
        _apiPort = _preferences->getInt(preference_api_port, REST_SERVER_PORT);
        _apitoken = new BridgeApiToken(_preferences, preference_api_token);
        _apiEnabled = _preferences->getBool(preference_api_enabled);
        
        _homeAutomationEnabled = _preferences->getBool(preference_har_enabled, false);
        _homeAutomationAdress = _preferences->getString(preference_har_address, "");
        _homeAutomationPort = _preferences->getInt(preference_har_port, 0);
        _homeAutomationMode = _preferences->getInt(preference_har_mode, 0);          // 0=UDP, 1=REST
        _homeAutomationRestMode = _preferences->getInt(preference_har_rest_mode, 0); // 0=GET, 1=POST

        _hostname = _preferences->getString(preference_hostname, "");

        if (_hostname == "")
        {
            uint8_t mac[6];
            esp_efuse_mac_get_default(mac);

            char deviceId[13];
            sprintf(deviceId, "%02X%02X%02X%02X%02X%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            _hostname = "NRB" + String(deviceId);
            _preferences->putString(preference_hostname, _hostname);
        }

        _homeAutomationPort = _preferences->getInt(preference_har_port, 0);

        if (_homeAutomationPort == 0)
        {
            _homeAutomationPort = 80;
            _preferences->putInt(preference_har_port, _homeAutomationPort);
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

        Log->print(F("[DEBUG] Host name: "));
        Log->println(_hostname);

        String _homeAutomationUser = _preferences->getString(preference_har_user);

        String _homeAutomationPass = _preferences->getString(preference_har_password);

        readSettings();

        // Give the network time to get an IP
        unsigned long startMillis;
        startMillis = millis();
        // Wait until there is a connection or 10 seconds have elapsed
        while (!isConnected() && (millis() - startMillis < 10000))
        {
            yield();
        }

        startNetworkServices();
        _networkServicesState = testNetworkServices();
    }
}

bool NukiNetwork::update()
{

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
                Log->println(F("[DEBUG] ETH Set static IP"));
                ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
                _checkIpTs = espMillis() + 5000;
            }

            _checkIpTs = -1;
        }
        break;
    }

    if (disableNetwork || (!_homeAutomationEnabled && !_apiEnabled) || isApOpen())
    {
        return false;
    }

    if (!isConnected() || (_networkServicesConnectCounter > 15))
    {
        _networkServicesConnectCounter = 0;

        if (_restartOnDisconnect && espMillis() > 60000)
        {
            Log->disableFileLog();
            delay(10);
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
    }

    if (isConnected() && (_apiEnabled || _homeAutomationEnabled))
    {
        if (ts - _lastNetworkServiceTs > 30000)
        { // test all 30 seconds
            _lastNetworkServiceTs = ts;
            _networkServicesState = testNetworkServices();

            bool svcBothDown = _homeAutomationEnabled && _apiEnabled && _networkServicesState != NetworkServiceState::OK;
            bool svcHADown = !_apiEnabled && _networkServicesState != NetworkServiceState::ERROR_REST_API_SERVER;
            bool svcAPIDown = !_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT;

            if (svcBothDown || svcHADown || svcAPIDown)
            { // error in network Services
                restartNetworkServices(_networkServicesState);
                delay(1000);
                _networkServicesState = testNetworkServices(); // test network services again

                bool expectedStateOk =
                    (_homeAutomationEnabled && _apiEnabled && _networkServicesState == NetworkServiceState::OK) ||
                    (_homeAutomationEnabled && !_apiEnabled && _networkServicesState == NetworkServiceState::ERROR_REST_API_SERVER) ||
                    (!_homeAutomationEnabled && _apiEnabled && _networkServicesState == NetworkServiceState::ERROR_HAR_CLIENT);

                if (!expectedStateOk)
                {
                    _networkServicesConnectCounter++;
                    return false;
                }
            }
        }

        _networkServicesConnectCounter = 0;
        if (forceEnableWebCfgServer && !_webCfgEnabled)
        {
            forceEnableWebCfgServer = false;
            Log->disableFileLog();
            delay(200);
            restartEsp(RestartReason::ReconfigureWebCfgServer);
        }
        else if (!_webCfgEnabled)
        {
            forceEnableWebCfgServer = false;
        }
        delay(2000);
    }

    if (_networkServicesState != NetworkServiceState::OK || !isConnected())
    {
        if (_networkTimeout > 0 && (ts - _lastConnectedTs > _networkTimeout * 1000) && ts > 60000)
        {
            if (!_webCfgEnabled)
            {
                forceEnableWebCfgServer = true;
            }
            Log->println(F("[WARNING] Networkservice timeout has been reached, restarting ..."));
            Log->disableFileLog();
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
            String key = _preferences->getString(preference_har_key_wifi_rssi);
            String param = _preferences->getString(preference_har_param_wifi_rssi);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                sendToHAInt(key.c_str(), param.c_str(), signalStrength());
            _lastRssi = rssi;
        }
    }

    if (_homeAutomationEnabled && (_lastMaintenanceTs == 0 || (ts - _lastMaintenanceTs) > _MaintenanceSendIntervall))
    {
        int64_t curUptime = ts / 1000 / 60;
        if (curUptime > _publishedUpTime)
        {
            String key = _preferences->getString(preference_har_key_uptime);
            String param = _preferences->getString(preference_har_param_uptime);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                sendToHAULong(key.c_str(), param.c_str(), curUptime);
            _publishedUpTime = curUptime;
        }

        if (_lastMaintenanceTs == 0)
        {
            String key = _preferences->getString(preference_har_key_restart_reason_fw);
            String param = _preferences->getString(preference_har_param_restart_reason_fw);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                sendToHAString(key.c_str(), param.c_str(), getRestartReason().c_str());

            key = _preferences->getString(preference_har_key_restart_reason_esp);
            param = _preferences->getString(preference_har_param_restart_reason_esp);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                sendToHAString(key.c_str(), param.c_str(), getEspRestartReason().c_str());

            key = _preferences->getString(preference_har_key_info_nuki_bridge_version);
            param = _preferences->getString(preference_har_param_info_nuki_bridge_version);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                sendToHAString(key.c_str(), param.c_str(), NUKI_REST_BRIDGE_VERSION);

            key = _preferences->getString(preference_har_key_info_nuki_bridge_build);
            param = _preferences->getString(preference_har_param_info_nuki_bridge_build);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                sendToHAString(key.c_str(), param.c_str(), NUKI_REST_BRIDGE_BUILD);
        }
        if (_sendDebugInfo)
        {
            String key = _preferences->getString(preference_har_key_freeheap);
            String param = _preferences->getString(preference_har_param_freeheap);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                sendToHAUInt(key.c_str(), param.c_str(), esp_get_free_heap_size());
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
        Log->disableFileLog();
        delay(200);
        restartEsp(RestartReason::ReconfigureWifi);
        break;
    case NetworkDeviceType::ETH:
        Log->disableFileLog();
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
    return (_networkDeviceType == NetworkDeviceType::ETH ? ETH.localIP().toString() : (WiFi.getMode() & WIFI_AP) ? WiFi.softAPIP().toString()
                                                                                                                 : WiFi.localIP().toString());
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

NetworkServiceState NukiNetwork::networkServicesState()
{
    return _networkServicesState;
}

uint8_t NukiNetwork::queryCommands()
{
    uint8_t qc = _queryCommands;
    _queryCommands = 0;
    return qc;
}

void NukiNetwork::sendToHAFloat(const char *path, const char *param, const float value, uint8_t precision)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        char buffer[30];
        dtostrf(value, 0, precision, buffer);
        sendDataToHA(path, param, buffer);
    }
}

void NukiNetwork::sendToHAInt(const char *path, const char *param, const int value)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        char buffer[30];
        itoa(value, buffer, 10);
        sendDataToHA(path, param, buffer);
    }
}

void NukiNetwork::sendToHAUInt(const char *path, const char *param, const unsigned int value)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        char buffer[30];
        utoa(value, buffer, 10);
        sendDataToHA(path, param, buffer);
    }
}

void NukiNetwork::sendToHAULong(const char *path, const char *param, const unsigned long value)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        char buffer[30];
        ultoa(value, buffer, 10);
        sendDataToHA(path, param, buffer);
    }
}

void NukiNetwork::sendToHALongLong(const char *path, const char *param, const int64_t value)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        char buffer[30];
        lltoa(value, buffer, 10);
        sendDataToHA(path, param, buffer);
    }
}

void NukiNetwork::sendToHABool(const char *path, const char *param, const bool value)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        char buffer[2] = {0};
        buffer[0] = value ? '1' : '0';
        sendDataToHA(path, param, buffer);
    }
}

void NukiNetwork::sendToHAString(const char *path, const char *param, const char *value)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        sendDataToHA(path, param, value);
    }
}

void NukiNetwork::sendToHALockBleAddress(const std::string &address)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {

        String key = _preferences->getString(preference_har_key_ble_address);
        String param = _preferences->getString(preference_har_param_ble_address);

        if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
            sendDataToHA(key.c_str(), param.c_str(), address.c_str());
    }
}

void NukiNetwork::sendToHABatteryReport(const NukiLock::BatteryReport &batteryReport)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        String key;
        key.reserve(384);
        String param;
        param.reserve(128);

        key = _preferences->getString(preference_har_key_battery_voltage);
        param = _preferences->getString(preference_har_param_battery_voltage);
        if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
        {
            sendToHAFloat(key.c_str(), param.c_str(), (float)batteryReport.batteryVoltage / 1000.0, true);
        }
        key = _preferences->getString(preference_har_key_battery_drain);
        param = _preferences->getString(preference_har_param_battery_drain);
        if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
        {
            sendToHAFloat(key.c_str(), param.c_str(), batteryReport.batteryDrain, true); // milliwatt seconds
        }
        key = _preferences->getString(preference_har_key_battery_max_turn_current);
        param = _preferences->getString(preference_har_param_battery_max_turn_current);
        if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
        {
            sendToHAFloat(key.c_str(), param.c_str(), (float)batteryReport.maxTurnCurrent / 1000.0, true);
        }
        key = _preferences->getString(preference_har_key_battery_lock_distance);
        param = _preferences->getString(preference_har_param_battery_lock_distance);
        if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
        {
            sendToHAFloat(key.c_str(), param.c_str(), batteryReport.lockDistance, true); // degrees
        }
    }
}

void NukiNetwork::sendToHABleRssi(const int &rssi)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        String key = _preferences->getString(preference_har_key_ble_rssi);
        String param = _preferences->getString(preference_har_param_ble_rssi);

        if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
            sendToHAInt(key.c_str(), param.c_str(), rssi);
    }
}

void NukiNetwork::sendToHAKeyTurnerState(const NukiLock::KeyTurnerState &keyTurnerState, const NukiLock::KeyTurnerState &lastKeyTurnerState)
{
    if (_homeAutomationEnabled && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT)
    {
        char str[50];
        memset(&str, 0, sizeof(str));

        String key;
        key.reserve(384);
        String param;
        param.reserve(128);

        if (_homeAutomationEnabled)
        {
            lockstateToString(keyTurnerState.lockState, str);

            key = _preferences->getString(preference_har_key_lock_state);
            param = _preferences->getString(preference_har_param_lock_state);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
            {
                sendToHAInt(key.c_str(), param.c_str(), (int)keyTurnerState.lockState);
            }

            key = _preferences->getString(preference_har_key_lockngo_state);
            param = _preferences->getString(preference_har_param_lockngo_state);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
            {
                sendToHAInt(key.c_str(), param.c_str(), (int)(keyTurnerState.lockNgoTimer != 255 ? keyTurnerState.lockNgoTimer : 0));
            }

            memset(&str, 0, sizeof(str));

            triggerToString(keyTurnerState.trigger, str);

            if (_firstTunerStateSent || keyTurnerState.trigger != lastKeyTurnerState.trigger)
            {
                key = _preferences->getString(preference_har_key_lock_trigger);
                param = _preferences->getString(preference_har_param_lock_trigger);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)keyTurnerState.trigger);
                }
            }

            key = _preferences->getString(preference_har_key_lock_night_mode);
            param = _preferences->getString(preference_har_param_lock_night_mode);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
            {
                sendToHAInt(key.c_str(), param.c_str(), (int)(keyTurnerState.nightModeActive != 255 ? keyTurnerState.nightModeActive : 0));
            }

            memset(&str, 0, sizeof(str));
            NukiLock::completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);

            if (_firstTunerStateSent || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
            {
                key = _preferences->getString(preference_har_key_lock_completionStatus);
                param = _preferences->getString(preference_har_param_lock_completionStatus);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)keyTurnerState.lastLockActionCompletionStatus);
                }
            }

            memset(&str, 0, sizeof(str));

            NukiLock::doorSensorStateToString(keyTurnerState.doorSensorState, str);

            if (_firstTunerStateSent || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
            {
                key = _preferences->getString(preference_har_key_doorsensor_state);
                param = _preferences->getString(preference_har_param_doorsensor_state);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)keyTurnerState.doorSensorState);
                }
            }

            bool critical = (keyTurnerState.criticalBatteryState & 1) == 1;
            bool charging = (keyTurnerState.criticalBatteryState & 2) == 2;
            uint8_t level = ((keyTurnerState.criticalBatteryState & 0b11111100) >> 1);
            bool keypadCritical = keyTurnerState.accessoryBatteryState != 255 ? ((keyTurnerState.accessoryBatteryState & 1) == 1 ? (keyTurnerState.accessoryBatteryState & 3) == 3 : false) : false;

            if ((_firstTunerStateSent || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState))
            {
                key = _preferences->getString(preference_har_key_lock_battery_critical);
                param = _preferences->getString(preference_har_param_lock_battery_critical);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)critical);
                }

                key = _preferences->getString(preference_har_key_lock_battery_level);
                param = _preferences->getString(preference_har_param_lock_battery_level);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)level);
                }

                key = _preferences->getString(preference_har_key_lock_battery_charging);
                param = _preferences->getString(preference_har_param_lock_battery_charging);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)charging);
                }
            }

            if ((_firstTunerStateSent || keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState))
            {
                key = _preferences->getString(preference_har_key_keypad_critical);
                param = _preferences->getString(preference_har_param_keypad_critical);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)keypadCritical);
                }
            }

            bool doorSensorCritical = keyTurnerState.accessoryBatteryState != 255 ? ((keyTurnerState.accessoryBatteryState & 4) == 4 ? (keyTurnerState.accessoryBatteryState & 12) == 12 : false) : false;

            if ((_firstTunerStateSent || keyTurnerState.accessoryBatteryState != lastKeyTurnerState.accessoryBatteryState))
            {
                key = _preferences->getString(preference_har_key_doorsensor_critical);
                param = _preferences->getString(preference_har_param_doorsensor_critical);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)doorSensorCritical);
                }
            }

            key = _preferences->getString(preference_har_key_remote_access_state);
            param = _preferences->getString(preference_har_param_remote_access_state);

            if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
            {
                sendToHAInt(key.c_str(), param.c_str(), (int)keyTurnerState.remoteAccessStatus);
            }

            if (keyTurnerState.bleConnectionStrength != 1)
            {
                key = _preferences->getString(preference_har_key_ble_strength);
                param = _preferences->getString(preference_har_param_ble_strength);

                if ((key && _homeAutomationMode == 1) || (param && _homeAutomationMode == 0))
                {
                    sendToHAInt(key.c_str(), param.c_str(), (int)keyTurnerState.bleConnectionStrength);
                }
            }

            _firstTunerStateSent = false;
        }
    }
}

void NukiNetwork::sendDataToHA(const char *key, const char *param, const char *value)
{
    // --- UDP Mode ---
    if (_homeAutomationMode == 0) // UDP
    {
        if (!param || !*param)
            return;
        char message[384];
        snprintf(message, sizeof(message), "%s=%s", param, value ? value : "");

        _udpClient->beginPacket(_homeAutomationAdress.c_str(), _homeAutomationPort);
        _udpClient->write(reinterpret_cast<const uint8_t *>(message), strlen(message));
        _udpClient->endPacket();
        return;
    }

    if (_homeAutomationMode == 1) // REST
    {
        if (!key || !*key)
            return;

        const size_t BUFFER_SIZE = 256;
        char url[BUFFER_SIZE];
        char postData[BUFFER_SIZE];

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

        // Add Path
        strncat(url, "/", BUFFER_SIZE - strlen(url) - 1);
        strncat(url, key, BUFFER_SIZE - strlen(url) - 1);

        int httpCode = -1;

        if (_homeAutomationRestMode == 0) // GET
        {

            if (param && *param)
            {
                strncat(url, "/", BUFFER_SIZE - strlen(url) - 1);
                strncat(url, param, BUFFER_SIZE - strlen(url) - 1);
            }
            if (value && *value)
            {
                strncat(url, value, BUFFER_SIZE - strlen(url) - 1);
            }

            // Send HTTP request
            _httpClient->begin(url);
            _httpClient->addHeader("Content-Type", "application/x-www-form-urlencoded");
            httpCode = _httpClient->GET();
        }
        else // POST
        {
            if (param && *param)
            {
                strncat(postData, param, BUFFER_SIZE - strlen(postData) - 1);
            }
            if (value && *value)
            {
                strncat(postData, value, BUFFER_SIZE - strlen(postData) - 1);
            }

            // Send HTTP request
            _httpClient->begin(url);
            _httpClient->addHeader("Content-Type", "application/x-www-form-urlencoded");
            httpCode = _httpClient->POST(postData);
        }

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
}

void NukiNetwork::sendResponse(JsonDocument &jsonResult, const char *message, int httpCode)
{
    jsonResult[F("code")] = httpCode;
    jsonResult[F("message")] = message;

    serializeJson(jsonResult, _buffer, _bufferSize);
    _server->send(httpCode, F("application/json"), _buffer);
}

void NukiNetwork::sendResponse(const char *jsonResultStr)
{
    _server->send(200, F("application/json"), jsonResultStr);
}

void NukiNetwork::setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *value))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void NukiNetwork::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *value))
{
    _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

void NukiNetwork::setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char *command, const uint &id, const String &name, const String &code, const int &enabled))
{
    _keypadCommandReceivedReceivedCallback = keypadCommandReceivedReceivedCallback;
}

void NukiNetwork::setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char *value))
{
    _timeControlCommandReceivedReceivedCallback = timeControlCommandReceivedReceivedCallback;
}

void NukiNetwork::setAuthCommandReceivedCallback(void (*authCommandReceivedReceivedCallback)(const char *value))
{
    _authCommandReceivedReceivedCallback = authCommandReceivedReceivedCallback;
}

void NukiNetwork::readSettings()
{
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
    _sendDebugInfo = _preferences->getBool(preference_send_debug_info, false);
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
        Log->println("[INFO] Attempting to connect to saved SSID " + String(_WiFissid));
        _openAP = false;
    }
    else
    {
        Log->println(F("[INFO] No SSID or Wifi password saved, opening AP"));
        _openAP = true;
    }

    scan(false, true);

    // If AP mode has been started, wait until AP is ready
    if (_openAP)
    {
        int retries = 10;

        while (!_APisReady && retries > 0)
        {
            Log->println(F("[DEBUG] Waiting for AP to be ready..."));
            delay(1000);
            retries--;
        }

        if (_APisReady)
        {
            Log->println(F("[DEBUG] AP is active and ready"));
        }
        else
        {
            Log->println(F("[ERROR] AP did not start correctly!"));
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
        Log->disableFileLog();
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
        Log->disableFileLog();
        delay(200);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }
}

void NukiNetwork::startNetworkServices()
{
    if (_homeAutomationEnabled)
    {
        Log->println(F("[INFO] start Home Automation Report Service"));

        if (_homeAutomationMode == 1 && _httpClient == nullptr) // REST
            _httpClient = new HTTPClient();
        else if (_homeAutomationMode == 0 && _udpClient == nullptr) // UDP
            _udpClient = new NetworkUDP();
    }

    if (_apiEnabled && localIP() != "0.0.0.0")
    {
        Log->println(F("[INFO] start REST API Server"));
        _server = new WebServer(_apiPort);
        if (_server)
        {
            _server->onNotFound([this]()
                                { onRestDataReceivedCallback(this->_server->uri().c_str(), *this->_server); });
            _server->begin();
            Log->println("[INFO] REST WebServer started on http://" + localIP() + ":" + String(_apiPort));
        }
    }
}

void NukiNetwork::onRestDataReceivedCallback(const char *path, WebServer &server)
{

    if (_inst)
    {
        if (!_inst->_apiEnabled)
            return;

        if ((_inst->_networkServicesState == NetworkServiceState::ERROR_REST_API_SERVER) || (_inst->_networkServicesState == NetworkServiceState::ERROR_BOTH))
        {
            return;
        }

        if (_inst->comparePrefixedPath(path, api_path_bridge, api_path_shutdown))
        {
            _inst->onShutdownReceived(path, server);
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

    if (server.args() == 2 && server.hasArg("val"))
    {
        // If there are only two arguments and one has the name "val",
        // only the value of "val" is saved (the other argument is always "token")
        strncpy(_buffer, server.arg(0).c_str(), _bufferSize - 1);
    }
    else if (server.args() == 2)
    {
        // If there are only two arguments and one does not have the name "val",
        // only the name of the argument is saved (the other argument always has the name "token")
        strncpy(_buffer, server.argName(0).c_str(), _bufferSize - 1);
    }
    // If there are more than two arguments, all arguments are returned as a json string, except "token"
    else if (server.args() > 2)
    {
        for (uint8_t i = 0; i < server.args(); i++)
        {
            if (server.argName(i) != "token")
                ;
            doc[server.argName(i)] = server.arg(i); // Add argument to the JSON document
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
    JsonDocument jsonResult;

    char *data = getArgs(server);

    // Bridge Rest API Requests
    if (comparePrefixedPath(path, api_path_bridge, api_path_disable_api))
    {

        Log->println(F("[INFO] (REST API) Disable REST API"));
        _apiEnabled = false;
        _preferences->putBool(preference_api_enabled, _apiEnabled);
        sendResponse(jsonResult);
    }
    else if (comparePrefixedPath(path, api_path_bridge, api_path_reboot))
    {
        Log->println(F("[INFO] (REST API) Reboot requested"));
        delay(200);
        sendResponse(jsonResult);
        Log->disableFileLog();
        delay(500);
        restartEsp(RestartReason::RequestedViaApi);
    }
    else if (comparePrefixedPath(path, api_path_bridge, api_path_enable_web_server))
    {
        if (!data || !*data)
        {
            sendResponse(jsonResult, "missing data", 400);
            return;
        }

        if (atoi(data) == 0)
        {
            if (!_preferences->getBool(preference_webcfgserver_enabled, true) && !forceEnableWebCfgServer)
            {
                return;
            }
            Log->println(F("[INFO] (REST API) Disable Config Web Server, restarting"));
            _preferences->putBool(preference_webcfgserver_enabled, false);
        }
        else
        {
            if (_preferences->getBool(preference_webcfgserver_enabled, true) || forceEnableWebCfgServer)
            {
                return;
            }
            Log->println(F("[INFO] (REST API) Enable Config Web Server, restarting"));
            _preferences->putBool(preference_webcfgserver_enabled, true);
        }
        sendResponse(jsonResult);

        clearWifiFallback();
        Log->disableFileLog();
        delay(200);
        restartEsp(RestartReason::ReconfigureWebCfgServer);

        // "Lock" Rest API Requests
    }
    else if (_lockEnabled)
    {
        if (comparePrefixedPath(path, api_path_lock, api_path_action))
        {

            if (!data || !*data)
            {
                sendResponse(jsonResult, "missing data", 400);
            }
            return;

            Log->println(F("[INFO] (REST API) Lock action received: "));
            Log->printf(F("[INFO] %s\n"), data);

            LockActionResult lockActionResult = LockActionResult::Failed;
            if (_lockActionReceivedCallback != NULL)
            {
                lockActionResult = _lockActionReceivedCallback(data);
            }

            switch (lockActionResult)
            {
            case LockActionResult::Success:
                sendResponse(jsonResult);
                break;
            case LockActionResult::UnknownAction:
                sendResponse(jsonResult, "unknown_action", 404);
                break;
            case LockActionResult::AccessDenied:
                sendResponse(jsonResult, "denied", 403);
                break;
            case LockActionResult::Failed:
                sendResponse(jsonResult, "error", 500);
                break;
            }
            return;
        }

        if (comparePrefixedPath(path, api_path_lock, api_path_keypad_command))
        {
            if (_keypadCommandReceivedReceivedCallback != nullptr)
            {
                if (!data || !*data)
                {
                    sendResponse(jsonResult, "missing data", 400);
                    return;
                }

                JsonDocument json;

                DeserializationError jsonError = deserializeJson(json, data);

                if (jsonError)
                {
                    sendResponse(jsonResult, "invalid data", 400);
                    return;
                }

                const char *command = json.containsKey("command") ? json["command"].as<const char *>() : nullptr;
                _keypadCommandId = json.containsKey("id") ? json["id"].as<unsigned int>() : 0;
                _keypadCommandName = json.containsKey("name") ? json["name"].as<String>() : "";
                _keypadCommandEncCode = json.containsKey("code") ? json["code"].as<String>() : "";
                _keypadCommandEnabled = json.containsKey("enabled") ? json["enabled"].as<int>() : 0;

                if (!command || !*command)
                {
                    sendResponse(jsonResult, "invalid data", 400);
                    return;
                }

                _keypadCommandReceivedReceivedCallback(command, _keypadCommandId, _keypadCommandName, _keypadCommandEncCode, _keypadCommandEnabled);

                _keypadCommandId = 0;
                _keypadCommandName = "";
                _keypadCommandEncCode = "000000";
                _keypadCommandEnabled = 1;

                return;
            }
        }

        bool queryCmdSet = false;
        if (strcmp(data, "1") == 0)
        {
            if (comparePrefixedPath(path, api_path_lock, api_path_query_config))
            {
                _queryCommands = _queryCommands | QUERY_COMMAND_CONFIG;
                queryCmdSet = true;
            }
            else if (comparePrefixedPath(path, api_path_lock, api_path_query_lockstate))
            {
                _queryCommands = _queryCommands | QUERY_COMMAND_LOCKSTATE;
                queryCmdSet = true;
            }
            else if (comparePrefixedPath(path, api_path_lock, api_path_query_keypad))
            {
                _queryCommands = _queryCommands | QUERY_COMMAND_KEYPAD;
                queryCmdSet = true;
            }
            else if (comparePrefixedPath(path, api_path_lock, api_path_query_battery))
            {
                _queryCommands = _queryCommands | QUERY_COMMAND_BATTERY;
                queryCmdSet = true;
            }
            if (queryCmdSet)
            {
                sendResponse(jsonResult);
                return;
            }
        }

        if (comparePrefixedPath(path, api_path_lock, api_path_config_action))
        {

            if (!data || !*data)
            {
                sendResponse(jsonResult, "missing data", 400);
                return;
            }

            if (_configUpdateReceivedCallback != NULL)
            {
                _configUpdateReceivedCallback(data);
            }
            return;
        }

        if (comparePrefixedPath(path, api_path_lock, api_path_timecontrol_action))
        {
            if (!data || !*data)
            {
                sendResponse(jsonResult, "missing data", 400);
                return;
            }

            if (_timeControlCommandReceivedReceivedCallback != NULL)
            {
                _timeControlCommandReceivedReceivedCallback(data);
            }
            return;
        }

        if (comparePrefixedPath(path, api_path_lock, api_path_auth_action))
        {
            if (!data || !*data)
            {
                sendResponse(jsonResult, "missing data", 400);
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

void NukiNetwork::onShutdownReceived(const char *path, WebServer &server)
{
    Log->println("[INFO] (REST API) Shutdown request received");
    Log->disableFileLog();
    delay(10);
    disableHAR();
    disableAPI();
    _preferences->end();
    safeShutdownESP(RestartReason::SafeShutdownRequestViaApi);
}

NetworkServiceState NukiNetwork::testNetworkServices()
{
    bool haClientOk = true;
    bool apiServerOk = true;

    if (_homeAutomationEnabled)
    {
        // 1. check whether _httpClient exists
        if (_homeAutomationMode == 1) // REST
        {
            if (_httpClient == nullptr)
            {
                Log->println(F("[DEBUG] _httpClient is NULL!"));
                haClientOk = false;
            }

            // 2. ping test for _homeAutomationAdress
            if (haClientOk && !_homeAutomationAdress.isEmpty())
            {
                if (!Ping.ping(_homeAutomationAdress.c_str()))
                {
                    Log->println(F("[ERROR] Ping to Home Automation Server failed!"));
                    haClientOk = false;
                }
                else
                {
                    Log->println(F("[DEBUG] Ping to Home Automation Server successful."));
                }
            }

            // 3. if Home Automation state API path exists, execute GET request
            String strPath = _preferences->getString(preference_har_key_state, "");
            if (haClientOk && !strPath.isEmpty())
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
                    haClientOk = false;
                }
            }
        }
        else if (_homeAutomationMode == 0) // UDP
        {
            if (_udpClient == nullptr)
            {
                Log->println(F("[DEBUG] _udpClient is NULL!"));
                haClientOk = false;
            }

            if (_udpClient && !_homeAutomationAdress.isEmpty())
            {
                if (!Ping.ping(_homeAutomationAdress.c_str()))
                {
                    Log->println(F("[ERROR] Ping to UDP Home Automation Server failed!"));
                    haClientOk = false;
                }
                else
                {
                    Log->println(F("[DEBUG] Ping to UDP Home Automation Server successful."));
                }
            }
        }
    }

    if (_apiEnabled)
    {
        // 4. check whether Rest Server (_server) exists
        if (_server == nullptr)
        {
            Log->println(F("[DEBUG] _server is NULL!"));
            apiServerOk = false;
        }

        if (apiServerOk)
        {
            WiFiClient client;
            IPAddress ip;
            // 5. test whether the local REST web server can be reached on the port
            if (localIP() == "0.0.0.0" || !ip.fromString(localIP()))
            {
                Log->printf(F("[ERROR] Invalid IP address for REST WebServer: %s\r\n"), localIP().c_str());
                apiServerOk = false;
            }
            //
            // The following block attempts to verify if the internal REST WebServer is responsive
            // by connecting a local HTTP client to the Ethernet interface using the device's own IP.
            // This typically fails on ESP32 due to missing loopback routing in LWIP,
            // unless special loopback flags are enabled (e.g., LWIP_NETIF_LOOPBACK).
            // As such, this check is disabled to avoid false negatives.
            //
            /*             else if (!client.connect(ip, _apiPort))
                        {
                            Log->printf(F("[ERROR] REST WebServer is not responding (%s:%d)!\r\n"), localIP().c_str(), _apiPort);
                            apiServerOk = false;
                        }
                        else
                        {
                            Log->println(F("[DEBUG] REST WebServer is responding."));
                            client.stop();
                        } */
        }
    }

    // 6. return error code
    if (apiServerOk && haClientOk)
        return NetworkServiceState::OK; // all OK
    if (!apiServerOk && haClientOk)
        return NetworkServiceState::ERROR_REST_API_SERVER; // _server not reachable
    if (apiServerOk && !haClientOk)
        return NetworkServiceState::ERROR_HAR_CLIENT; // _httpClient / Home Automation not reachable
    return NetworkServiceState::ERROR_BOTH;           // Both _server and _httpClient not reachable
}

void NukiNetwork::restartNetworkServices(NetworkServiceState status)
{
    if (!_homeAutomationEnabled && !_apiEnabled)
        return;

    if (status == NetworkServiceState::UNKNOWN)
    {
        status = testNetworkServices();
    }

    if (status == NetworkServiceState::OK)
    {
        Log->println(F("[DEBUG] Network services are running."));
        return; // No restart required
    }

    // If _httpClient is not reachable (-2 or -3), reinitialize
    if (_homeAutomationEnabled)
    {
        if (status == NetworkServiceState::ERROR_HAR_CLIENT || status == NetworkServiceState::ERROR_BOTH)
        {
            Log->println(F("[INFO] Reinitialization of HTTP client..."));
            // Clean up depending on the mode
            if (_homeAutomationMode == 1 && _httpClient)
            {
                delete _httpClient;
                _httpClient = nullptr;
                Log->println(F("[INFO] Deleted old HTTP client."));
            }
            else if (_homeAutomationMode == 0 && _udpClient)
            {
                delete _udpClient;
                _udpClient = nullptr;
                Log->println(F("[INFO] Deleted old UDP client."));
            }
            // Reinitialize
            if (_homeAutomationMode == 1)
            {
                _httpClient = new HTTPClient();
                if (_httpClient)
                    Log->println(F("[INFO] HTTP client successfully reinitialized."));
                else
                    Log->println(F("[ERROR] Failed to reinitialize HTTP client."));
            }
            else if (_homeAutomationMode == 0)
            {
                _udpClient = new NetworkUDP();
                if (_udpClient)
                    Log->println(F("[INFO] UDP client successfully reinitialized."));
                else
                    Log->println(F("[ERROR] Failed to reinitialize UDP client."));
            }
        }
    }
    // If the REST web server cannot be reached (-1 or -3), restart it
    if (_apiEnabled)
    {
        if (status == NetworkServiceState::ERROR_REST_API_SERVER || status == NetworkServiceState::ERROR_BOTH)
        {
            if (_server)
            {
                Log->println(F("[INFO] Restarting the REST WebServer..."));
                _server->stop();
                delete _server;
                _server = nullptr;
            }
            else
            {
                Log->println(F("[INFO] start REST API Server"));
            }
            _server = new WebServer(_apiPort);
            if (_server)
            {
                _server->onNotFound([this]()
                                    { onRestDataReceivedCallback(this->_server->uri().c_str(), *this->_server); });
                _server->begin();
                Log->println("[INFO] REST WebServer started on http://" + localIP() + ":" + String(_apiPort));
            }
            else
            {
                Log->println(F("[ERROR] REST Web Server cannot be initialized."));
            }
        }
    }
    Log->println(F("[DEBUG] Network services have been checked and reinit/restarted if necessary."));
}

void NukiNetwork::onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    Log->printf("[DEBUG] (LAN Event) event: %d\r\n", event);

    switch (event)
    {
    // --- Ethernet Events ---
    case ARDUINO_EVENT_ETH_START:
        Log->println(F("[DEBUG] ETH Started"));
        ETH.setHostname(_hostname.c_str());
        break;

    case ARDUINO_EVENT_ETH_CONNECTED:
        Log->println(F("[INFO] ETH Connected"));
        _ethConnected = true;
        break;

    case ARDUINO_EVENT_ETH_GOT_IP:
        Log->printf("[DEBUG] ETH Got IP: '%s'\r\n", ETH.localIP().toString());

        _connected = true;
        if (_preferences->getBool(preference_ntw_reconfigure, false))
        {
            _preferences->putBool(preference_ntw_reconfigure, false);
        }
        break;
    case ARDUINO_EVENT_ETH_GOT_IP6:
        if (!_connected)
        {
            Log->printf("[DEBUG] ETH Got IP: '%s'\r\n", ETH.localIP().toString());

            _connected = true;
            if (_preferences->getBool(preference_ntw_reconfigure, false))
            {
                _preferences->putBool(preference_ntw_reconfigure, false);
            }
            break;
        }
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

        Log->print(F("[DEBUG] WiFi connecting"));
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
                Log->disableFileLog();
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
        Log->println(F("[INFO] Starting AP with SSID NukiRestBridge and Password NukiBridgeESP32"));
        _startAP = false;
        WiFi.mode(WIFI_AP);
        delay(500);
        WiFi.softAPsetHostname(_hostname.c_str());
        delay(500);
        WiFi.softAP(F("NukiRestBridge"), F("NukiBridgeESP32"));
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
            Log->disableFileLog();
            delay(10);
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
        break;
    }
}

bool NukiNetwork::comparePrefixedPath(const char *fullPath, const char *mainPath, const char *subPath)
{
    char prefixedPath[385];
    buildApiPath(mainPath, subPath, prefixedPath);
    return strcmp(fullPath, prefixedPath) == 0;
}

void NukiNetwork::buildApiPath(const char *mainPath, const char *subPath, char *outPath)
{
    // Copy (mainPath) to outPath
    strncpy(outPath, mainPath, 384);
    outPath[384] = '\0'; // Zero terminate to be on the safe side

    // Append the (path) zo outPath
    strncat(outPath, subPath, 384 - strlen(outPath));
}

void NukiNetwork::assignNewApiToken()
{
    _apitoken->assignNewToken();
}

char *NukiNetwork::getApiToken()
{

    return _apitoken->get();
}
