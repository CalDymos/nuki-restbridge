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
 * @brief Minimal Web Configuration Server that accepts configuration via `/` and `/save`.
 *
 * Provides a local HTML interface to configure Wi-Fi, REST API, Home Automation Reporting, and lock pairing.
 * Runs on a separate port (default 80).
 */
class WebCfgServer
{
public:
    /**
     * @brief Constructor for the Web Configuration Server.
     * @param nuki         Pointer to the NukiWrapper instance for controlling the smart lock.
     * @param network      Pointer to the NukiNetwork instance for network communication.
     * @param preferences  Pointer to the Preferences instance for storing settings.
     */
    WebCfgServer(NukiWrapper *nuki, NukiNetwork *network, Preferences *preferences);

    /**
     * @brief Destructor
     */
    ~WebCfgServer();

    /**
     * @brief Starts the web server, sets route handlers and begins listening.
     */
    void initialize();

    /**
     * @brief Must be called regularly to handle incoming HTTP requests.
     */
    void handleClient();

private:
    /**
     * @brief Sends embedded CSS styling to the client.
     */
    void sendCss(WebServer *server);

    /**
     * @brief Issues a redirect to a different URL.
     */
    void redirect(WebServer *server, const char *url, int code = 301);

    /**
     * @brief Generates a random confirmation code for verification.
     * @return 4-digit string code.
     */
    String generateConfirmCode();

    /**
     * @brief Creates a list of scanned SSIDs and their signal strength.
     */
    void createSsidList();

    /**
     * @brief Checks if a user is authenticated.
     * @param server Pointer to the WebServer instance.
     * @return True if session is authenticated.
     */
    bool isAuthenticated(WebServer *server);

    /**
     * @brief Performs login using credentials.
     * @param server Pointer to the WebServer instance.
     * @return HTTP status code.
     */
    int doAuthentication(WebServer *server);

    /**
     * @brief Builds the HTML page for setting access levels (user/admin).
     * @param server Pointer to the active WebServer instance.
     */
    void buildAccLvlHtml(WebServer *server);

    /**
     * @brief Builds the HTML page for configuring Nuki device settings.
     * @param server Pointer to the active WebServer instance.
     */
    void buildNukiConfigHtml(WebServer *server);

    /**
     * @brief Builds the HTML page for advanced system configuration.
     * @param server Pointer to the active WebServer instance.
     */
    void buildAdvancedConfigHtml(WebServer *server);

    /**
     * @brief Builds the HTML list of visible SSIDs for Wi-Fi selection.
     * @param server Pointer to the active WebServer instance.
     */
    void buildSSIDListHtml(WebServer *server);

    /**
     * @brief Builds the login page HTML.
     * @param server Pointer to the active WebServer instance.
     */
    void buildLoginHtml(WebServer *server);

    /**
     * @brief Builds the bypass configuration page (used for enabling/disabling config access).
     * @param server Pointer to the active WebServer instance.
     */
    void buildBypassHtml(WebServer *server);

    /**
     * @brief Builds the confirmation page after applying settings.
     * @param server Pointer to the active WebServer instance.
     * @param message Message to display on the page.
     * @param redirectDelay Delay in milliseconds before redirect (0 = none).
     * @param redirect If true, perform a redirect after delay.
     * @param redirectTo URL to redirect to (default: "/").
     */
    void buildConfirmHtml(WebServer *server, const String &message, uint32_t redirectDelay, bool redirect = false, String redirectTo = "/");

    /**
     * @brief Builds the HTML page showing core dump or crash info.
     * @param server Pointer to the active WebServer instance.
     */
    void buildCoredumpHtml(WebServer *server);

    /**
     * @brief Builds the device info page (version, MAC, uptime, etc.).
     * @param server Pointer to the active WebServer instance.
     */
    void buildInfoHtml(WebServer *server);

    /**
     * @brief Builds the HTML layout for the network configuration screen.
     */
    void buildNetworkConfigHtml(WebServer *server);

    /**
     * @brief Builds the HTML page for editing stored credentials (user/password).
     * @param server Pointer to the active WebServer instance.
     */
    void buildCredHtml(WebServer *server);

    /**
     * @brief Builds the page that allows network mode selection and configuration.
     * @param server Pointer to the active WebServer instance.
     */
    void buildConnectHtml(WebServer *server);

    /**
     * @brief Builds the main HTML layout including navigation.
     */
    void buildHtml(WebServer *server);

    /**
     * @brief Builds the initial HTML header including optional extra headers.
     * @param response Reference to the HTML response string.
     * @param additionalHeader Optional extra HTML to include inside <head> (e.g. CSS/JS).
     */
    void buildHtmlHeader(String &response, const String &additionalHeader = "");

    /**
     * @brief Builds the HTML page to configure Wi-Fi SSID and password.
     * @param server Pointer to the active WebServer instance.
     */
    void buildConfigureWifiHtml(WebServer *server);

    /**
     * @brief Builds the REST API token configuration page.
     * @param server Pointer to the active WebServer instance.
     */
    void buildApiConfigHtml(WebServer *server);

    /**
     * @brief Builds the Home Automation Reporting (HAR) configuration page.
     * @param server Pointer to the active WebServer instance.
     */
    void buildHARConfigHtml(WebServer *server);

    /**
     * @brief Builds a JSON response with current status information (API, HAR, Lock, etc.).
     *
     * This method creates a compressed JSON structure that contains the connection status of API and HAR,
     * the status of the Nuki Lock and devices and their pairing status.
     * It is typically called from a web server endpoint (`/get?page=status`),
     * to cyclically update the status in the web interface.
     *
     * @param server Pointer to the active WebServer instance.
     */
    void buildStatusHtml(WebServer *server);

    /**
     * @brief Handles waiting and asynchronous processing if requested.
     * @param blocking Whether the call should block until finished.
     * @param duration Timeout duration in milliseconds.
     */
    void waitAndProcess(const bool blocking, const uint32_t duration);

    /**
     * Processes the transmitted connection settings from (buildConnectHtml).
     *
     * @param server   Pointer to the WebServer object with the request data.
     * @param message  [out] Message with the status of processing (e.g. error message).
     * @param nwMode   [out] Set network mode (LAN, WLAN, ...)
     * @return         true for success, false for error.
     */
    bool processConnectionSettings(WebServer *server, String &message, int &nwMode);

    /**
     * Processes the transmitted settings of the build... Html pages
     *
     * @param server   Pointer to the WebServer object with the request data.
     * @param message  [out] Message with the status of processing (e.g. error message).
     * @return         true for success, false for error.
     */
    bool processArgs(WebServer *server, String &message);

    /**
     * @brief Processes the current bypass request from the web UI.
     * @param server Pointer to the WebServer instance.
     * @return True if bypass settings were updated successfully.
     */
    bool processBypass(WebServer *server);

    /**
     * @brief Processes user login credentials submitted via HTML form.
     * @param server Pointer to the WebServer instance.
     * @return True if login was successful.
     */
    bool processLogin(WebServer *server);

    /**
     * @brief Performs factory reset and clears stored preferences.
     * @param server Pointer to the WebServer instance.
     * @return True if successful.
     */
    bool processFactoryReset(WebServer *server);

    /**
     * @brief Processes the unpair command sent from the web UI.
     * @param server Pointer to the WebServer instance.
     * @return True if the unpairing was successfully triggered.
     */
    bool processUnpair(WebServer *server);

    /**
     * @brief Appends a static parameter (read-only) row to the HTML output.
     * @param response Reference to the HTML response string.
     * @param description Label for the parameter.
     * @param value Value to display.
     * @param link Optional link associated with the value.
     * @param id Optional HTML element ID.
     */
    void appendParameterRow(String &response,
                            const char *description,
                            const char *value,
                            const char *link = "",
                            const char *id = "");

    /**
     * @brief Appends a navigation menu entry (link) to the HTML.
     * @param response Reference to the HTML response string.
     * @param title Display name of the menu item.
     * @param targetPath URL path to navigate to when clicked.
     * @param warningMessage Optional JavaScript confirmation warning.
     */
    void appendNavigationMenuEntry(String &response,
                                   const char *title,
                                   const char *targetPath,
                                   const char *warningMessage = "");

    /**
     * @brief Appends an input field to the HTML page.
     * @param response Reference to the HTML response string.
     * @param token Field identifier.
     * @param description Label shown next to the input.
     * @param value Pre-filled text value.
     * @param maxLength Max number of characters.
     * @param args Optional arguments (e.g. maxlength, required).
     * @param isPassword True if this is a password input field.
     * @param showLengthRestriction Whether to display remaining length counter.
     */
    void appendInputFieldRow(String &response,
                             const char *token,
                             const char *description,
                             const char *value,
                             const size_t &maxLength,
                             const char *args,
                             const bool &isPassword = false,
                             const bool &showLengthRestriction = false);

    /**
     * @brief Appends an input field for numeric values to the HTML page.
     * @param response Reference to the HTML response string.
     * @param token Field identifier.
     * @param description Label shown next to the input.
     * @param value Numeric value.
     * @param maxLength Maximum number of digits allowed.
     * @param args Optional arguments for the input field.
     */
    void appendInputFieldRow(String &response,
                             const char *token,
                             const char *description,
                             const int value,
                             size_t maxLength,
                             const char *args);

    /**
     * @brief Appends a dropdown field (select box) to the HTML page.
     * @param response Reference to the HTML response string.
     * @param token Unique identifier (name attribute).
     * @param description Label shown next to the dropdown.
     * @param preselectedValue Currently selected option.
     * @param options List of selectable key-value pairs.
     * @param className Optional CSS class name.
     * @param id Optional HTML element ID.
     * @param onChange Optional JavaScript onchange handler.
     */
    void appendDropDownRow(String &response,
                           const char *token,
                           const char *description,
                           const String preselectedValue,
                           const std::vector<std::pair<String, String>> &options,
                           const char *className = "",
                           const char *id = "",
                           const char *onChange = "");

    /**
     * @brief Appends a textarea (multi-line input) field to the HTML page.
     * @param response Reference to the HTML response string.
     * @param token Unique identifier for the field.
     * @param description Label shown next to the textarea.
     * @param value Default content for the textarea.
     * @param maxLength Maximum number of characters allowed.
     * @param enabled Whether the field should be editable.
     * @param showLengthRestriction Display character counter.
     */
    void appendTextareaRow(String &response,
                           const char *token,
                           const char *description,
                           const char *value,
                           const size_t &maxLength,
                           const bool &enabled = true,
                           const bool &showLengthRestriction = false);

    /**
     * @brief Appends a checkbox input field to the HTML response.
     *
     * Adds a labeled checkbox row, optionally with custom CSS class or HTML ID.
     *
     * @param response Reference to the HTML response string.
     * @param token Unique identifier (used as name and id for the checkbox).
     * @param description Label shown next to the checkbox.
     * @param value Initial checked state (true = checked).
     * @param className Optional CSS class name for the checkbox.
     * @param id Optional HTML element ID.
     */
    void appendCheckBoxRow(String &response,
                           const char *token,
                           const char *description,
                           const bool value,
                           const char *className = "",
                           const char *id = "");

    /**
     * Estimate required HTML response buffer size.
     *
     * @param checkboxCount Number of checkbox rows (appendCheckBoxRow).
     * @param inputFieldCount Number of input fields (appendInputFieldRow).
     * @param dropdownCount Number of dropdown elements (appendDropDownRow / <select>).
     * @param dropdownOptionCountTotal Total number of all dropdown options across all dropdowns (<option>).
     * @param textareaCount Number of multiline textarea rows (appendTextareaRow).
     * @param parameterRowCount Number of static info rows (appendParameterRow).
     * @param buttonCount Number of buttons (<input type="button" onclick="...">)
     * @param navigationMenuCount Number of naviagtion menus ()
     * @param extraContentBytes Additional manually added content JS/CSS etc. size (optional).
     * @return Estimated total length of the HTML content in bytes.
     */
    size_t estimateHtmlSize(
        int checkboxCount,
        int inputFieldCount = 0,
        int dropdownCount = 0,
        int dropdownOptionCountTotal = 0,
        int textareaCount = 0,
        int parameterRowCount = 0,
        int buttonCount = 0,
        int navigationMenuCount = 0,
        int extraContentBytes = 0);

    /**
     * Reserve memory for the HTML response string using estimated layout.
     *
     * @param response The String object to reserve space for.
     * @param checkboxCount Number of checkbox rows.
     * @param inputFieldCount Number of input field rows.
     * @param dropdownCount Number of dropdowns.
     * @param dropdownOptionCountTotal Total number of dropdown options.
     * @param textareaCount Number of textarea fields.
     * @param parameterRowCount Number of static parameter/info rows.
     * @param buttonCount Number of buttons
     * @param navigationMenuCount Number of naviagtion menus
     * @param extraContentBytes Additional manually added content JS/CSS etc. size (optional).
     */
    inline void reserveHtmlResponse(String &response,
                                    int checkboxCount,
                                    int inputFieldCount = 0,
                                    int dropdownCount = 0,
                                    int dropdownOptionCountTotal = 0,
                                    int textareaCount = 0,
                                    int parameterRowCount = 0,
                                    int buttonCount = 0,
                                    int navigationMenuCount = 0,
                                    int extraContentBytes = 0)
    {
        response.reserve(estimateHtmlSize(
            checkboxCount,
            inputFieldCount,
            dropdownCount,
            dropdownOptionCountTotal,
            textareaCount,
            parameterRowCount,
            buttonCount,
            navigationMenuCount,
            extraContentBytes));
    }

    /**
     * @brief Returns network mode options for dropdowns.
     * @return Vector of value/label pairs for network mode selection.
     */
    const std::vector<std::pair<String, String>> getNetworkDetectionOptions() const;

    /**
     * @brief Converts a NukiPinState to a human-readable string.
     * @param value NukiPinState enum.
     * @return Corresponding text description.
     */
    const String pinStateToString(const NukiPinState &value) const;

    /**
     * @brief Logs out the current session.
     * @param server Pointer to the WebServer instance.
     */
    void logoutSession(WebServer *server);

    /**
     * @brief Saves the current sessions to memory or preferences.
     */
    void saveSessions();

    /**
     * @brief Loads saved sessions.
     */
    void loadSessions();

    /**
     * @brief Clears all HTTP sessions.
     */
    void clearSessions();

    // --- Member variables ---
    std::vector<String> _ssidList;       // List of visible SSIDs (Wi-Fi networks).
    std::vector<int> _rssiList;          // Corresponding RSSI values for each SSID.
                                         //
    NukiWrapper *_nuki = nullptr;        // Pointer to the NukiWrapper instance for Smart Lock control.
    NukiNetwork *_network = nullptr;     // Pointer to the NukiNetwork instance for connectivity control.
    Preferences *_preferences = nullptr; // Pointer to the Preferences instance for configuration storage.
    WebServer *_webServer = nullptr;     // Pointer to the internal web server instance.
    JsonDocument _httpSessions;          // In-memory representation of active HTTP login sessions.
                                         //
    bool _APIConfigured = false;         // True if REST API configuration was completed.
    bool _HAConfigured = false;          // True if Home Automation Reporting (HAR) was configured.
    bool _rebootRequired = false;        // True if a system reboot is required after saving settings.
                                         //
    String _confirmCode = "----";        // Temporary confirmation code displayed after saving.
                                         //
    String _hostname;                    // Device hostname used in mDNS and web interface.
    char _credUser[31] = {0};            // Stored username for the web interface login.
    char _credPassword[31] = {0};        // Stored password for the web interface login.
                                         //
    bool _allowRestartToPortal = false;  // Allows restarting into access point (config portal) mode.
    bool _newBypass = false;             // True if bypass feature was activated or changed.
};
