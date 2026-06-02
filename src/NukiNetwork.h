#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "networkDevices/NetworkDevice.h"
#include "networkDevices/NetworkDeviceFactory.h"
#include "NukiConstants.h"
#include "NukiLockConstants.h"
#include "IPConfiguration.h"
#include "NetworkDeviceType.h"
#include "NetworkServiceState.h"
#include "ServiceRestartRequest.h"
#include "LockActionResult.h"
#include "ImportExport.h"
#include "HarClient.h"
#include "RestApiServer.h"

/**
 * @brief Manages network interfaces (Wi-Fi, Ethernet), REST API, and Home Automation communication.
 *
 * NukiNetwork handles network connection setup and state changes, DHCP/static IP configuration,
 * automatic failover, HTTP client checks, and a REST web server for external API access.
 */
class NukiNetwork
{
public:
    using ServiceRestartRequest = ::ServiceRestartRequest;

    /**
     * @brief Constructs the NukiNetwork and initializes internal state from preferences.
     *
     * Initializes API configuration, webserver settings, and device type.
     *
     * @param preferences Pointer to the Preferences instance.
     * @param importExport Pointer to the ImportExport instance.
     */
    NukiNetwork(Preferences *preferences, ImportExport* importExport);

    /**
     * @brief Destroys the network instance and cleans up allocated resources.
     *
     * Stops and deletes the webserver and HTTP client, if they exist.
     */
    virtual ~NukiNetwork();

    /**
     * @brief Initializes all network services (API path, hostname, IP configuration, etc.).
     *
     * Sets up default paths, loads preferences, and prepares the device for WiFi or Ethernet usage.
     * Does not actually connect to the network yet.
     */
    void initialize();

    /**
     * @brief Controls the periodic processes, watchdog checks,
     *        reconnection, IP configuration, web server acceptance, etc.
     * @return true if network is active and connection exists, otherwise false.
     */
    bool update();

    /**
     * @brief Performs a new configuration / reconnect (e.g. with a new SSID).
     */
    void reconfigureAdapter();

    /**
     * @brief Stops or restarts WebServer(API)/HTTPClient(HAR) in case of failure.
     */
    void restartNetworkServices(NetworkServiceState status);

    /**
     * @brief Starts a WiFi scan (synchronous or asynchronous).
     */
    void scan(bool passive = false, bool async = true);

    // Data send methods to home automation
    /**
     * @brief Sends an Float value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Float value to send.
     */
    void sendToHAFloat(const char *key, const char *param, const float value, const uint8_t precision = 2);

    /**
     * @brief Sends an integer value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Integer value to send.
     */
    void sendToHAInt(const char *key, const char *param, const int value);

    /**
     * @brief Sends an unsigned integer value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Unsigned integer value to send.
     */
    void sendToHAUInt(const char *key, const char *param, const unsigned int value);

    /**
     * @brief Sends an unsigned long value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Unsigned long value to send.
     */
    void sendToHAULong(const char *key, const char *param, const unsigned long value);

    /**
     * @brief Sends a 64-bit integer value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value 64-bit integer value to send.
     */
    void sendToHALongLong(const char *key, const char *param, int64_t value);

    /**
     * @brief Sends a boolean value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Boolean value to send.
     */
    void sendToHABool(const char *key, const char *param, const bool value);

    /**
     * @brief Sends a string value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value String value to send.
     */
    void sendToHAString(const char *key, const char *param, const char *value);

    /**
     * @brief Sends the BLE address of the lock to the Home Automation system.
     * @param address BLE address as std::string.
     */
    void sendToHALockBleAddress(const std::string &address);

    /**
     * @brief Sends the key turner state to the Home Automation system.
     * @param keyTurnerState Current key turner state.
     * @param lastKeyTurnerState Previously known key turner state.
     */
    void sendToHAKeyTurnerState(const NukiLock::KeyTurnerState &keyTurnerState, const NukiLock::KeyTurnerState &lastKeyTurnerState);

    /**
     * @brief Sends the battery report to the Home Automation system.
     * @param batteryReport Struct containing battery data.
     */
    void sendToHABatteryReport(const NukiLock::BatteryReport &batteryReport);

    /**
     * @brief Sends the BLE RSSI value to the Home Automation system.
     * @param rssi Received signal strength indicator (RSSI).
     */
    void sendToHABleRssi(const int &rssi);

    /**
     * @brief Checks whether the access point is currently open (AP mode).
     * @return True if access point is open.
     */
    bool isApOpen() const;

    /**
     * @brief Checks whether the network connection is active.
     */
    bool isConnected() const;

    /**
     * @brief Checks whether the network gate is open (i.e., network operations are permitted).
     *
     */
    bool networkGateOpen() const;

    /**
     * @brief Checks whether WiFi credentials are configured.
     * @return True if SSID and password are available.
     */
    bool isWifiConfigured() const;

    /**
     * @brief Returns the local IP address.
     */
    String localIP() const;

    /**
     * @brief Returns the network BSSID string (only available for WiFi).
     */
    String networkBSSID() const;

    /**
     * @brief Query signal strength (RSSI), only relevant for WLAN.
     */
    int8_t signalStrength();

    /**
     * @brief Initiates the deactivation of any existing WiFi fallback.
     */
    void clearWifiFallback();

    /**
     * @brief Returns the currently active network device type.
     * @return The active NetworkDeviceType (WiFi or Ethernet).
     */
    const NetworkDeviceType networkDeviceType();

    /**
     * @brief Disables automatic restarts on disconnect.
     */
    void disableAutoRestarts();

    /**
     * @brief Disables the REST API interface.
     */
    void disableAPI();

    /**
     * @brief Disables Home Automation Reporting.
     */
    void disableHAR();

    /**
     * @brief Getter for network service status (API WebServer and HAR HTTPClient).
     */
    NetworkServiceState networkServicesState();

    /**
     * @brief Returns the bitmask of active query commands received via REST API.
     * @return Bitfield of QUERY_COMMAND_* flags.
     */
    uint8_t queryCommands();

    /**
     * @brief Sende HTTP-Response as JSON-string to Client (on request of home automation).
     */
    void sendResponse(const char *jsonResultStr);

    /**
     * @brief Sends a formatted JSON response to the requesting REST client.
     * @param jsonResult The JSON response to be sent as JsonDocument.
     * @param success Whether the response indicates success (HTTP 200) or failure.
     * @param httpCode Optional HTTP status code to return.
     */
    void sendResponse(JsonDocument &jsonResult, const char *message = "", int httpCode = 200);

    /**
     * @brief Sets the callback for lock action requests.
     * @param lockActionReceivedCallback Function pointer to lock action handler.
     */
    void setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *value));

    void setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *value));

    void setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char *command, const uint &id, const String &name, const String &code, const int &enabled));

    /**
     * @brief Sets the callback for time control command requests.
     * @param timeControlCommandReceivedReceivedCallback Function pointer to time control handler.
     */
    void setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char *value));

    void setAuthCommandReceivedCallback(void (*authCommandReceivedReceivedCallback)(const char* value));
    
    /**
     * @brief Loads saved WiFi and IP configuration settings.
     */
    void readSettings();

    /**
     * @brief Sets a new API token and stores it in preferences.
     */
    void assignNewApiToken();

    /**
     * @brief Returns the current API token string.
     * @return Pointer to the API token.
     */
    char *getApiToken();

    /**
     * @brief Consumes any pending service restart request.
     * @return The pending ServiceRestartRequest value.
     */
    ServiceRestartRequest consumeServiceRestartRequest();

    /**
     * @brief Requests a service restart, optionally with reconnection.
     * @param reconnect Whether to attempt reconnection after restart.
     */
    void requestServiceRestart(bool reconnect = false);

    /**
     * @brief Tests the validity of Wi-Fi credentials.
     *
     * @param ssid  Wi-Fi network name.
     * @param pass  Wi-Fi password.
     * @return true  if the credentials are valid and a connection can be established within the timeout.
     * @return false if the connection failed or the device is not Wi-Fi.
     * @param timeoutMs  Time in milliseconds to wait for a successful connection.
     */
    bool testWifiCredentials(const String& ssid, const String& pass, uint32_t timeoutMs);

private:
    /**
     * @brief Sets up the hardware type (WiFi or Ethernet) based on preferences or fallback logic.
     *
     * This method evaluates the saved hardware mode or fallback flags and sets the appropriate
     * device type for further initialization.
     */
    void setupDevice();

    /**
     * @brief Starts WebServer (API) and HTTPClient (HAR) if necessary.
     */
    void startNetworkServices();

    /**
     * @brief Runs tests for WebServer (API) and HTTPClient (HAR) (e.g., ping).
     */
    NetworkServiceState testNetworkServices();

    Preferences *_preferences;                                                // Preferences handler for NVS access
    ImportExport* _importExport;                                              // Import/Export handler                                                                 //
    IPConfiguration *_ipConfiguration = nullptr;                              // IP configuration helper (DHCP/static)
    String _hostname;                                                         // Hostname used on the network (WiFi or Ethernet)
                                                                              //
    bool _firstBootAfterDeviceChange = false;                                 // True after switching from WiFi to Ethernet or vice versa
    bool _webCfgEnabled = true;                                               // Whether the Web Config interface is enabled
    bool _restartOnDisconnect = false;                                        // Whether the device should reboot on disconnect
    NetworkServiceState _networkServicesState = NetworkServiceState::UNKNOWN; // Current state of network services
                                                                              //
    ServiceRestartRequest _pendingServiceRestart = ServiceRestartRequest::None;  // Pending service restart request
                                                                              //
    int64_t _lastConnectedTs = 0;                                             // Last time a successful connection occurred
    int64_t _lastNetworkServiceTs = 0;                                        // Last time services were checked
                                                                              //
    int _networkTimeout = 0;                                                  // Timeout in ms for network operations
    int _networkServicesConnectCounter = 0;                                   // Counter for tracking connection attempts
                                                                              //
    NetworkDevice* _device = nullptr;                                         // Owned - is created in initialize()
    NetworkDeviceType _networkDeviceType = NetworkDeviceType::UNDEFINED;      // WiFi or Ethernet
                                                                              //
    HarClient* _harClient = nullptr;                                          // Home Automation Reporting client (owned)
    RestApiServer* _restApiServer = nullptr;                                  // REST API server (owned)     

};

// Globale oder externe Variablen
extern bool ethCriticalFailure;
extern bool wifiFallback;
extern bool disableNetwork;
extern bool forceEnableWebCfgServer;