#include "NukiNetwork.h"
#include "CharBuffer.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include "RestartReason.h"
#include "hal/wdt_hal.h"
#include "util/TaskUtils.h"
#include <esp_mac.h>
#include "HarClient.h"
#include "RestApiServer.h"

namespace
{
    constexpr int NETWORK_HARDWARE_WIFI = 1;
    constexpr int NETWORK_HARDWARE_LEGACY_LAN8720 = 2;
    constexpr int NETWORK_HARDWARE_M5STACK_W5500 = 3;
    constexpr int NETWORK_HARDWARE_OLIMEX_LAN8720 = 4;
    constexpr int NETWORK_HARDWARE_WT32_LAN8720 = 5;
    constexpr int NETWORK_HARDWARE_M5STACK_POE_TLK110 = 6;
    constexpr int NETWORK_HARDWARE_LILYGO_T_ETH_POE = 7;
    constexpr int NETWORK_HARDWARE_GL_S10_IP101 = 8;
    constexpr int NETWORK_HARDWARE_ETH01_EVO_DM9051 = 9;
    constexpr int NETWORK_HARDWARE_M5STACK_W5500_S3 = 10;
    constexpr int NETWORK_HARDWARE_CUSTOM = 11;
    constexpr int NETWORK_HARDWARE_LILYGO_T_ETH_ELITE = 12;
    constexpr int NETWORK_HARDWARE_WAVESHARE_ESP32_S3_ETH = 13;
    constexpr int NETWORK_HARDWARE_LILYGO_T_ETH_LITE_S3 = 14;
    constexpr int NETWORK_HARDWARE_OLIMEX_LAN8720_WROVER = 20;

#if !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32P4)
    typedef enum
    {
        ETH_CLOCK_GPIO0_IN = 0,
        ETH_CLOCK_GPIO0_OUT = 1,
        ETH_CLOCK_GPIO16_OUT = 2,
        ETH_CLOCK_GPIO17_OUT = 3
    } eth_clock_mode_t;
#endif

    enum class EthernetBusType
    {
        RMII,
        SPI
    };

    struct EthernetDeviceConfig
    {
        int hardwareId;                    // Preference value used by the web configuration.
        const char *deviceName;            // Human-readable device name for logging.
        EthernetBusType busType;           // Ethernet bus type used by the device.
        eth_phy_type_t phyType;            // Ethernet PHY type used by ETH.begin().
        int32_t phyAddr;                   // Ethernet PHY address.
        int powerPin;                      // RMII PHY power pin.
        int mdcPin;                        // RMII MDC pin.
        int mdioPin;                       // RMII MDIO pin.
        eth_clock_mode_t clockMode;        // RMII clock mode.
        int csPin;                         // SPI chip-select pin.
        int irqPin;                        // SPI interrupt pin.
        int resetPin;                      // SPI reset pin.
        int spiSckPin;                     // SPI SCK pin.
        int spiMisoPin;                    // SPI MISO pin.
        int spiMosiPin;                    // SPI MOSI pin.
    };

    const EthernetDeviceConfig *findPresetEthernetDeviceConfig(int hardwareId)
    {
        static const EthernetDeviceConfig configs[] =
        {
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
            // Backward-compatible value for the former generic "LAN module" option.
            {NETWORK_HARDWARE_LEGACY_LAN8720, "LAN module (LAN8720 / Olimex-compatible)", EthernetBusType::RMII, ETH_PHY_LAN8720, 0, 12, 23, 18, ETH_CLOCK_GPIO17_OUT, -1, -1, -1, -1, -1, -1},
            {NETWORK_HARDWARE_OLIMEX_LAN8720, "Olimex ESP32-POE/POE-ISO WROOM (LAN8720)", EthernetBusType::RMII, ETH_PHY_LAN8720, 0, 12, 23, 18, ETH_CLOCK_GPIO17_OUT, -1, -1, -1, -1, -1, -1},
            {NETWORK_HARDWARE_OLIMEX_LAN8720_WROVER, "Olimex ESP32-POE/POE-ISO WROVER (LAN8720)", EthernetBusType::RMII, ETH_PHY_LAN8720, 0, 12, 23, 18, ETH_CLOCK_GPIO0_OUT, -1, -1, -1, -1, -1, -1},
            {NETWORK_HARDWARE_WT32_LAN8720, "WT32-ETH01 (LAN8720)", EthernetBusType::RMII, ETH_PHY_LAN8720, 1, 16, 23, 18, ETH_CLOCK_GPIO0_IN, -1, -1, -1, -1, -1, -1},
            {NETWORK_HARDWARE_M5STACK_POE_TLK110, "M5Stack PoESP32 Unit (TLK110)", EthernetBusType::RMII, ETH_PHY_TLK110, 1, 5, 23, 18, ETH_CLOCK_GPIO0_IN, -1, -1, -1, -1, -1, -1},
            {NETWORK_HARDWARE_LILYGO_T_ETH_POE, "LilyGO T-ETH-POE (LAN8720)", EthernetBusType::RMII, ETH_PHY_LAN8720, 0, -1, 23, 18, ETH_CLOCK_GPIO17_OUT, -1, -1, -1, -1, -1, -1},
            {NETWORK_HARDWARE_GL_S10_IP101, "GL-S10 (IP101)", EthernetBusType::RMII, ETH_PHY_IP101, 1, 5, 23, 18, ETH_CLOCK_GPIO0_IN, -1, -1, -1, -1, -1, -1},
#endif
            {NETWORK_HARDWARE_M5STACK_W5500, "M5Stack Atom POE (W5500)", EthernetBusType::SPI, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN, 19, -1, -1, 22, 23, 33},
            {NETWORK_HARDWARE_M5STACK_W5500_S3, "M5Stack Atom POE S3 (W5500)", EthernetBusType::SPI, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN, 6, -1, -1, 5, 7, 8},
            {NETWORK_HARDWARE_ETH01_EVO_DM9051, "ETH01-Evo (DM9051)", EthernetBusType::SPI, ETH_PHY_DM9051, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN, 9, 8, 6, 7, 3, 10},
            {NETWORK_HARDWARE_LILYGO_T_ETH_ELITE, "LilyGO T-ETH ELite (W5500)", EthernetBusType::SPI, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN, 45, 14, -1, 48, 47, 21},
            {NETWORK_HARDWARE_WAVESHARE_ESP32_S3_ETH, "Waveshare ESP32-S3-ETH / POE-ETH (W5500)", EthernetBusType::SPI, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN, 14, 10, 9, 13, 12, 11},
            {NETWORK_HARDWARE_LILYGO_T_ETH_LITE_S3, "LilyGO T-ETH-Lite-ESP32S3 (W5500)", EthernetBusType::SPI, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN, 9, 13, 14, 10, 11, 12}
        };

        for (const auto &config : configs)
        {
            if (config.hardwareId == hardwareId)
            {
                return &config;
            }
        }

        return nullptr;
    }

    const char *getCustomEthernetDeviceName(int customPhy)
    {
        switch (customPhy)
        {
        case 1:
            return "Custom (W5500)";
        case 2:
            return "Custom (DM9051)";
        case 3:
            return "Custom (KSZ8851SNL)";
        case 4:
            return "Custom (LAN8720)";
        case 5:
            return "Custom (RTL8201)";
        case 6:
            return "Custom (TLK110/IP101)";
        case 7:
            return "Custom (DP83848)";
        case 8:
            return "Custom (KSZ8041)";
        case 9:
            return "Custom (KSZ8081)";
        default:
            return "Custom Ethernet";
        }
    }

    eth_phy_type_t getCustomEthernetPhyType(int customPhy)
    {
        switch (customPhy)
        {
        case 1:
            return ETH_PHY_W5500;
        case 2:
            return ETH_PHY_DM9051;
        case 3:
            return ETH_PHY_KSZ8851;
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
        case 4:
            return ETH_PHY_LAN8720;
        case 5:
            return ETH_PHY_RTL8201;
        case 6:
            return ETH_PHY_TLK110;
        case 7:
            return ETH_PHY_DP83848;
        case 8:
            return ETH_PHY_KSZ8041;
        case 9:
            return ETH_PHY_KSZ8081;
#endif
        default:
            return ETH_PHY_W5500;
        }
    }

    eth_clock_mode_t getCustomEthernetClockMode(int clockPreference)
    {
        switch (clockPreference)
        {
        case 0:
            return ETH_CLOCK_GPIO0_IN;
        case 1:
            return ETH_CLOCK_GPIO0_OUT;
        case 2:
            return ETH_CLOCK_GPIO16_OUT;
        case 3:
            return ETH_CLOCK_GPIO17_OUT;
        default:
            return ETH_CLOCK_GPIO17_OUT;
        }
    }

    bool buildCustomEthernetDeviceConfig(Preferences *preferences, EthernetDeviceConfig &config)
    {
        const int customPhy = preferences->getInt(preference_network_custom_phy, 0);

        if (customPhy >= 1 && customPhy <= 3)
        {
            config = {
                NETWORK_HARDWARE_CUSTOM,
                getCustomEthernetDeviceName(customPhy),
                EthernetBusType::SPI,
                getCustomEthernetPhyType(customPhy),
                preferences->getInt(preference_network_custom_addr, -1),
                -1,
                -1,
                -1,
                ETH_CLOCK_GPIO0_IN,
                preferences->getInt(preference_network_custom_cs, -1),
                preferences->getInt(preference_network_custom_irq, -1),
                preferences->getInt(preference_network_custom_rst, -1),
                preferences->getInt(preference_network_custom_sck, -1),
                preferences->getInt(preference_network_custom_miso, -1),
                preferences->getInt(preference_network_custom_mosi, -1)};
            return true;
        }

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
        if (customPhy >= 4 && customPhy <= 9)
        {
            config = {
                NETWORK_HARDWARE_CUSTOM,
                getCustomEthernetDeviceName(customPhy),
                EthernetBusType::RMII,
                getCustomEthernetPhyType(customPhy),
                preferences->getInt(preference_network_custom_addr, -1),
                preferences->getInt(preference_network_custom_pwr, -1),
                preferences->getInt(preference_network_custom_mdc, -1),
                preferences->getInt(preference_network_custom_mdio, -1),
                getCustomEthernetClockMode(preferences->getInt(preference_network_custom_clk, 0)),
                -1,
                -1,
                -1,
                -1,
                -1,
                -1};
            return true;
        }
#endif

        return false;
    }

    bool resolveEthernetDeviceConfig(Preferences *preferences, EthernetDeviceConfig &config)
    {
        const int hardwareId = preferences->getInt(preference_network_hardware, NETWORK_HARDWARE_WIFI);

        if (hardwareId == NETWORK_HARDWARE_CUSTOM)
        {
            return buildCustomEthernetDeviceConfig(preferences, config);
        }

        const EthernetDeviceConfig *presetConfig = findPresetEthernetDeviceConfig(hardwareId);
        if (presetConfig == nullptr)
        {
            return false;
        }

        config = *presetConfig;
        return true;
    }

    bool beginEthernetDevice(const EthernetDeviceConfig &config)
    {
        if (config.busType == EthernetBusType::SPI)
        {
            SPI.begin(config.spiSckPin, config.spiMisoPin, config.spiMosiPin);
            return ETH.begin(config.phyType, config.phyAddr, config.csPin, config.irqPin, config.resetPin, SPI);
        }

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
        return ETH.begin(config.phyType, config.phyAddr, config.mdcPin, config.mdioPin, config.powerPin, config.clockMode);
#else
        return false;
#endif
    }
}


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

        _harClient->initialize();
        
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
            else
            {
                _checkIpTs = -1;
            }
        }
        break;
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
    switch (_networkDeviceType)
    {
    case NetworkDeviceType::WiFi:
        _preferences->putString(preference_wifi_ssid, "");
        _preferences->putString(preference_wifi_pass, "");
        Log->disableFileLog();
        TaskWdtResetAndDelay(200);
        restartEsp(RestartReason::ReconfigureWifi);
        break;
    case NetworkDeviceType::ETH:
        Log->disableFileLog();
        TaskWdtResetAndDelay(200);
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

bool NukiNetwork::networkGateOpen() const
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
            TaskWdtResetAndDelay(1000);
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
    TaskWdtResetAndDelay(250);
    if (ethCriticalFailure)
    {
        ethCriticalFailure = false;
        Log->println(F("[ERROR] Failed to initialize ethernet hardware"));
        Log->println(F("[ERROR] Network device has a critical failure, enable fallback to Wi-Fi and reboot."));
        wifiFallback = true;
        Log->disableFileLog();
        TaskWdtResetAndDelay(200);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }

    EthernetDeviceConfig ethernetConfig = {};
    if (!resolveEthernetDeviceConfig(_preferences, ethernetConfig))
    {
        Log->println(F("[ERROR] Unsupported ethernet hardware configuration"));
        Log->println(F("[ERROR] Network device has a critical failure, enable fallback to Wi-Fi and reboot."));
        wifiFallback = true;
        Log->disableFileLog();
        TaskWdtResetAndDelay(200);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }

    Log->print(F("[INFO] Init Ethernet: "));
    Log->println(ethernetConfig.deviceName);

    Network.onEvent([this](arduino_event_id_t event, arduino_event_info_t info)
                    { this->onNetworkEvent(event, info); });

    ethCriticalFailure = true;
    _hardwareInitialized = beginEthernetDevice(ethernetConfig);
    ethCriticalFailure = false;

    if (_hardwareInitialized)
    {
        Log->println(F("[INFO] Ethernet hardware Initialized"));
        wifiFallback = false;

        if (!_ipConfiguration->dhcpEnabled())
        {
            if (ethernetConfig.busType == EthernetBusType::SPI)
            {
                ETH.config(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
            }
            else
            {
                _checkIpTs = espMillis() + 2000;
            }
        }
    }
    else
    {
        Log->println(F("[ERROR] Failed to initialize ethernet hardware"));
        Log->println(F("[ERROR] Network device has a critical failure, enable fallback to Wi-Fi and reboot."));
        wifiFallback = true;
        Log->disableFileLog();
        TaskWdtResetAndDelay(200);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }
}

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
        TaskWdtResetAndDelay(500);

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
            TaskWdtResetAndDelay(100);
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
                TaskWdtResetAndDelay(100);
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
        TaskWdtResetAndDelay(500);
        WiFi.softAPsetHostname(_hostname.c_str());
        TaskWdtResetAndDelay(500);
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
            TaskWdtResetAndDelay(10);
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
        break;
    }
}

void NukiNetwork::assignNewApiToken()
{
    _restApiServer->assignNewApiToken();
}

char *NukiNetwork::getApiToken()
{

    return _restApiServer->getApiToken();
}
