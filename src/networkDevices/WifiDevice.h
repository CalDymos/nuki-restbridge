#pragma once

#include <WiFi.h>
#include "esp_wifi.h"
#include "NetworkDevice.h"

/**
 * @brief Wi-Fi implementation of NetworkDevice.
 *
 * Handles station mode, AP fallback, network scanning, event-driven
 * connection management, and RSSI access.
 *
 * Connection strategy (following NukiHub pattern):
 *   - If SSID + password are saved AND preference_find_best_rssi is false:
 *     connect directly (no scan).
 *   - If SSID + password are saved AND preference_find_best_rssi is true:
 *     scan first, pick the AP with the strongest signal, then connect.
 *   - If SSID or password are missing:
 *     open a configuration Access Point (SSID: NukiRestBridge).
 *
 * Event handling mirrors the original NukiNetwork::onNetworkEvent() Wi-Fi
 * section exactly, to avoid any behaviour change during the refactoring.
 */
class WifiDevice : public NetworkDevice
{
public:
    /**
     * @brief Construct a WifiDevice.
     *
     * @param hostname         mDNS / DHCP hostname to advertise.
     * @param preferences      NVS preferences (not owned).
     * @param ipConfiguration  IP settings for static-IP mode (not owned).
     */
    WifiDevice(const String& hostname,
               Preferences* preferences,
               const IPConfiguration* ipConfiguration);

    // -----------------------------------------------------------------------
    // NetworkDevice interface
    // -----------------------------------------------------------------------

    /**
     * @brief Read SSID/password from NVS, register the event handler,
     *        decide between scan/direct-connect/AP mode, and wait for the
     *        connection or AP to become ready.
     *
     * Direct-connect path: avoids the scan overhead when find_best_rssi is
     * disabled — matches NukiHub behaviour.
     * Scan path: used when find_best_rssi is active; waits for
     * _wifiClientStarted before starting the scan.
     */
    void initialize() override;

    /**
     * @brief No-op for Wi-Fi — state changes are event-driven.
     */
    void update() override {}

    /**
     * @brief Clear SSID and password from NVS and restart.
     */
    void reconfigure() override;

    /**
     * @brief Start a Wi-Fi network scan.
     *
     * Waits up to 5 s for the Wi-Fi client to initialise (_wifiClientStarted)
     * before calling WiFi.scanNetworks().
     *
     * @param passive  Passive scan (listen only, no probe requests).
     * @param async    Non-blocking scan (return immediately).
     */
    void scan(bool passive = false, bool async = true) override;

    /**
     * @brief Test whether the given credentials can establish a Wi-Fi connection.
     *
     * Sets the credentials, attempts to connect, and waits up to timeoutMs.
     * Does NOT save credentials — WebCfgServer saves them only on success.
     * Restores previous _ssid/_pass if the attempt fails.
     *
     * @param ssid       Network name to test.
     * @param pass       Password to test.
     * @param timeoutMs  Maximum wait time in milliseconds.
     * @return true  if connected within timeoutMs.
     * @return false if the connection failed.
     */
    bool testWifiCredentials(const String& ssid,
                             const String& pass,
                             uint32_t timeoutMs) override;

    /**
     * @brief Disable reconnect-on-disconnect and restart-on-disconnect.
     *
     * Called during OTA to prevent the watchdog from rebooting mid-flash.
     */
    void disableAutoRestarts() override;

    // -----------------------------------------------------------------------
    // Status
    // -----------------------------------------------------------------------

    /** @brief True when the station interface has an IP address. */
    bool isConnected() const override;

    /** @brief True when in Access Point mode. */
    bool isApOpen() const override;

    /**
     * @brief True only when connected (Wi-Fi must be online to reach services).
     */
    bool networkGateOpen() const override { return isConnected(); }

    // -----------------------------------------------------------------------
    // Network information
    // -----------------------------------------------------------------------

    /**
     * @brief Return the current IP address string.
     *
     * Returns the AP IP when in AP mode, or the STA IP when connected.
     */
    String localIP() const override;

    /** @brief Return the connected AP's BSSID (MAC) string. */
    String networkBSSID() const override;

    /** @brief Return the current RSSI in dBm. */
    int8_t signalStrength() const override;

    /** @brief Return NetworkDeviceType::WiFi. */
    NetworkDeviceType type() const override;

    /** @brief Return "Built-in Wi-Fi". */
    String deviceName() const override;

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Connect to _ssid using _pass.
     *
     * Waits up to 5 s for _wifiClientStarted. If preference_find_best_rssi
     * is active, picks the scan result with the best signal. Applies static
     * IP config when DHCP is disabled. Falls back to a new scan if the
     * connection times out.
     *
     * @return true  if connected within the timeout.
     * @return false if the connection attempt failed.
     */
    bool connect();

    /**
     * @brief Start the soft Access Point.
     *
     * No-op if _startAP is false (AP has already been started).
     * SSID: "NukiRestBridge", Password: "NukiBridgeESP32".
     */
    void openAP();

    /**
     * @brief Return true if both SSID and password are non-empty.
     */
    bool isWifiConfigured() const;

    /**
     * @brief Wi-Fi event dispatcher.
     *
     * Registered via WiFi.onEvent() in initialize().
     * Handles: READY, SCAN_DONE, STA_START, STA_STOP, STA_CONNECTED,
     *          STA_DISCONNECTED, STA_AUTHMODE_CHANGE, STA_GOT_IP,
     *          STA_LOST_IP, STA_GOT_IP6, AP_START, AP_STOP,
     *          AP_STACONNECTED, AP_STADISCONNECTED, AP_STAIPASSIGNED,
     *          AP_PROBEREQRECVED, AP_GOT_IP6.
     *
     * Mirrors the original NukiNetwork::onNetworkEvent() Wi-Fi section.
     */
    void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info);

    /**
     * @brief Called when the station interface obtains an IP address.
     *
     * Sets _connected = true and logs the event.
     * Skipped when in AP mode (_openAP == true).
     */
    void onConnected();

    /**
     * @brief Called on station disconnect events.
     *
     * Sets _connected = false and calls connect() to retry,
     * unless _autoRestartEnabled is false or espMillis() < 60 s.
     */
    void onDisconnected();

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    /** Stored Wi-Fi SSID, read from NVS in initialize(). */
    String _ssid;

    /** Stored Wi-Fi password, read from NVS in initialize(). */
    String _pass;

    /** True when the soft Access Point is (or should be) active. */
    bool _openAP = false;

    /** True until openAP() has been called for the first time. */
    bool _startAP = true;

    /** True once the AP is fully initialised (ANDROID_EVENT_WIFI_AP_START). */
    bool _apReady = false;

    /**
     * @brief True once the Wi-Fi STA interface has started.
     *
     * Set in ARDUINO_EVENT_WIFI_STA_START. scan() and connect() wait up
     * to 5 s for this flag before calling the WiFi driver (NukiHub pattern).
     */
    bool _wifiClientStarted = false;

    /** True when the station has a valid IP address. */
    bool _connected = false;

    /** Number of networks found during the last scan. */
    int _foundNetworks = 0;

    /**
     * @brief When false, onDisconnected() skips the reconnect attempt.
     *
     * Set to false by disableAutoRestarts() during OTA.
     */
    bool _autoRestartEnabled = true;
};