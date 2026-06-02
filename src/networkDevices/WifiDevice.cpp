#include "WifiDevice.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "EspMillis.h"
#include "util/TaskUtils.h"

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------

WifiDevice::WifiDevice(const String& hostname,
                       Preferences* preferences,
                       const IPConfiguration* ipConfiguration)
    : NetworkDevice(hostname, preferences, ipConfiguration)
{
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

void WifiDevice::initialize()
{
    _ssid = _preferences->getString(preference_wifi_ssid, "");
    _ssid.trim();
    _pass = _preferences->getString(preference_wifi_pass, "");
    _pass.trim();

    WiFi.setHostname(_hostname.c_str());

    // Register event handler
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info)
    {
        onWifiEvent(event, info);
    });

    if (isWifiConfigured())
    {
        Log->println("[INFO] Attempting to connect to saved SSID " + _ssid);
        _openAP = false;

        // NukiHub pattern: only scan when the user wants best-RSSI selection.
        // Otherwise connect directly — avoids an unnecessary scan delay.
        if (_preferences->getBool(preference_find_best_rssi, false))
        {
            scan(false, true);
        }
        else
        {
            WiFi.mode(WIFI_STA);
            connect();
        }
    }
    else
    {
        Log->println(F("[INFO] No SSID or Wi-Fi password saved, opening AP"));
        _openAP = true;
        scan(false, true);
    }

    // If AP mode is active, wait until the AP is ready
    if (_openAP)
    {
        int retries = 10;
        while (!_apReady && retries > 0)
        {
            Log->println(F("[DEBUG] Waiting for AP to be ready..."));
            TaskWdtResetAndDelay(1000);
            retries--;
        }

        if (_apReady)
            Log->println(F("[DEBUG] AP is active and ready"));
        else
            Log->println(F("[ERROR] AP did not start correctly!"));
    }
}

void WifiDevice::reconfigure()
{
    _preferences->putString(preference_wifi_ssid, "");
    _preferences->putString(preference_wifi_pass, "");
    Log->disableFileLog();
    TaskWdtResetAndDelay(200);
    restartEsp(RestartReason::ReconfigureWifi);
}

// -----------------------------------------------------------------------
// Scan
// -----------------------------------------------------------------------

void WifiDevice::scan(bool passive, bool async)
{
    if (!_openAP)
    {
        _wifiClientStarted = false;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
    }

    // Wait up to 5 s for the STA interface to start before scanning
    int loop = 0;
    while (!_wifiClientStarted && loop < 50)
    {
        TaskWdtResetAndDelay(100);
        loop++;
    }

    WiFi.scanDelete();
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    Log->println(async ? F("[DEBUG] Wi-Fi async scan started")
                       : F("[DEBUG] Wi-Fi sync scan started"));

    if (passive)
        WiFi.scanNetworks(async, false, true, 75U);
    else
        WiFi.scanNetworks(async);
}

// -----------------------------------------------------------------------
// Connect with external credentials (for WebCfgServer)
// -----------------------------------------------------------------------

bool WifiDevice::testWifiCredentials(const String& ssid,
                                      const String& pass,
                                      uint32_t timeoutMs)
{
    // Save current credentials so we can restore them on failure
    const String prevSsid = _ssid;
    const String prevPass = _pass;

    _ssid = ssid;
    _pass = pass;

    // Apply static IP config if needed
    if (!_ipConfiguration->dhcpEnabled())
    {
        WiFi.config(_ipConfiguration->ipAddress(),
                    _ipConfiguration->dnsServer(),
                    _ipConfiguration->defaultGateway(),
                    _ipConfiguration->subnet());
    }

    WiFi.begin(_ssid, _pass);

    Log->print(F("[DEBUG] Testing WiFi credentials"));

    constexpr uint32_t stepMs = 25;
    uint32_t elapsed = 0;
    while (!isConnected() && elapsed < timeoutMs)
    {
        Log->print(".");
        TaskWdtResetAndDelay(stepMs);
        elapsed += stepMs;
    }
    Log->println("");

    const bool connected = isConnected();

    if (!connected)
    {
        Log->println(F("[INFO] testWifiCredentials: connection failed, "
                       "restoring previous credentials"));
        // Restore original credentials — caller will not save the new ones
        _ssid = prevSsid;
        _pass = prevPass;
    }

    return connected;
}

// -----------------------------------------------------------------------
// Disable auto-restarts (OTA safety)
// -----------------------------------------------------------------------

void WifiDevice::disableAutoRestarts()
{
    _autoRestartEnabled = false;
}

// -----------------------------------------------------------------------
// Status
// -----------------------------------------------------------------------

bool WifiDevice::isConnected() const
{
    return WiFi.isConnected();
}

bool WifiDevice::isApOpen() const
{
    return _openAP;
}

// -----------------------------------------------------------------------
// Network information
// -----------------------------------------------------------------------

String WifiDevice::localIP() const
{
    if (WiFi.getMode() & WIFI_AP)
        return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

String WifiDevice::networkBSSID() const
{
    return WiFi.BSSIDstr();
}

int8_t WifiDevice::signalStrength() const
{
    return WiFi.RSSI();
}

NetworkDeviceType WifiDevice::type() const
{
    return NetworkDeviceType::WiFi;
}

String WifiDevice::deviceName() const
{
    return F("Built-in Wi-Fi");
}

// -----------------------------------------------------------------------
// Internal: connect()
// -----------------------------------------------------------------------

bool WifiDevice::connect()
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_hostname.c_str());

    // Wait up to 5 s for STA interface to start
    int loop = 0;
    while (!_wifiClientStarted && loop < 50)
    {
        TaskWdtResetAndDelay(100);
        loop++;
    }

    int bestConnection = -1;

    if (_preferences->getBool(preference_find_best_rssi, false))
    {
        for (int i = 0; i < _foundNetworks; i++)
        {
            if (_ssid == WiFi.SSID(i))
            {
                Log->println("[INFO] Saved SSID " + _ssid +
                             " found with RSSI: " + String(WiFi.RSSI(i)) +
                             " (" + String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) +
                             " %) and BSSID: " + WiFi.BSSIDstr(i) +
                             " and channel: " + String(WiFi.channel(i)));

                if (bestConnection == -1 || WiFi.RSSI(i) > WiFi.RSSI(bestConnection))
                    bestConnection = i;
            }
        }

        if (bestConnection == -1)
        {
            Log->print(F("[WARNING] No network found with SSID: "));
            Log->println(_ssid);
        }
        else
        {
            Log->println("[INFO] Trying to connect to SSID " + _ssid +
                         " found with RSSI: " + String(WiFi.RSSI(bestConnection)) +
                         " (" + String(constrain((100.0 + WiFi.RSSI(bestConnection)) * 2, 0, 100)) +
                         " %) and BSSID: " + WiFi.BSSIDstr(bestConnection) +
                         " and channel: " + String(WiFi.channel(bestConnection)));
        }
    }

    // Apply static IP config if DHCP is disabled
    if (!_ipConfiguration->dhcpEnabled())
    {
        WiFi.config(_ipConfiguration->ipAddress(),
                    _ipConfiguration->dnsServer(),
                    _ipConfiguration->defaultGateway(),
                    _ipConfiguration->subnet());
    }

    if (bestConnection == -1)
        WiFi.begin(_ssid, _pass);
    else
        WiFi.begin(_ssid, _pass,
                   WiFi.channel(bestConnection),
                   WiFi.BSSID(bestConnection),
                   true);

    Log->print(F("[DEBUG] WiFi connecting"));

    loop = 0;
    while (!isConnected() && loop < 600)
    {
        Log->print(".");
        TaskWdtResetAndDelay(25);
        loop++;
    }
    Log->println("");

    if (!isConnected())
    {
        Log->println(F("[ERROR] Failed to connect within 15 seconds"));

        if (_autoRestartEnabled &&
            _preferences->getBool(preference_restart_on_disconnect, false) &&
            espMillis() > 60000)
        {
            Log->println(F("[INFO] Restart on disconnect watchdog triggered, rebooting"));
            Log->disableFileLog();
            TaskWdtResetAndDelay(100);
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
        else
        {
            Log->println(F("[INFO] Retrying Wi-Fi connection"));
            scan(false, true);
        }

        return false;
    }

    return true;
}

// -----------------------------------------------------------------------
// Internal: openAP()
// -----------------------------------------------------------------------

void WifiDevice::openAP()
{
    if (!_startAP)
        return;

    Log->println(F("[INFO] Starting AP with SSID NukiRestBridge and Password NukiBridgeESP32"));
    _startAP = false;
    WiFi.mode(WIFI_AP);
    TaskWdtResetAndDelay(500);
    WiFi.softAPsetHostname(_hostname.c_str());
    TaskWdtResetAndDelay(500);
    WiFi.softAP(F("NukiRestBridge"), F("NukiBridgeESP32"));
}

// -----------------------------------------------------------------------
// Internal: isWifiConfigured()
// -----------------------------------------------------------------------

bool WifiDevice::isWifiConfigured() const
{
    return _ssid.length() > 0 && _pass.length() > 0;
}

// -----------------------------------------------------------------------
// Internal: onWifiEvent()  (mirrors NukiNetwork::onNetworkEvent Wi-Fi section)
// -----------------------------------------------------------------------

void WifiDevice::onWifiEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    Log->printf("[DEBUG] (WiFi Event) event: %d\r\n", event);

    switch (event)
    {
    case ARDUINO_EVENT_WIFI_READY:
        Log->println(F("[DEBUG] WiFi interface ready"));
        break;

    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        Log->println(F("[DEBUG] Completed scan for access points"));
        _foundNetworks = WiFi.scanComplete();

        for (int i = 0; i < _foundNetworks; i++)
        {
            Log->println("[DEBUG] SSID " + WiFi.SSID(i) +
                         " found with RSSI: " + String(WiFi.RSSI(i)) +
                         " (" + String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) +
                         " %) and BSSID: " + WiFi.BSSIDstr(i) +
                         " and channel: " + String(WiFi.channel(i)));
        }

        if (_openAP)
        {
            openAP();
        }
        else if (_foundNetworks > 0 ||
                 _preferences->getBool(preference_find_best_rssi, false))
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
        _wifiClientStarted = true;
        break;

    case ARDUINO_EVENT_WIFI_STA_STOP:
        Log->println(F("[DEBUG] WiFi clients stopped"));
        if (!_openAP)
            onDisconnected();
        break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Log->println(F("[DEBUG] Connected to access point"));
        if (!_openAP)
            onConnected();
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Log->println(F("[DEBUG] Disconnected from WiFi access point"));
        if (!_openAP)
            onDisconnected();
        break;

    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
        Log->println(F("[DEBUG] Authentication mode of access point has changed"));
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Log->print(F("[DEBUG] Obtained IP address: "));
        Log->println(WiFi.localIP());
        if (!_openAP)
            onConnected();
        break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        Log->println(F("[WARNING] Lost IP address and IP address is reset to 0"));
        if (!_openAP)
            onDisconnected();
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
        Log->println(F("[DEBUG] STA IPv6 is preferred"));
        break;

    case ARDUINO_EVENT_WIFI_AP_START:
        Log->println(F("[DEBUG] WiFi access point started"));
        _apReady = true;
        break;

    case ARDUINO_EVENT_WIFI_AP_STOP:
        Log->println(F("[DEBUG] WiFi access point stopped"));
        _apReady = false;
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

    default:
        Log->print(F("[DEBUG] Unknown WiFi Event: "));
        Log->println(event);
        break;
    }
}

// -----------------------------------------------------------------------
// Internal: onConnected() / onDisconnected()
// -----------------------------------------------------------------------

void WifiDevice::onConnected()
{
    Log->println(F("[INFO] Wi-Fi connected"));
    _connected = true;
}

void WifiDevice::onDisconnected()
{
    if (!_connected)
        return;

    _connected = false;
    Log->println(F("[INFO] Wi-Fi disconnected"));
    connect();
}