#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "IPConfiguration.h"
#include "NetworkDeviceType.h"

/**
 * @brief Abstract base class for network hardware adapters.
 *
 * WifiDevice and EthernetDevice both derive from this class.
 * NukiNetwork only sees this interface — all hardware-specific code
 * is encapsulated in the concrete subclasses.
 *
 * Lifecycle (called by NukiNetwork in this order):
 *   1. Factory::create()  — constructs the concrete device
 *   2. initialize()       — brings the hardware up, registers event handlers
 *   3. update()           — called every network-task loop for maintenance work
 *   4. reconfigure()      — clears stored config + restarts (usually triggers reboot)
 *
 * Thread safety: all methods must be called from the same task (networkTask).
 */
class NetworkDevice
{
public:
    /**
     * @brief Construct a NetworkDevice.
     *
     * @param hostname         mDNS / DHCP hostname for this device.
     * @param preferences      Pointer to NVS preferences (not owned).
     * @param ipConfiguration  Pointer to IP settings (not owned).
     */
    NetworkDevice(const String& hostname,
                  Preferences* preferences,
                  const IPConfiguration* ipConfiguration)
        : _hostname(hostname),
          _preferences(preferences),
          _ipConfiguration(ipConfiguration)
    {}

    /** @brief Virtual destructor. */
    virtual ~NetworkDevice() = default;

    // -----------------------------------------------------------------------
    // Mandatory lifecycle interface
    // -----------------------------------------------------------------------

    /**
     * @brief Bring the hardware up and register event handlers.
     *
     * Called once by NukiNetwork::initialize() after the hostname is set.
     * For Wi-Fi: reads SSID/password, sets hostname, starts scan or connect.
     * For Ethernet: checks ethCriticalFailure, initialises ETH hardware.
     */
    virtual void initialize() = 0;

    /**
     * @brief Periodic maintenance, called every network-task loop.
     *
     * For Wi-Fi:    no-op (events drive state changes).
     * For Ethernet: retries static IP configuration when DHCP is disabled
     *               and the hardware has not yet received an assigned IP.
     */
    virtual void update() {}

    /**
     * @brief Clear stored network config and trigger a reboot to re-configure.
     *
     * For Wi-Fi:    clears SSID and password preferences, then restarts.
     * For Ethernet: restarts to re-detect ETH hardware.
     */
    virtual void reconfigure() = 0;

    // -----------------------------------------------------------------------
    // Optional interface — defaults are no-ops or safe fallbacks
    // -----------------------------------------------------------------------

    /**
     * @brief Trigger a Wi-Fi scan.
     *
     * No-op for Ethernet. WifiDevice overrides this.
     *
     * @param passive  Use passive scanning (listen only, no probe requests).
     * @param async    Return immediately (true) or block until scan is done.
     */
    virtual void scan(bool passive = false, bool async = true) {}

    /**
     * @brief Test whether the given credentials can establish a Wi-Fi connection.
     *
     * Used by WebCfgServer::processConnectionSettings() to validate new
     * credentials before saving them to NVS. If the connection succeeds
     * within timeoutMs, the caller saves the credentials; otherwise they
     * are discarded.
     *
     * No-op / returns false for Ethernet.
     *
     * @param ssid       Wi-Fi network name to test.
     * @param pass       Wi-Fi password to test.
     * @param timeoutMs  Maximum time in milliseconds to wait for a connection.
     * @return true  if the connection was established within timeoutMs.
     * @return false if the connection failed or the device is not Wi-Fi.
     */
    virtual bool testWifiCredentials(const String& ssid, const String& pass, uint32_t timeoutMs) { return false; }

    /**
     * @brief Suppress automatic reconnect / restart-on-disconnect behaviour.
     *
     * Called by WebCfgServer during OTA to prevent the device from rebooting
     * itself mid-update. No-op in the default implementation.
     */
    virtual void disableAutoRestarts() {}

    // -----------------------------------------------------------------------
    // Status queries
    // -----------------------------------------------------------------------

    /**
     * @brief Return true if the device has an active network connection.
     *
     * For Wi-Fi:    delegates to WiFi.isConnected().
     * For Ethernet: true once ARDUINO_EVENT_ETH_GOT_IP has fired.
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Return true if the device is running in Access Point mode.
     *
     * Always false for Ethernet. WifiDevice overrides this.
     */
    virtual bool isApOpen() const { return false; }

    /**
     * @brief Return true when the network path to external services is open.
     *
     * For Wi-Fi:    equals isConnected() (must be online to reach services).
     * For Ethernet: always true (LAN is always locally accessible; HAR and
     *               REST API may be available even without a WAN route).
     */
    virtual bool networkGateOpen() const { return isConnected(); }

    // -----------------------------------------------------------------------
    // Network information
    // -----------------------------------------------------------------------

    /**
     * @brief Return the current local IP address as a string.
     *
     * Returns "0.0.0.0" if not connected.
     */
    virtual String localIP() const = 0;

    /**
     * @brief Return the connected AP's BSSID string.
     *
     * Returns an empty string for Ethernet. WifiDevice overrides this.
     */
    virtual String networkBSSID() const { return ""; }

    /**
     * @brief Return the current Wi-Fi RSSI value in dBm.
     *
     * Returns 127 for Ethernet (= "not available").
     * HarClient::update() skips RSSI reporting when this value is 127.
     * WifiDevice overrides this with WiFi.RSSI().
     */
    virtual int8_t signalStrength() const { return 127; }

    /**
     * @brief Return the concrete device type (WiFi or ETH).
     */
    virtual NetworkDeviceType type() const = 0;

    /**
     * @brief Return a human-readable device name for logging and the web UI.
     */
    virtual String deviceName() const = 0;

protected:
    /** mDNS / DHCP hostname. Set by NukiNetwork before calling initialize(). */
    String _hostname;

    /** NVS preferences handle. Not owned — must outlive this object. */
    Preferences* _preferences;

    /** IP address configuration (static or DHCP). Not owned. */
    const IPConfiguration* _ipConfiguration;
};