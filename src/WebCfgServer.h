#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "NukiPinState.h"
#include "Config.h"
#include "NukiWrapper.h"
#include "NukiNetwork.h"
#include <ArduinoJson.h>

extern TaskHandle_t networkTaskHandle;
extern TaskHandle_t nukiTaskHandle;
extern TaskHandle_t webCfgTaskHandle;

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
    void redirect(WebServer *server, const char *url, int code = 301);

    std::vector<String> _ssidList;
    std::vector<int> _rssiList;
    String generateConfirmCode();
    void createSsidList();
    void logoutSession(WebServer *server);

    bool isAuthenticated(WebServer *server);
    int doAuthentication(WebServer *server);
    void buildAccLvlHtml(WebServer *server);
    void buildNukiConfigHtml(WebServer *server);
    void buildAdvancedConfigHtml(WebServer *server);
    void buildSSIDListHtml(WebServer *server);
    void buildLoginHtml(WebServer *server);
    void buildBypassHtml(WebServer *server);
    void buildConfirmHtml(WebServer *server, const String &message, uint32_t redirectDelay, bool redirect = false, String redirectTo = "/");
    void buildCoredumpHtml(WebServer *server);
    void buildInfoHtml(WebServer *server);
    void buildNetworkConfigHtml(WebServer *server);
    void buildCredHtml(WebServer *server);
    void buildWifiConnectHtml(WebServer *server);
    void buildHtml(WebServer *server);
    void buildHtmlHeader(String &response, const String &additionalHeader = "");
    void buildNavigationMenuEntry(String &response, const char *title, const char *targetPath, const char *warningMessage = "");
    void buildConfigureWifiHtml(WebServer *server);
    void appendParameterRow(String &response, const char *description, const char *value, const char *link = "", const char *id = "");
    void waitAndProcess(const bool blocking, const uint32_t duration);
    bool processWiFi(WebServer *server, String &message);
    bool processBypass(WebServer *server);
    bool processLogin(WebServer *server);
    bool processFactoryReset(WebServer *server);
    bool processUnpair(WebServer *server);

    void appendInputFieldRow(String &response,
                             const char *token,
                             const char *description,
                             const char *value,
                             const size_t &maxLength,
                             const char *args,
                             const bool &isPassword = false,
                             const bool &showLengthRestriction = false);
    void appendInputFieldRow(String &response,
                             const char *token,
                             const char *description,
                             const int value,
                             size_t maxLength,
                             const char *args);
    void appendDropDownRow(String &response,
                           const char *token,
                           const char *description,
                           const String preselectedValue,
                           const std::vector<std::pair<String, String>> &options,
                           const String className);
    void appendTextareaRow(String &response,
                           const char *token,
                           const char *description,
                           const char *value,
                           const size_t &maxLength,
                           const bool &enabled = true,
                           const bool &showLengthRestriction = false);
    void appendCheckBoxRow(String &response,
                           const char *token,
                           const char *description,
                           const bool value,
                           const char *htmlClass);

    const std::vector<std::pair<String, String>> getNetworkDetectionOptions() const;

    const String pinStateToString(const NukiPinState &value) const;

    void saveSessions();
    void loadSessions();
    void clearSessions();

    // --- Membervariablen ---
    NukiWrapper *_nuki = nullptr;
    Preferences *_preferences = nullptr; // externe Preferences, hier nur referenziert
    WebServer *_webServer = nullptr;     // der eigentliche WebServer
    NukiNetwork *_network = nullptr;
    JsonDocument _httpSessions;

    bool _APIConfigured = false;
    bool _HAConfigured = false;
    bool _rebootRequired = false;

    String _confirmCode = "----";

    String _hostname;
    char _credUser[31] = {0};
    char _credPassword[31] = {0};
    bool _allowRestartToPortal = false;
    bool _newBypass = false;
};
