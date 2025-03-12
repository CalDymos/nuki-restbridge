#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "Config.h"
#include "NukiWrapper.h"
#include "NukiNetwork.h"
#include <ArduinoJson.h>

/**
 * @brief Minimaler Web-Config-Server, der Konfiguration über / und /save entgegennimmt.
 *        Läuft auf einem eigenen Port (z.B. 8080).
 */
class WebCfgServer
{
public:
    /**
     * @brief Konstruktor für den Web-Konfigurationsserver.
     * @param nuki         Pointer auf die NukiWrapper-Instanz zur Steuerung des Smart Locks.
     * @param network      Pointer auf die NukiNetwork-Instanz für die Netzwerkkommunikation.
     * @param preferences  Pointer auf die Preferences-Instanz zur Speicherung von Einstellungen.
     */
    WebCfgServer(NukiWrapper *nuki, NukiNetwork *network, Preferences *preferences);

    /**
     * @brief Destruktor
     */
    ~WebCfgServer();

    /**
     * @brief Startet den Webserver (setzt Routen und ruft webServer.begin() auf)
     */
    void initialize();

    /**
     * @brief Muss regelmäßig in loop() aufgerufen werden, damit Anfragen verarbeitet werden.
     */
    void handleClient();

private:

    void sendCss(WebServer *server);
    void redirect(WebServer *server, const char* url, int code=301);

    std::vector<String> _ssidList;
    std::vector<int> _rssiList;
    String generateConfirmCode();
    void createSsidList();

    bool isAuthenticated(WebServer *server);
    int doAuthentication(WebServer *server);
    void buildSSIDListHtml(WebServer *server);
    void waitAndProcess(const bool blocking, const uint32_t duration);
    bool processWiFi(WebServer *server, String& message);
    void buildConfirmHtml(WebServer *server, const String &message, uint32_t redirectDelay, bool redirect, String redirectTo = "/");

    // --- Membervariablen ---
    NukiWrapper* _nuki = nullptr;
    Preferences *_preferences = nullptr; // externe Preferences, hier nur referenziert
    WebServer *_webServer = nullptr;      // der eigentliche WebServer
    NukiNetwork* _network = nullptr;
    JsonDocument _httpSessions;

    String _confirmCode = "----";

    String _hostname;
    char _credUser[31] = {0};
    char _credPassword[31] = {0};
};
