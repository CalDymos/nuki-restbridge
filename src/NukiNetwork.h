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

#include "RestApiPaths.h"
#include "IPConfiguration.h"
#include "NetworkDeviceType.h"
#include "BridgeApiToken.h"
#include "NetworkServiceStates.h"
#include "QueryCommand.h"
#include "LockActionResult.h"

/**
 * @brief Kapselt sämtliche Netzwerkkommunikation (WiFi/Ethernet, HTTP/REST) in einer eigenen Klasse.
 */
class NukiNetwork
{
public:
    /**
     * @brief Konstruktor für die Netzwerkklasse
     * @param preferences  Pointer auf Preferences (Speicherung von SSID, PW etc.)
     * @param buffer       Ein Buffer für JSON-Responses o.ä.
     * @param bufferSize   Größe des Buffers
     */
    NukiNetwork(Preferences *preferences, char *buffer, size_t bufferSize);

    /**
     * @brief Destruktor – hier ggf. Aufräumarbeiten (z.B. WebServer stoppen).
     */
    virtual ~NukiNetwork();

    /**
     * @brief Initialisiert das Netzwerk (z.B. WiFi starten, Ethernet einrichten usw.).
     */
    void initialize();

    /**
     * @brief Regelt die periodischen Abläufe, z.B. Watchdog-Checks,
     *        Neuverbinden, IP-Konfiguration, WebServer-Annahme, usw.
     * @return true, wenn Netzwerk aktiv ist und Verbindung besteht, sonst false.
     */
    bool update();

    /**
     * @brief Führt eine erneute Konfiguration / Reconnect durch (z.B. bei neuer SSID).
     */
    void reconfigure();

    /**
     * @brief Startet einen WiFi-Scan (synchron oder asynchron).
     */
    void scan(bool passive = false, bool async = true);

    // Data send methods to home automation
    void sendToHAFloat(const char *path, const char *query, const float value, const uint8_t precision = 2);
    /**
     * @brief Sendet Daten als Int an eine Home-Automation.
     * @param path  REST-Pfad
     * @param query Zusätzliche Query
     * @param value Der zu sendende Wert
     */
    void sendToHAInt(const char *path, const char *query, const int value);
    void sendToHAUInt(const char *path, const char *query, const unsigned int value);
    void sendToHAULong(const char *path, const char *query, const unsigned long value);
    void sendToHALongLong(const char *path, const char *query, int64_t value);
    void sendToHABool(const char *path, const char *query, const bool value);
    void sendToHAString(const char *path, const char *query, const char *value);

    void sendToHALockBleAddress(const std::string& address);

    /**
     * @brief Prüft, ob die (evtl. im AP-Modus geöffnete) Access-Point-Schnittstelle offen ist.
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
     * @brief Liefert eine Kennung für Ethernet oder WiFi (z.B. BSSID).
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

private:
    /**
     * @brief Device setup and initialization
     */
    void setupDevice();

    /**
     * @brief Liest gespeicherte WiFi-Konfiguration / IP-Konfiguration etc.
     */
    void readSettings();

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

    bool comparePrefixedPath(const char *fullPath, const char *subPath);
    void buildApiPath(const char *path, char *outPath);
    char *getArgs(WebServer &server);

    // Singleton instance
    static NukiNetwork *_inst;

    // Configuration & network parameters
    Preferences *_preferences;
    BridgeApiToken *_apitoken = nullptr;
    IPConfiguration *_ipConfiguration = nullptr;
    String _hostname;
    String _WiFissid;
    String _WiFipass;

    // Network & system states
    bool _firstBootAfterDeviceChange = false;
    bool _webEnabled = true;
    bool _apiEnabled = false;
    bool _openAP = false;
    bool _startAP = true;
    bool _connected = false;
    bool _lockEnabled = false;
    bool _hardwareInitialized = false;
    bool _publishDebugInfo = false;
    bool _initialized = false;
    bool _restartOnDisconnect = false;
    bool _disableNetworkIfNotConnected = false;
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
    int _rssiPublishInterval = 0;
    int _MaintenancePublishIntervall = 0;
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
    char _apiLockPath[129] = { 0 };
    int _apiPort;

    LockActionResult (*_lockActionReceivedCallback)(const char* value) = nullptr;
    void (*_configUpdateReceivedCallback)(const char* value) = nullptr;
    void (*_keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled) = nullptr;
    void (*_timeControlCommandReceivedReceivedCallback)(const char* value) = nullptr;
    void (*_authCommandReceivedReceivedCallback)(const char* value) = nullptr;
};
