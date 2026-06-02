#include "NukiNetwork.h"
#include "CharBuffer.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include "RestartReason.h"
#include "hal/wdt_hal.h"
#include "util/TaskUtils.h"
#include <esp_mac.h>
#include "networkDevices/NetworkDevice.h"
#include "networkDevices/NetworkDeviceFactory.h"
#include "HarClient.h"
#include "RestApiServer.h"

NukiNetwork::NukiNetwork(Preferences *preferences, ImportExport* importExport)
    : _preferences(preferences),
      _importExport(importExport)
{
    _webCfgEnabled = _preferences->getBool(preference_webcfgserver_enabled, true);
    _harClient = new HarClient(_preferences);
    _restApiServer = new RestApiServer(
        _preferences,
        [this]() { _harClient->disable(); },       // disableHarFn
        [this]() { clearWifiFallback(); }          // clearWifiFallbackFn
    );
    setupDevice();
}

NukiNetwork::~NukiNetwork()
{
    if (_restApiServer)
    {
        delete _restApiServer;
        _restApiServer = nullptr;
    }
    if (_harClient)
    {
        delete _harClient;
        _harClient = nullptr;
    }
    if (_device)
    {
        delete _device;
        _device = nullptr;
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
        _restApiServer->initialize();
        
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

        // Create device / hostname is set at this point
        _device = NetworkDeviceFactory::create(_networkDeviceType, _hostname, _preferences, _ipConfiguration);

        if (_device)
        {
            _device->initialize();
        }
        else
        {
            Log->println(F("[ERROR] Failed to create network device"));

            if (_networkDeviceType == NetworkDeviceType::ETH)
            {
                Log->println(F("[ERROR] Ethernet device could not be created, "
                "enable fallback to Wi-Fi and reboot."));
                wifiFallback = true;
                Log->disableFileLog();
                TaskWdtResetAndDelay(200);
                restartEsp(RestartReason::NetworkDeviceCriticalFailure);
            }
        }

        Log->print(F("[DEBUG] Host name: "));
        Log->println(_hostname);

        _harClient->initialize();
        
        readSettings();

        // Give the network time to get an IP
        const int64_t startMillis = espMillis();
        // Wait until there is a connection or 10 seconds have elapsed
        while (!isConnected() && (espMillis() - startMillis < 10000))
        {
            yield();
        }

        startNetworkServices();
        _networkServicesState = testNetworkServices();
    }
}

NukiNetwork::ServiceRestartRequest NukiNetwork::consumeServiceRestartRequest()
{
    ServiceRestartRequest request = _pendingServiceRestart;
    _pendingServiceRestart = ServiceRestartRequest::None;
    return request;
}

void NukiNetwork::requestServiceRestart(bool reconnect)
{
    _pendingServiceRestart = reconnect ? ServiceRestartRequest::RestartWithReconnect
                                       : ServiceRestartRequest::Restart;
}

bool NukiNetwork::update()
{

    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    int64_t ts = espMillis();

    // update device
    if (_device)
    {
        _device->update();
    }

    if (disableNetwork || (!_harClient->isEnabled() && !_restApiServer->isEnabled()) || isApOpen())
    {
        return false;
    }

    if (!isConnected() || (_networkServicesConnectCounter > 15))
    {
        _networkServicesConnectCounter = 0;

        if (_restartOnDisconnect && espMillis() > 60000)
        {
            Log->disableFileLog();
            TaskWdtResetAndDelay(10);
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
    }

    if (isConnected() && (_restApiServer->isEnabled() || _harClient->isEnabled()))
    {
        if (ts - _lastNetworkServiceTs > 30000)
        { // test all 30 seconds
            _lastNetworkServiceTs = ts;
            _networkServicesState = testNetworkServices();

            bool svcBothDown = _harClient->isEnabled() && _restApiServer->isEnabled() && _networkServicesState != NetworkServiceState::OK;
            bool svcHADown = !_restApiServer->isEnabled() && _networkServicesState != NetworkServiceState::ERROR_REST_API_SERVER;
            bool svcAPIDown = !_harClient->isEnabled() && _networkServicesState != NetworkServiceState::ERROR_HAR_CLIENT;

            if (svcBothDown || svcHADown || svcAPIDown)
            { // error in network Services
                restartNetworkServices(_networkServicesState);
                TaskWdtResetAndDelay(1000);
                _networkServicesState = testNetworkServices(); // test network services again

                bool expectedStateOk =
                    (_harClient->isEnabled() && _restApiServer->isEnabled() && _networkServicesState == NetworkServiceState::OK) ||
                    (_harClient->isEnabled() && !_restApiServer->isEnabled() && _networkServicesState == NetworkServiceState::ERROR_REST_API_SERVER) ||
                    (!_harClient->isEnabled() && _restApiServer->isEnabled() && _networkServicesState == NetworkServiceState::ERROR_HAR_CLIENT);

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
            TaskWdtResetAndDelay(200);
            restartEsp(RestartReason::ReconfigureWebCfgServer);
        }
        else if (!_webCfgEnabled)
        {
            forceEnableWebCfgServer = false;
        }
        TaskWdtResetAndDelay(2000);
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
            TaskWdtResetAndDelay(200);
            restartEsp(RestartReason::NetworkTimeoutWatchdog);
        }
        TaskWdtResetAndDelay(2000);
        return false;
    }

    _lastConnectedTs = ts;

    _harClient->update(ts, signalStrength());

    _restApiServer->handleClient();

    return true;
}

void NukiNetwork::reconfigureAdapter()
{
    if (_device) _device->reconfigure();
}

void NukiNetwork::scan(bool passive, bool async)
{
    if (_device) _device->scan(passive, async);
}

bool NukiNetwork::isApOpen() const
{
    return _device ? _device->isApOpen() : false;
}

bool NukiNetwork::isConnected() const
{
    return _device ? _device->isConnected() : false;
}

bool NukiNetwork::networkGateOpen() const
{
    return _device ? _device->networkGateOpen() : false;
}

String NukiNetwork::localIP() const
{
    return _device ? _device->localIP() : "0.0.0.0";
}

String NukiNetwork::networkBSSID() const
{
    return _device ? _device->networkBSSID() : "";
}

const NetworkDeviceType NukiNetwork::networkDeviceType()
{
    return _device ? _device->type() : NetworkDeviceType::UNDEFINED;
}

int8_t NukiNetwork::signalStrength()
{
    return _device ? _device->signalStrength() : 127;
}

void NukiNetwork::clearWifiFallback()
{
    wifiFallback = false;
}

void NukiNetwork::disableAutoRestarts()
{
    _networkTimeout = 0;
    _restartOnDisconnect = false;
    if (_device) _device->disableAutoRestarts();
}

bool NukiNetwork::testWifiCredentials(const String& ssid, const String& pass, uint32_t timeoutMs)
{
    return _device ? _device->testWifiCredentials(ssid, pass, timeoutMs) : false;
}

void NukiNetwork::disableAPI()
{
    _restApiServer->disable();
}
void NukiNetwork::disableHAR()
{
    _harClient->disable();
}

NetworkServiceState NukiNetwork::networkServicesState()
{
    return _networkServicesState;
}

uint8_t NukiNetwork::queryCommands()
{
    return _restApiServer->queryCommands();
}

void NukiNetwork::sendToHAFloat(const char *path, const char *param, const float value, uint8_t precision)
{
    _harClient->sendFloat(path, param, value, precision);
}

void NukiNetwork::sendToHAInt(const char *path, const char *param, const int value)
{
    _harClient->sendInt(path, param, value);
}

void NukiNetwork::sendToHAUInt(const char *path, const char *param, const unsigned int value)
{
    _harClient->sendUInt(path, param, value);
}

void NukiNetwork::sendToHAULong(const char *path, const char *param, const unsigned long value)
{
    _harClient->sendULong(path, param, value);
}

void NukiNetwork::sendToHALongLong(const char *path, const char *param, const int64_t value)
{
    _harClient->sendLongLong(path, param, value);
}

void NukiNetwork::sendToHABool(const char *path, const char *param, const bool value)
{
    _harClient->sendBool(path, param, value);
}

void NukiNetwork::sendToHAString(const char *path, const char *param, const char *value)
{
    _harClient->sendString(path, param, value);
}

void NukiNetwork::sendToHALockBleAddress(const std::string &address)
{
    _harClient->sendLockBleAddress(address);
}

void NukiNetwork::sendToHABatteryReport(const NukiLock::BatteryReport &batteryReport)
{
    _harClient->sendBatteryReport(batteryReport);
}

void NukiNetwork::sendToHABleRssi(const int &rssi)
{
    _harClient->sendBleRssi(rssi);
}

void NukiNetwork::sendToHAKeyTurnerState(const NukiLock::KeyTurnerState &keyTurnerState, const NukiLock::KeyTurnerState &lastKeyTurnerState)
{
    _harClient->sendKeyTurnerState(keyTurnerState, lastKeyTurnerState);
}

void NukiNetwork::sendResponse(JsonDocument &jsonResult, const char *message, int httpCode)
{
    _restApiServer->sendResponse(jsonResult, message, httpCode);
}

void NukiNetwork::sendResponse(const char *jsonResultStr)
{
    _restApiServer->sendResponse(jsonResultStr);
}

void NukiNetwork::setLockActionReceivedCallback(LockActionResult (*cb)(const char *value))
{
    _restApiServer->setLockActionReceivedCallback(cb);
}

void NukiNetwork::setConfigUpdateReceivedCallback(void (*cb)(const char *value))
{
    _restApiServer->setConfigUpdateReceivedCallback(cb);
}

void NukiNetwork::setKeypadCommandReceivedCallback(void (*cb)(const char *command, const uint &id, const String &name, const String &code, const int &enabled))
{
    _restApiServer->setKeypadCommandReceivedCallback(cb);
}

void NukiNetwork::setTimeControlCommandReceivedCallback(void (*cb)(const char *value))
{
    _restApiServer->setTimeControlCommandReceivedCallback(cb);
}

void NukiNetwork::setAuthCommandReceivedCallback(void (*cb)(const char *value))
{
    _restApiServer->setAuthCommandReceivedCallback(cb);
}

void NukiNetwork::readSettings()
{
    _restartOnDisconnect = _preferences->getBool(preference_restart_on_disconnect, false);

    _networkTimeout = _preferences->getInt(preference_network_timeout, 0);
    if (_networkTimeout == 0)
    {
        _networkTimeout = -1;
        _preferences->putInt(preference_network_timeout, _networkTimeout);
    }

    _harClient->readSettings();
}

// -----------------------------------------------------------------------------
//  PRIVATE METHODEN
// -----------------------------------------------------------------------------

void NukiNetwork::startNetworkServices()
{
    _harClient->start();

    _restApiServer->start(localIP());
}

NetworkServiceState NukiNetwork::testNetworkServices()
{
    bool apiOk = _restApiServer->test(localIP());
    _restApiServer->setOk(apiOk);

    bool harOk = _harClient->test();
    _harClient->setOk(harOk);

    if ( apiOk &&  harOk) return NetworkServiceState::OK;
    if (!apiOk &&  harOk) return NetworkServiceState::ERROR_REST_API_SERVER;
    if ( apiOk && !harOk) return NetworkServiceState::ERROR_HAR_CLIENT;
    return NetworkServiceState::ERROR_BOTH;
}

void NukiNetwork::restartNetworkServices(NetworkServiceState status)
{
    if (!_harClient->isEnabled() && !_restApiServer->isEnabled())
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

    if (_harClient->isEnabled())
    {
        if (status == NetworkServiceState::ERROR_HAR_CLIENT ||
            status == NetworkServiceState::ERROR_BOTH)
        {
            _harClient->restart();
        }
    }

    // If the REST web server cannot be reached (-1 or -3), restart it
    if (_restApiServer->isEnabled())
    {
        if (status == NetworkServiceState::ERROR_REST_API_SERVER ||
            status == NetworkServiceState::ERROR_BOTH)
        {
            _restApiServer->restart(localIP());
        }
    }
    Log->println(F("[DEBUG] Network services have been checked and reinit/restarted if necessary."));
}

void NukiNetwork::assignNewApiToken()
{
    _restApiServer->assignNewApiToken();
}

char *NukiNetwork::getApiToken()
{

    return _restApiServer->getApiToken();
}
