#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <ETH.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <NetworkUdp.h>
#include <WiFiClient.h>
#include "ESP32Ping.h"
#include <esp_mac.h>
#include <ArduinoJson.h>

#include "NukiConstants.h"
#include "NukiLockConstants.h"
#include "RestApiPaths.h"
#include "IPConfiguration.h"
#include "NetworkDeviceType.h"
#include "BridgeApiToken.h"
#include "NetworkServiceState.h"
#include "QueryCommand.h"
#include "LockActionResult.h"

/**
 * @brief Manages network interfaces (Wi-Fi, Ethernet), REST API, and Home Automation communication.
 *
 * NukiNetwork handles network connection setup and state changes, DHCP/static IP configuration,
 * automatic failover, HTTP client checks, and a REST web server for external API access.
 */
class NukiNetwork
{
public:
    /**
     * @brief Constructs the NukiNetwork and initializes internal state from preferences.
     *
     * Initializes API configuration, webserver settings, and device type.
     *
     * @param preferences Pointer to the Preferences instance.
     * @param buffer Reusable character buffer (e.g., for REST JSON responses).
     * @param bufferSize Size of the buffer.
     */
    NukiNetwork(Preferences *preferences, char *buffer, size_t bufferSize);

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
    void reconfigure();

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
     * @brief Checks whether (at least) the WiFi module is connected.
     *
     */
    bool isWifiConnected();

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
     * @brief Sends arbitrary requests to Home Automation (e.g. to provide status values).
     */
    void sendDataToHA(const char *key, const char *param, const char *value);

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
    void sendResponse(JsonDocument &jsonResult, bool success = true, int httpCode = 200);

    /**
     * @brief Sets the callback for lock action requests.
     * @param lockActionReceivedCallback Function pointer to lock action handler.
     */
    void setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char *value));

    /**
     * @brief Sets the callback for time control command requests.
     * @param timeControlCommandReceivedReceivedCallback Function pointer to time control handler.
     */
    void setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char *value));

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

private:
    /**
     * @brief Sets up the hardware type (WiFi or Ethernet) based on preferences or fallback logic.
     *
     * This method evaluates the saved hardware mode or fallback flags and sets the appropriate
     * device type for further initialization.
     */
    void setupDevice();

    /**
     * @brief Initializes WiFi (and optionally calls connect()).
     */
    void initializeWiFi();

    /**
     * @brief Initializes Ethernet (DHCP or static IP).
     */
    void initializeEthernet();

    /**
     * @brief Starts WebServer (API) and HTTPClient (HAR) if necessary.
     */
    void startNetworkServices();

    /**
     * @brief Static entry point for REST request processing (WebServer handler).
     * @param path Full request URI path.
     * @param server Reference to the WebServer instance.
     */
    static void onRestDataReceivedCallback(const char *path, WebServer &server);

    /**
     * @brief Handles the logic for REST requests internally.
     * @param path Full request URI path.
     * @param server Reference to the WebServer instance.
     */
    void onRestDataReceived(const char *path, WebServer &server);

    /**
     * @brief Handles logic for shutdown REST request.
     * @param path Full request URI path.
     * @param server Reference to the WebServer instance.
     */
    void onShutdownReceived(const char *path, WebServer &server);

    /**
     * @brief Runs tests for WebServer (API) and HTTPClient (HAR) (e.g., ping).
     */
    NetworkServiceState testNetworkServices();

    /**
     * @brief Stops or restarts WebServer(API)/HTTPClient(HAR) in case of failure.
     */
    void restartNetworkServices(NetworkServiceState status);

    /**
     * @brief Handles network-related events (WiFi/Ethernet callback).
     */
    void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);

    /**
     * @brief Internal callback when a network connection is successfully established.
     */
    void onConnected();

    /**
     * @brief Connects using saved SSID/password or opens access point.
     */
    bool connect();

    /**
     * @brief Opens an access point if no SSID is configured.
     */
    void openAP();

    /**
     * @brief Handles disconnect if WiFi or Ethernet connection is lost.
     */
    void onDisconnected();

    /**
     * @brief Checks whether a REST path starts with the configured bridge path.
     * @param fullPath Full path received from the request.
     * @param subPath Path to compare with.
     * @return True if prefixed path matches.
     */
    bool comparePrefixedPath(const char *fullPath, const char *subPath);

    /**
     * @brief Combines bridge path with the subpath.
     * @param path Path to append.
     * @param outPath Output buffer.
     */
    void buildApiPath(const char *path, char *outPath);

    /**
     * @brief Extracts and serializes HTTP arguments from the REST request into the internal buffer.
     * @param server Reference to the WebServer instance.
     * @return Pointer to the serialized argument string (JSON or plain text).
     */
    char *getArgs(WebServer &server);

    // Singleton instance
    static NukiNetwork *_inst;

    Preferences *_preferences;                                                // Preferences handler for NVS access
    IPConfiguration *_ipConfiguration = nullptr;                              // IP configuration helper (DHCP/static)
    String _hostname;                                                         // Hostname used on the network (WiFi or Ethernet)
    String _WiFissid;                                                         // Stored WiFi SSID
    String _WiFipass;                                                         // Stored WiFi password
                                                                              //
    BridgeApiToken *_apitoken = nullptr;                                      // Token used for REST API authentication
    bool _firstBootAfterDeviceChange = false;                                 // True after switching from WiFi to Ethernet or vice versa
    bool _webCfgEnabled = true;                                               // Whether the Web Config interface is enabled
    bool _apiEnabled = false;                                                 // Whether REST API is enabled
    bool _openAP = false;                                                     // Whether Access Point mode is active
    bool _APisReady = false;                                                  // True if AP is initialized and ready
    bool _startAP = true;                                                     // True if AP should be started due to no WiFi
    bool _connected = false;                                                  // Network connection state
    bool _ethConnected = false;                                               // Flag to temporarily store (ARDUINO_EVENT_ETH_CONNECTED)
    bool _lockEnabled = false;                                                // Whether lock control via API is enabled
    bool _hardwareInitialized = false;                                        // Flag indicating that network hardware is initialized
    bool _sendDebugInfo = false;                                              // Whether extended debug info should be published
    bool _restartOnDisconnect = false;                                        // Whether the device should reboot on disconnect
    bool _firstTunerStateSent = true;                                         // Ensures the first lock state is always sent
    NetworkServiceState _networkServicesState = NetworkServiceState::UNKNOWN; // Current state of network services
                                                                              //
    String _keypadCommandName = "";                                           // Temporary buffer for keypad command name
    String _keypadCommandCode = "";                                           // Temporary buffer for keypad command code
    uint _keypadCommandId = 0;                                                // Temporary buffer for keypad command ID
    int _keypadCommandEnabled = 1;                                            // Temporary buffer for keypad enabled state
    uint8_t _queryCommands = 0;                                               // Bitmask of active QUERY_COMMAND_* values
                                                                              //
    int64_t _checkIpTs = -1;                                                  // Last time IP was validated
    int64_t _lastConnectedTs = 0;                                             // Last time a successful connection occurred
    int64_t _lastMaintenanceTs = 0;                                           // Last time maintenance was performed
    int64_t _lastNetworkServiceTs = 0;                                        // Last time services were checked
    int64_t _publishedUpTime = 0;                                             // Last time uptime was sent
    int64_t _lastRssiTs = 0;                                                  // Last time RSSI was transmitted
                                                                              //
    WebServer *_server = nullptr;                                             // REST API web server instance
    HTTPClient *_httpClient = nullptr;                                        // HTTP client for sending Data to HA
    NetworkUDP *_udpClient = nullptr;                                         // UDP client for sending Data to HA
    int _foundNetworks = 0;                                                   // Number of WiFi networks found during last scan
    int _networkTimeout = 0;                                                  // Timeout in ms for network operations
    int _rssiSendInterval = 0;                                                // Interval for RSSI reporting
    int _MaintenanceSendIntervall = 0;                                        // Interval for periodic maintenance
    int _networkServicesConnectCounter = 0;                                   // Counter for tracking connection attempts
                                                                              //
    NetworkDeviceType _networkDeviceType = NetworkDeviceType::UNDEFINED;      // WiFi or Ethernet
    int8_t _lastRssi = 127;                                                   // Last known RSSI value
                                                                              //
    bool _homeAutomationEnabled = false;                                      // Whether HA communication is enabled
    String _homeAutomationAdress;                                             // Target IP or hostname of HA server
    String _homeAutomationUser;                                               // Optional HA user name
    String _homeAutomationPassword;                                           // Optional HA password
    int _homeAutomationMode;                                                  // current Mode for data reporting to Ha (0=UDP/1=REST)
    int _homeAutomationRestMode;                                              // Rest Mode (0=GET/1=POST)
    int _homeAutomationPort;                                                  // Port for HA
                                                                              //
    char *_buffer;                                                            // Shared buffer for response generation
    const size_t _bufferSize;                                                 // Size of the shared buffer
    char _apiBridgePath[129] = {0};                                           // API base path (e.g. "/bridge")
    char _apiLockPath[129] = {0};                                             // API path for lock-related commands
    int _apiPort;                                                             // REST API server port

    // Callback handlers
    LockActionResult (*_lockActionReceivedCallback)(const char *value) = nullptr;                                                                              // Lock command handler
    void (*_configUpdateReceivedCallback)(const char *value) = nullptr;                                                                                        // Config update handler
    void (*_keypadCommandReceivedReceivedCallback)(const char *command, const uint &id, const String &name, const String &code, const int &enabled) = nullptr; // Keypad handler
    void (*_timeControlCommandReceivedReceivedCallback)(const char *value) = nullptr;                                                                          // Time control handler
    void (*_authCommandReceivedReceivedCallback)(const char *value) = nullptr;                                                                                 // Auth command handler
};
