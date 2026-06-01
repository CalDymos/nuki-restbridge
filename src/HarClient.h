#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <NetworkUdp.h>
#include <string>
#include "NukiLockConstants.h"
#include "NetworkServiceState.h"

/**
 * @brief Manages all outbound communication to a Home Automation (HA) system.
 *
 * HarClient encapsulates the entire HAR (Home Automation Reporting) subsystem,
 * which was previously embedded in NukiNetwork. It supports two transport modes:
 *   - UDP  (mode 0): sends plain "param=value" datagrams
 *   - REST (mode 1): sends HTTP GET or POST requests
 *
 * Lifecycle:
 *   1. Construct with a Preferences pointer.
 *   2. Call initialize() once to read settings and create the transport client.
 *   3. Call update() from the network task every loop to handle periodic sends
 *      (WiFi RSSI, uptime, restart reason, firmware info, heap).
 *   4. Call sendXxx() methods from NukiWrapper / NukiNetwork as data changes.
 *   5. Call test() to verify connectivity; call restart() on failure.
 *   6. Call disable() to shut down HAR (e.g. before reboot).
 *
 * Thread safety: all methods must be called from the same task (networkTask).
 * No internal mutex is provided.
 */
class HarClient
{
public:
    /**
     * @brief Construct a HarClient.
     *
     * Does not allocate any network resources yet — call initialize() first.
     *
     * @param preferences  Pointer to the ESP32 NVS preferences instance.
     *                     Must remain valid for the lifetime of this object.
     */
    explicit HarClient(Preferences* preferences);

    /**
     * @brief Destructor. Releases HTTPClient / NetworkUDP if allocated.
     */
    ~HarClient();

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Read all HAR settings from NVS and create the transport client.
     *
     * Must be called once before any send or test method. Safe to call again
     * after a settings change (re-reads all preferences and recreates the
     * transport client).
     *
     * Fixes the original NukiNetwork bug where _homeAutomationUser and
     * _homeAutomationPassword were stored in local variables instead of members,
     * causing HA authentication to never work.
     */
    void initialize();

    /**
     * @brief Allocate the transport client (HTTPClient or NetworkUDP).
     *
     * Called by NukiNetwork::startNetworkServices(). Safe to call if the
     * client is already allocated (no-op in that case).
     */
    void start();

    /**
     * @brief Test connectivity to the HA server (ping + optional HTTP GET).
     *
     * @return true  if the transport client exists and the server is reachable.
     * @return false if the client is null or the ping / test GET failed.
     */
    bool test();

    /**
     * @brief Delete and recreate the transport client.
     *
     * Called by NukiNetwork::restartNetworkServices() when test() returns false.
     * Performs a clean teardown of the existing client before re-allocating.
     */
    void restart();

    /**
     * @brief Disable HAR reporting (e.g. on API request or before reboot).
     *
     * Sets _enabled = false. Further send calls become no-ops.
     * Does NOT free the transport client — call restart() or destructor for that.
     */
    void disable();

    /**
     * @brief Re-read interval and debug settings from NVS.
     *
     * Should be called whenever the user saves new configuration via WebCfgServer.
     * Reads: rssi_send_interval, maintenance_send_interval, send_debug_info.
     */
    void readSettings();

    // -----------------------------------------------------------------------
    // Status
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if HAR is enabled in preferences and disable() has not
     *        been called.
     */
    bool isEnabled() const;

    /**
     * @brief Returns the current health flag set by NukiNetwork after test().
     *
     * NukiNetwork calls setOk(false) when testNetworkServices() detects a HAR
     * failure, and setOk(true) after a successful restart().
     */
    bool isOk() const;

    /**
     * @brief Set the health flag from NukiNetwork.
     *
     * @param ok  true = client is reachable; false = client is in error state.
     */
    void setOk(bool ok);

    // -----------------------------------------------------------------------
    // Periodic update (call from networkTask every loop)
    // -----------------------------------------------------------------------

    /**
     * @brief Handle time-driven periodic HAR sends.
     *
     * Must be called every loop iteration from networkTask. Manages:
     *   - WiFi RSSI reporting (interval: _rssiInterval)
     *   - Uptime reporting (interval: _maintenanceInterval)
     *   - One-shot boot information (restart reason, firmware version, build)
     *   - Free heap reporting (only when sendDebugInfo is true)
     *
     * @param ts            Current system time in milliseconds (espMillis()).
     * @param wifiRssi      Current WiFi RSSI value (-1 for Ethernet / not connected).
     */
    void update(int64_t ts, int8_t wifiRssi);

    // -----------------------------------------------------------------------
    // Data send methods (typed overloads — delegate to sendData())
    // -----------------------------------------------------------------------

    /**
     * @brief Send a float value to the HA system.
     *
     * @param key       REST path key (used in REST mode).
     * @param param     UDP parameter name (used in UDP mode).
     * @param value     Float value to send.
     * @param precision Number of decimal places (default: 2).
     */
    void sendFloat(const char* key, const char* param,
                   float value, uint8_t precision = 2);

    /**
     * @brief Send a signed integer value to the HA system.
     *
     * @param key   REST path key.
     * @param param UDP parameter name.
     * @param value Integer value to send.
     */
    void sendInt(const char* key, const char* param, int value);

    /**
     * @brief Send an unsigned integer value to the HA system.
     *
     * @param key   REST path key.
     * @param param UDP parameter name.
     * @param value Unsigned integer value to send.
     */
    void sendUInt(const char* key, const char* param, unsigned int value);

    /**
     * @brief Send an unsigned long value to the HA system.
     *
     * @param key   REST path key.
     * @param param UDP parameter name.
     * @param value Unsigned long value to send.
     */
    void sendULong(const char* key, const char* param, unsigned long value);

    /**
     * @brief Send a 64-bit signed integer to the HA system.
     *
     * @param key   REST path key.
     * @param param UDP parameter name.
     * @param value 64-bit integer value to send.
     */
    void sendLongLong(const char* key, const char* param, int64_t value);

    /**
     * @brief Send a boolean value to the HA system.
     *
     * Encoded as "1" (true) or "0" (false).
     *
     * @param key   REST path key.
     * @param param UDP parameter name.
     * @param value Boolean value to send.
     */
    void sendBool(const char* key, const char* param, bool value);

    /**
     * @brief Send a string value to the HA system.
     *
     * @param key   REST path key.
     * @param param UDP parameter name.
     * @param value Null-terminated C-string to send.
     */
    void sendString(const char* key, const char* param, const char* value);

    // -----------------------------------------------------------------------
    // Higher-level send methods (read key/param from preferences)
    // -----------------------------------------------------------------------

    /**
     * @brief Send the BLE MAC address of the Nuki lock to the HA system.
     *
     * Reads the configured key/param paths from preferences
     * (preference_har_key_ble_address / preference_har_param_ble_address).
     *
     * @param address BLE address as std::string (e.g. "AA:BB:CC:DD:EE:FF").
     */
    void sendLockBleAddress(const std::string& address);

    /**
     * @brief Send the full KeyTurnerState to the HA system.
     *
     * Sends lock state, LockNGo timer, trigger, night mode, completion status,
     * door sensor state, battery levels, keypad critical, and BLE strength.
     * Each field is only sent when it changes relative to lastKeyTurnerState.
     *
     * @param keyTurnerState      Current state received from the lock.
     * @param lastKeyTurnerState  Previously known state (for change detection).
     */
    void sendKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState,
                            const NukiLock::KeyTurnerState& lastKeyTurnerState);

    /**
     * @brief Send a battery report to the HA system.
     *
     * Sends: battery voltage, battery drain, max turn current, lock distance.
     *
     * @param batteryReport  Battery report struct received from the lock.
     */
    void sendBatteryReport(const NukiLock::BatteryReport& batteryReport);

    /**
     * @brief Send the current BLE RSSI value to the HA system.
     *
     * Reads key/param from preferences
     * (preference_har_key_ble_rssi / preference_har_param_ble_rssi).
     *
     * @param rssi  BLE RSSI in dBm.
     */
    void sendBleRssi(const int& rssi);

private:
    // -----------------------------------------------------------------------
    // Internal transport
    // -----------------------------------------------------------------------

    /**
     * @brief Core send function: dispatches to UDP or REST based on _mode.
     *
     * In UDP mode, sends a "param=value" datagram to _address:_port.
     * In REST mode, builds an HTTP URL and issues GET or POST via _httpClient.
     *
     * Guards: returns immediately if !_enabled, !_isOk, or the required
     * key/param string is empty.
     *
     * @param key   REST path component (e.g. "api/lock/state"). Used in REST mode.
     * @param param UDP parameter name. Used in UDP mode.
     * @param value String representation of the value to send.
     */
    void sendData(const char* key, const char* param, const char* value);

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    /** NVS preferences handle. Not owned — must outlive this object. */
    Preferences* _preferences;

    /** HTTP client for REST mode (mode == 1). Owned by this class. */
    HTTPClient*  _httpClient  = nullptr;

    /** UDP client for UDP mode (mode == 0). Owned by this class. */
    NetworkUDP*  _udpClient   = nullptr;

    /** True if HAR is enabled in preferences and disable() has not been called. */
    bool    _enabled           = false;

    /**
     * @brief Health flag managed by NukiNetwork.
     *
     * Starts as true. Set to false by NukiNetwork when test() detects a failure.
     * Set back to true after a successful restart().
     * When false, all sendData() calls are no-ops to avoid flooding logs with
     * HTTP errors when the HA server is known to be unreachable.
     */
    bool    _isOk              = true;

    /** Target IP address or hostname of the HA server. */
    String  _address;

    /** Optional HTTP Basic Auth username. Previously broken (local var shadow). */
    String  _user;

    /** Optional HTTP Basic Auth password. Previously broken (local var shadow). */
    String  _password;

    /**
     * @brief Transport mode.
     * 0 = UDP (send "param=value" datagrams)
     * 1 = REST (HTTP GET or POST)
     */
    int     _mode              = 0;

    /**
     * @brief HTTP request method in REST mode.
     * 0 = GET  (value appended to URL path)
     * 1 = POST (value in request body)
     */
    int     _restMode          = 0;

    /** Network port for both UDP datagrams and HTTP requests. Default: 80. */
    int     _port              = 80;

    /** WiFi RSSI send interval in milliseconds (0 = disabled). */
    int     _rssiInterval      = 60000;

    /** Maintenance telemetry send interval in milliseconds (0 = disabled). */
    int     _maintenanceInterval = 0;

    /** Timestamp of last WiFi RSSI transmission (espMillis()). */
    int64_t _lastRssiTs        = 0;

    /** Timestamp of last maintenance telemetry transmission (espMillis()). */
    int64_t _lastMaintenanceTs = 0;

    /** Last reported uptime in minutes (for change detection). */
    int64_t _publishedUpTime   = 0;

    /** Last reported WiFi RSSI value (for change detection). */
    int8_t  _lastWifiRssi      = 127;

    bool _sendDebugInfo = false;                                              // Whether extended debug info should be published

    /**
     * @brief True until the first KeyTurnerState has been sent.
     *
     * Forces all fields to be transmitted on the first call to
     * sendKeyTurnerState(), regardless of change detection.
     * Mirrors the original NukiNetwork::_firstTunerStateSent behaviour.
     */
    bool    _firstTunerStateSent = true;

};