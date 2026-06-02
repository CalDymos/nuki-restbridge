#include "EthernetDevice.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "EspMillis.h"
#include "util/TaskUtils.h"

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
#include "esp_private/esp_gpio_reserve.h"
#include <bootloader_common.h>
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
#include "esp_psram.h"
#endif
#include "esp32-hal.h"
#endif

// ethCriticalFailure and wifiFallback are defined in main.cpp
extern bool ethCriticalFailure;
extern bool wifiFallback;

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------

EthernetDevice::EthernetDevice(const String& hostname,
                               Preferences* preferences,
                               const IPConfiguration* ipConfiguration,
                               const Config& config)
    : NetworkDevice(hostname, preferences, ipConfiguration),
      _config(config)
{
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

void EthernetDevice::initialize()
{
    TaskWdtResetAndDelay(250);

    // Critical-failure guard: if the previous ETH.begin() caused a crash,
    // ethCriticalFailure is still set. Fall back to Wi-Fi and reboot.
    if (ethCriticalFailure)
    {
        ethCriticalFailure = false;
        Log->println(F("[ERROR] Failed to initialize ethernet hardware"));
        Log->println(F("[ERROR] Network device has a critical failure, "
                       "enable fallback to Wi-Fi and reboot."));
        wifiFallback = true;
        Log->disableFileLog();
        TaskWdtResetAndDelay(200);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }

    Log->print(F("[INFO] Init Ethernet: "));
    Log->println(_config.deviceName);

    // Register network event handler before ETH.begin()
    Network.onEvent([this](arduino_event_id_t event, arduino_event_info_t info)
    {
        onNetworkEvent(event, info);
    });

    // Set the critical-failure flag BEFORE calling ETH.begin().
    // If ETH.begin() causes a hard crash, the flag survives the reboot
    // and initialize() will detect it on the next boot.
    ethCriticalFailure = true;
    _hardwareInitialized = beginEthernetDevice();
    ethCriticalFailure = false;

    if (!_hardwareInitialized)
    {
        Log->println(F("[ERROR] Failed to initialize ethernet hardware"));
        Log->println(F("[ERROR] Network device has a critical failure, "
                       "enable fallback to Wi-Fi and reboot."));
        wifiFallback = true;
        Log->disableFileLog();
        TaskWdtResetAndDelay(200);
        restartEsp(RestartReason::NetworkDeviceCriticalFailure);
        return;
    }

    Log->println(F("[INFO] Ethernet hardware initialized"));
    wifiFallback = false;

    // Apply static IP immediately for SPI devices; for RMII, schedule a retry
    if (!_ipConfiguration->dhcpEnabled())
    {
        if (_config.useSpi)
        {
            ETH.config(_ipConfiguration->ipAddress(),
                       _ipConfiguration->defaultGateway(),
                       _ipConfiguration->subnet(),
                       _ipConfiguration->dnsServer());
        }
        else
        {
            // RMII: hardware may not be ready for IP config immediately
            _checkIpTs = espMillis() + 2000;
        }
    }
}

void EthernetDevice::update()
{
    if (_checkIpTs == -1 || _checkIpTs >= espMillis())
        return;

    if (_ipConfiguration->ipAddress() != ETH.localIP())
    {
        Log->println(F("[DEBUG] ETH Set static IP"));
        ETH.config(_ipConfiguration->ipAddress(),
                   _ipConfiguration->defaultGateway(),
                   _ipConfiguration->subnet(),
                   _ipConfiguration->dnsServer());
        _checkIpTs = espMillis() + 5000; // retry again if needed
        return;
    }

    _checkIpTs = -1; // IP matches — no further retries needed
}

void EthernetDevice::reconfigure()
{
    Log->disableFileLog();
    TaskWdtResetAndDelay(200);
    restartEsp(RestartReason::ReconfigureETH);
}

void EthernetDevice::disableAutoRestarts()
{
    _autoRestartEnabled = false;
}

// -----------------------------------------------------------------------
// Status
// -----------------------------------------------------------------------

bool EthernetDevice::isConnected() const
{
    return _connected;
}

// -----------------------------------------------------------------------
// Network information
// -----------------------------------------------------------------------

String EthernetDevice::localIP() const
{
    return ETH.localIP().toString();
}

NetworkDeviceType EthernetDevice::type() const
{
    return NetworkDeviceType::ETH;
}

String EthernetDevice::deviceName() const
{
    return _config.deviceName;
}

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
static void revokeReservedGpiosForRmii()
{
    // Release reserved GPIO pins on older ESP32 packages before RMII Ethernet init.
    uint32_t pkgVersion = bootloader_common_get_chip_ver_pkg();

#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
    if (esp_psram_get_size() <= 0 && pkgVersion <= 3)
#else
    if (pkgVersion <= 3)
#endif
    {
        esp_gpio_revoke(0xFFFFFFFFFFFFFFFFULL);
    }
}
#endif

// -----------------------------------------------------------------------
// Internal: beginEthernetDevice()
// -----------------------------------------------------------------------

bool EthernetDevice::beginEthernetDevice()
{
    if (_config.useSpi)
    {
        SPI.begin(_config.spiSckPin, _config.spiMisoPin, _config.spiMosiPin);
        return ETH.begin(_config.phyType,
                         _config.phyAddr,
                         _config.csPin,
                         _config.irqPin,
                         _config.resetPin,
                         SPI);
    }

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
    revokeReservedGpiosForRmii();

    return ETH.begin(_config.phyType,
                     _config.phyAddr,
                     _config.mdcPin,
                     _config.mdioPin,
                     _config.powerPin,
                     _config.clockMode);
#else
    return false; // RMII not supported on this target
#endif
}

// -----------------------------------------------------------------------
// Internal: onNetworkEvent()  (mirrors NukiNetwork::onNetworkEvent ETH section)
// -----------------------------------------------------------------------

void EthernetDevice::onNetworkEvent(arduino_event_id_t event,
                                    arduino_event_info_t info)
{
    Log->printf("[DEBUG] (LAN Event) event: %d\r\n", event);

    switch (event)
    {
    case ARDUINO_EVENT_ETH_START:
        Log->println(F("[DEBUG] ETH Started"));
        ETH.setHostname(_hostname.c_str());
        break;

    case ARDUINO_EVENT_ETH_CONNECTED:
        Log->println(F("[INFO] ETH Connected"));
        _ethConnected = true;
        break;

    case ARDUINO_EVENT_ETH_GOT_IP:
        Log->printf("[DEBUG] ETH Got IP: '%s'\r\n", ETH.localIP().toString().c_str());
        _connected = true;
        if (_preferences->getBool(preference_ntw_reconfigure, false))
            _preferences->putBool(preference_ntw_reconfigure, false);
        break;

    case ARDUINO_EVENT_ETH_GOT_IP6:
        if (!_connected)
        {
            Log->printf("[DEBUG] ETH Got IP: '%s'\r\n", ETH.localIP().toString().c_str());
            _connected = true;
            if (_preferences->getBool(preference_ntw_reconfigure, false))
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

    default:
        Log->print(F("[DEBUG] Unknown LAN Event: "));
        Log->println(event);
        break;
    }
}

// -----------------------------------------------------------------------
// Internal: onDisconnected()
// -----------------------------------------------------------------------

void EthernetDevice::onDisconnected()
{
    if (_autoRestartEnabled &&
        _preferences->getBool(preference_restart_on_disconnect, false) &&
        espMillis() > 60000)
    {
        Log->disableFileLog();
        TaskWdtResetAndDelay(10);
        restartEsp(RestartReason::RestartOnDisconnectWatchdog);
    }
}
