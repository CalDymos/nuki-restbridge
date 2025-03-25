#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <ETH.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "ESP32Ping.h"
#include <ArduinoJson.h>

#include "NukiConstants.h"
#include "NukiLockConstants.h"
#include "RestApiPaths.h"
#include "IPConfiguration.h"
#include "NetworkDeviceType.h"
#include "BridgeApiToken.h"
#include "NetworkServiceStates.h"
#include "QueryCommand.h"
#include "LockActionResult.h"

/**
 * @brief Encapsulates all network communication (WiFi/Ethernet, HTTP/REST) in a separate class.
 */
class NukiNetwork
{
public:
    /**
     * @brief Constructor for the network class
     * @param preferences  Pointer to preferences (storage of SSID, PW etc.)
     * @param buffer       A buffer for JSON responses or similar.
     * @param bufferSize   Size of the buffer
     */
    NukiNetwork(Preferences *preferences, char *buffer, size_t bufferSize);

    /**
     * @brief Destructor
     */
    virtual ~NukiNetwork();

    /**
     * @brief Initializes the network (start WiFi, set up Ethernet, etc.).
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
    void sendToHAFloat(const char *path, const char *query, const float value, const uint8_t precision = 2);

    /**
     * @brief Sends an integer value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Integer value to send.
     */
    void sendToHAInt(const char *path, const char *query, const int value);

    /**
     * @brief Sends an unsigned integer value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Unsigned integer value to send.
     */
    void sendToHAUInt(const char *path, const char *query, const unsigned int value);

    /**
     * @brief Sends an unsigned long value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Unsigned long value to send.
     */
    void sendToHAULong(const char *path, const char *query, const unsigned long value);

    /**
     * @brief Sends a 64-bit integer value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value 64-bit integer value to send.
     */
    void sendToHALongLong(const char *path, const char *query, int64_t value);

    /**
     * @brief Sends a boolean value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value Boolean value to send.
     */
    void sendToHABool(const char *path, const char *query, const bool value);

    /**
     * @brief Sends a string value to the Home Automation system.
     * @param path Target REST path.
     * @param query Query string.
     * @param value String value to send.
     */
    void sendToHAString(const char *path, const char *query, const char *value);

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
     * @brief Prüft, ob das Netzwerk (WiFi/Ethernet) verbunden ist.
     */
    bool isConnected() const;

    /**
     * @brief Prüft, ob (zumindest) das WiFi-Modul verbunden ist.
     *        (Bei Ethernet kann man anders entscheiden.)
     */
    bool isWifiConnected();

    bool isWifiConfigured() const;

    /**
     * @brief Liefert die lokale IP-Adresse.
     */
    String localIP() const;

    /**
     * @brief Returns the network BSSID string (only available for WiFi).
     */
    String networkBSSID() const;

    /**
     * @brief Signalstärke abfragen (RSSI), nur relevant bei WLAN.
     */
    int8_t signalStrength();

    /**
     * @brief Löst das Abschalten eines evtl. vorhandenen WiFi-Fallbacks aus.
     */
    void clearWifiFallback();

    const NetworkDeviceType networkDeviceType();

    /**
     * @brief Deaktiviert automatische Neustarts beim Trennen usw.
     */
    void disableAutoRestarts();

    /**
     * @brief Disables the REST API interface.
     */
    void disableAPI();

    /**
     * @brief Deaktiviert Home Automation .
     */
    void disableHAR();

    /**
     * @brief Getter für Netzwerk-Dienstestatus (z.B. Webserver, HTTPClient).
     */
    NetworkServiceStates networkServicesState();

    uint8_t queryCommands();

    /**
     * @brief Sendet beliebige Requests an die Home-Automation (Um Statuswerte zu übermitteln).
     */
    void sendRequestToHA(const char *path, const char *query, const char *value);

    /**
     * @brief Sende HTTP-Response an Client (z.B. auf Request der Home-Automation).
     */
    void sendResponse(const char *jsonResultStr);
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
     * @brief Device setup and initialization
     */
    void setupDevice();

    /**
     * @brief Initialisiert WiFi (und ruft ggf. connect() auf).
     */
    void initializeWiFi();

    /**
     * @brief Initialisiert Ethernet (DHCP oder statische IP).
     */
    void initializeEthernet();

    /**
     * @brief Startet ggf. WebServer und HTTPClient.
     */
    void startNetworkServices();

    static void onRestDataReceivedCallback(const char *path, WebServer &server);

    void onRestDataReceived(const char *path, WebServer &server);

    /**
     * @brief Führt Testläufe für WebServer und HTTPClient aus (z.B. Ping).
     */
    NetworkServiceStates testNetworkServices();

    /**
     * @brief Stoppt/Restartet WebServer / HTTPClient im Fehlerfall.
     */
    void restartNetworkServices(NetworkServiceStates status);

    /**
     * @brief Ereignisbehandlung bei Netzwerk-Events (WiFi/Ethernet Callback).
     */
    void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);

    void onConnected();

    /**
     * @brief Verbindet sich mit gespeicherter SSID/Pass oder öffnet AP.
     */
    bool connect();

    /**
     * @brief Öffnet einen Access Point, falls keine SSID hinterlegt ist.
     */
    void openAP();

    /**
     * @brief Falls WiFi/ETH-Verbindung abbricht, handle disconnect.
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

    // Configuration & network parameters
    Preferences *_preferences;
    IPConfiguration *_ipConfiguration = nullptr;
    String _hostname;
    String _WiFissid;
    String _WiFipass;

    BridgeApiToken *_apitoken = nullptr;
    // Network & system states
    bool _firstBootAfterDeviceChange = false;
    bool _webCfgEnabled = true;
    bool _apiEnabled = false;
    bool _openAP = false;
    bool _APisReady = false;
    bool _startAP = true;
    bool _connected = false;
    bool _lockEnabled = false;
    bool _hardwareInitialized = false;
    bool _publishDebugInfo = false;
    bool _restartOnDisconnect = false;
    bool _disableNetworkIfNotConnected = false;
    bool _firstTunerStatePublish = true;
    NetworkServiceStates _networkServicesState = NetworkServiceStates::UNDEFINED;

    String _keypadCommandName = "";
    String _keypadCommandCode = "";
    uint _keypadCommandId = 0;
    int _keypadCommandEnabled = 1;
    uint8_t _queryCommands = 0;

    // Timestamps for network monitoring
    int64_t _checkIpTs = -1;
    int64_t _lastConnectedTs = 0;
    int64_t _lastMaintenanceTs = 0;
    int64_t _lastNetworkServiceTs = 0;
    int64_t _publishedUpTime = 0;
    int64_t _lastRssiTs = 0;

    // Network services & connection parameters
    WebServer *_server = nullptr; // for REST-API Requests
    HTTPClient *_httpClient = nullptr;
    int _foundNetworks = 0;
    int _networkTimeout = 0;
    int _rssiSendInterval = 0;
    int _MaintenanceSendIntervall = 0;
    int _networkServicesConnectCounter = 0;

    // Signal strength & network type
    NetworkDeviceType _networkDeviceType = NetworkDeviceType::UNDEFINED;
    int8_t _lastRssi = 127;

    // Home automation parameters
    bool _homeAutomationEnabled = false;
    String _homeAutomationAdress;
    String _homeAutomationUser;
    String _homeAutomationPassword;
    int _homeAutomationPort;

    char *_buffer;
    const size_t _bufferSize;
    char _apiBridgePath[129] = {0};
    char _apiLockPath[129] = {0};
    int _apiPort;

    LockActionResult (*_lockActionReceivedCallback)(const char *value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char *value) = nullptr;
    void (*_keypadCommandReceivedReceivedCallback)(const char *command, const uint &id, const String &name, const String &code, const int &enabled) = nullptr;
    void (*_timeControlCommandReceivedReceivedCallback)(const char *value) = nullptr;
    void (*_authCommandReceivedReceivedCallback)(const char *value) = nullptr;
};
