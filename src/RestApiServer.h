#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "BridgeApiToken.h"
#include "NetworkServiceState.h"
#include "QueryCommand.h"
#include "LockActionResult.h"
#include "RestApiPaths.h"
#include "Config.h"

/**
 * @brief Global flag set by main.cpp to force-enable the web config server
 *        regardless of the NVS preference value.
 *
 * Declared extern here because RestApiServer::onRequestReceived() reads it
 * when processing the /bridge/enableWebServer endpoint.
 * Defined in main.cpp.
 */
extern bool forceEnableWebCfgServer;

/**
 * @brief Manages the REST API WebServer on port 8080.
 *
 * RestApiServer encapsulates all REST API handling that was previously
 * embedded in NukiNetwork. It owns the WebServer instance, handles token
 * authentication, routes all incoming requests, and dispatches them to
 * NukiWrapper via registered callbacks.
 *
 * Lifecycle:
 *   1. Construct — pass Preferences and two cross-cutting callbacks.
 *   2. initialize() — read API settings and create the BridgeApiToken.
 *   3. start()      — create and start the WebServer.
 *   4. handleClient() — call every network-task loop to process requests.
 *   5. test()       — verify the server is running.
 *   6. restart()    — stop and recreate the server on failure.
 *   7. stop() / disable() — graceful teardown.
 *
 * NukiNetwork keeps all public REST-facing methods (sendResponse,
 * queryCommands, set*Callback, assignNewApiToken, getApiToken, disableAPI)
 * as thin delegates to this class, so NukiWrapper and WebCfgServer require
 * no changes.
 *
 * Thread safety: all methods must be called from the same task (networkTask).
 */
class RestApiServer
{
public:
    /**
     * @brief Construct a RestApiServer.
     *
     * @param preferences          Pointer to the ESP32 NVS preferences instance.
     *                             Must remain valid for the lifetime of this object.
     * @param disableHarFn         Callback invoked during shutdown to disable
     *                             the HarClient (avoids a circular dependency on
     *                             NukiNetwork).
     * @param clearWifiFallbackFn  Callback invoked when enabling/disabling the web
     *                             config server via REST to clear the WiFi fallback
     *                             flag before restart.
     */
    RestApiServer(Preferences* preferences,
                  std::function<void()> disableHarFn,
                  std::function<void()> clearWifiFallbackFn);

    /**
     * @brief Destructor. Stops and deletes the WebServer and BridgeApiToken.
     */
    ~RestApiServer();

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /**
     * @brief Read API settings from NVS and create the BridgeApiToken.
     *
     * Reads: api_enabled, api_port, lock_enabled.
     * Must be called once before start().
     */
    void initialize();

    /**
     * @brief Create and start the WebServer.
     *
     * No-op if the API is disabled or the local IP is "0.0.0.0".
     * Registers an onNotFound handler that routes all requests through
     * onRequestReceived().
     *
     * Pre-existing bug fixed here: handleClient() was never called in the
     * original NukiNetwork::update(), so the server never processed requests.
     * RestApiServer::handleClient() must now be called from NukiNetwork::update().
     *
     * @param localIP  Current local IP address string (e.g. "192.168.1.10").
     *                 Used to log the server URL and guard against 0.0.0.0.
     */
    void start(const String& localIP);

    /**
     * @brief Poll the WebServer for incoming connections.
     *
     * Must be called every loop iteration from networkTask via
     * NukiNetwork::update(). Without this call, no REST requests are
     * ever processed.
     *
     * Fixes the pre-existing bug where handleClient() was missing.
     */
    void handleClient();

    /**
     * @brief Test whether the WebServer is running and the local IP is valid.
     *
     * Performs two checks (matching the original NukiNetwork::testNetworkServices()):
     *   1. _server must not be null.
     *   2. localIP must be a valid, parseable IP address (not "0.0.0.0").
     *
     * Note: a direct HTTP loopback connection test is intentionally omitted —
     * it reliably fails on ESP32 because LWIP does not route loopback traffic
     * unless LWIP_NETIF_LOOPBACK is enabled (same reasoning as original code).
     *
     * @param localIP  Current local IP address string from NukiNetwork::localIP().
     * @return true  if the server is running and the IP is valid.
     * @return false if the server is null or the IP is unusable.
     */
    bool test(const String& localIP) const;

    /**
     * @brief Stop and recreate the WebServer.
     *
     * Called by NukiNetwork::restartNetworkServices() when test() returns
     * false. Performs a clean stop/delete before re-allocating.
     *
     * @param localIP  Current local IP address string.
     */
    void restart(const String& localIP);

    /**
     * @brief Stop and delete the WebServer without disabling the API.
     *
     * Used by NukiNetwork destructor and restartNetworkServices cleanup.
     */
    void stop();

    /**
     * @brief Disable the REST API.
     *
     * Sets _enabled = false. Further requests will receive 503 or be rejected.
     * Stops the WebServer if running.
     */
    void disable();

    // -----------------------------------------------------------------------
    // Status
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if the REST API is enabled (preference + not disabled).
     */
    bool isEnabled() const;

    /**
     * @brief Returns the health flag last set by NukiNetwork after test().
     *
     * NukiNetwork calls setOk(false) when testNetworkServices() detects a
     * REST API server failure, and setOk(true) after a successful restart().
     */
    bool isOk() const;

    /**
     * @brief Set the health flag from NukiNetwork.
     *
     * @param ok  true = server is running; false = server is in error state.
     */
    void setOk(bool ok);

    // -----------------------------------------------------------------------
    // REST API response helpers (called by NukiWrapper via NukiNetwork)
    // -----------------------------------------------------------------------

    /**
     * @brief Send a pre-serialized JSON string response to the client.
     *
     * @param jsonResultStr  Null-terminated JSON string.
     */
    void sendResponse(const char* jsonResultStr);

    /**
     * @brief Serialize a JsonDocument and send it as an HTTP response.
     *
     * Adds "code" and "message" fields to the document before serializing.
     * Uses CharBufferGuard for the serialization buffer.
     *
     * @param jsonResult  JsonDocument to serialize (modified in place).
     * @param message     Optional message string (default: "").
     * @param httpCode    HTTP status code to return (default: 200).
     */
    void sendResponse(JsonDocument& jsonResult,
                      const char* message = "",
                      int httpCode = 200);

    /**
     * @brief Return the current bitmask of pending query commands and clear it.
     *
     * Each bit corresponds to a QUERY_COMMAND_* flag set when a /query/*
     * REST endpoint was called. Called by NukiWrapper::update() every loop.
     *
     * @return uint8_t  Bitmask of pending QUERY_COMMAND_* flags.
     */
    uint8_t queryCommands();

    /**
     * @brief Consumes a pending lock reboot request triggered via POST /lock/reboot.
     * @return true once if a reboot was requested, false otherwise.
     */
    bool consumeRebootLockRequest();

    // -----------------------------------------------------------------------
    // Token management (called by WebCfgServer via NukiNetwork)
    // -----------------------------------------------------------------------

    /**
     * @brief Generate and store a new random API token.
     */
    void assignNewApiToken();

    /**
     * @brief Store a user-supplied API token persistently.
     *
     * @param token Null-terminated C-string containing the token to store.
     */
    void assignApiToken(const char* token);

    /**
     * @brief Return the current API token as a null-terminated C-string.
     *
     * @return Pointer to the internal token buffer. Valid until next token change.
     */
    char* getApiToken();

    // -----------------------------------------------------------------------
    // Callback registration (called by NukiWrapper via NukiNetwork)
    // -----------------------------------------------------------------------

    /**
     * @brief Register the callback invoked when a lock action is received via REST.
     *
     * @param cb  Function pointer: receives the action string, returns LockActionResult.
     */
    void setLockActionReceivedCallback(
        LockActionResult (*cb)(const char* value));

    /**
     * @brief Register the callback invoked when a config update is received via REST.
     *
     * @param cb  Function pointer: receives the JSON config string.
     */
    void setConfigUpdateReceivedCallback(
        void (*cb)(const char* value));

    /**
     * @brief Register the callback invoked when a keypad command is received via REST.
     *
     * @param cb  Function pointer: receives command name, id, name, code, enabled.
     */
    void setKeypadCommandReceivedCallback(
        void (*cb)(const char* command, const uint& id, const String& name,
                   const String& code, const int& enabled));

    /**
     * @brief Register the callback invoked when a time-control command is received.
     *
     * @param cb  Function pointer: receives the JSON command string.
     */
    void setTimeControlCommandReceivedCallback(
        void (*cb)(const char* value));

    /**
     * @brief Register the callback invoked when an authorization command is received.
     *
     * @param cb  Function pointer: receives the JSON command string.
     */
    void setAuthCommandReceivedCallback(
        void (*cb)(const char* value));

private:
    // -----------------------------------------------------------------------
    // Internal request handling
    // -----------------------------------------------------------------------

    /**
     * @brief Main request handler: authenticates and routes all incoming requests.
     *
     * Called from the onNotFound lambda registered in start().
     * Checks token, guards against error state, routes to onShutdownReceived()
     * or the main dispatch logic.
     *
     * @param path    Full URI path of the request.
     * @param server  Reference to the WebServer processing the request.
     */
    void onRequestReceived(const char* path, WebServer& server);

    /**
     * @brief Handle the /bridge/shutdown endpoint.
     *
     * Disables the REST API and HAR client, closes preferences, and
     * enters deep sleep via safeShutdownESP().
     *
     * @param path    Full URI path of the request.
     * @param server  Reference to the WebServer processing the request.
     */
    void onShutdownReceived(const char* path, WebServer& server);

    /**
     * @brief Verify the request is authorized to use the REST API.
     *
     * Two checks are applied in order:
     *   1. IP allowlist — if preference_api_allowed_ip is set (e.g. the Loxone
     *      Miniserver IP), requests from any other host are rejected immediately.
     *      Leave empty to allow all hosts (less secure, but simpler setup).
     *   2. Token check — the "token" query parameter is compared against the
     *      stored API token using a constant-time comparison to prevent
     *      timing-based token reconstruction attacks.
     *
     * Note: The Loxone Miniserver Gen 1 sends tokens as query parameters and
     * does not support custom HTTP headers or HTTPS. The query-parameter
     * approach is therefore required for compatibility.
     *
     * @param server  The current WebServer request context.
     * @return true if both checks pass.
     */
    bool isAuthenticated(WebServer& server) const;

    /**
     * @brief Extract and return request arguments as a C-string.
     *
     * Counts only data args (excluding any legacy "token" query param).
     * Behaviour:
     *   - 1 data arg named "val": returns value of "val".
     *   - 1 data arg (other name): returns the arg name.
     *   - >1 data args: serializes all non-token args as JSON into _argsBuffer.
     *   - 0 data args: returns empty string.
     *
     * Writes into the fixed-size _argsBuffer member (REST_ARGS_BUFFER_SIZE bytes).
     *
     * @param server  Reference to the WebServer processing the request.
     * @return char*  Pointer to _argsBuffer. Valid until next call.
     */
    char* getArgs(WebServer& server);

    /**
     * @brief Check whether a full URI path matches a prefixed subpath.
     *
     * Concatenates mainPath + subPath and compares with fullPath.
     *
     * @param fullPath  Full incoming URI (e.g. "/lock/action").
     * @param mainPath  Path prefix (e.g. "/lock").
     * @param subPath   Sub-path suffix (e.g. "/action").
     * @return true if fullPath == mainPath + subPath.
     */
    bool comparePrefixedPath(const char* fullPath,
                             const char* mainPath,
                             const char* subPath);

    /**
     * @brief Concatenate mainPath and subPath into outPath.
     *
     * @param mainPath  Source prefix string.
     * @param subPath   Source suffix string.
     * @param outPath   Output buffer (must be at least 385 bytes).
     */
    void buildApiPath(const char* mainPath,
                      const char* subPath,
                      char* outPath);

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    /** NVS preferences handle. Not owned — must outlive this object. */
    Preferences* _preferences;

    /** Called in onShutdownReceived() to disable the HarClient. */
    std::function<void()> _disableHarFn;

    /** Called in onRequestReceived() before restart on web-server toggle. */
    std::function<void()> _clearWifiFallbackFn;

    /** API token handler. Owned by this class. */
    BridgeApiToken* _apitoken = nullptr;

    /** If non-empty, only this IP address may call REST endpoints (e.g. Loxone Miniserver). */
    String _allowedIp = "";

    /** WebServer instance for the REST API. Owned by this class. */
    WebServer* _server = nullptr;

    /** True if the REST API is enabled in preferences and disable() not called. */
    bool _enabled = false;

    /**
     * @brief Health flag managed by NukiNetwork.
     *
     * Set to false when testNetworkServices() detects a server failure.
     * Set back to true after a successful restart().
     * When false, onRequestReceived() rejects all incoming requests.
     */
    bool _isOk = true;

    /** True if lock control via REST API is enabled (preference_lock_enabled). */
    bool _lockEnabled = false;

    /** REST API listen port (default: REST_SERVER_PORT = 8080). */
    int _apiPort = REST_SERVER_PORT;

    /** Bitmask of pending QUERY_COMMAND_* flags set by /query/* endpoints. */
    uint8_t _queryCommands = 0;
    bool    _rebootLockPending  = false;  ///< Set by POST /lock/reboot, consumed by networkTask

    /**
     * @brief Fixed-size buffer for request argument serialization.
     *
     * Replaces the original heap-allocated _restArgsBuffer in NukiNetwork,
     * avoiding an unnecessary malloc since the size is known at compile time.
     * Size: REST_ARGS_BUFFER_SIZE (768 bytes).
     */
    char _argsBuffer[REST_ARGS_BUFFER_SIZE];

    // Temporary storage for multi-field keypad commands
    /** Keypad command name parsed from the incoming JSON request. */
    String _keypadCommandName = "";

    /** Keypad command encrypted/plain code parsed from the incoming JSON request. */
    String _keypadCommandEncCode = "";

    /** Keypad command entry ID parsed from the incoming JSON request. */
    uint _keypadCommandId = 0;

    /** Keypad command enabled flag parsed from the incoming JSON request. */
    int _keypadCommandEnabled = 1;

    // Callback function pointers registered by NukiWrapper
    /** Callback for /lock/action requests. */
    LockActionResult (*_lockActionReceivedCallback)(const char* value) = nullptr;

    /** Callback for /lock/config/action requests. */
    void (*_configUpdateReceivedCallback)(const char* value) = nullptr;

    /** Callback for /lock/keypad/command requests. */
    void (*_keypadCommandReceivedReceivedCallback)(
        const char* command, const uint& id, const String& name,
        const String& code, const int& enabled) = nullptr;

    /** Callback for /lock/timecontrol/action requests. */
    void (*_timeControlCommandReceivedReceivedCallback)(const char* value) = nullptr;

    /** Callback for /lock/authorization/action requests. */
    void (*_authCommandReceivedReceivedCallback)(const char* value) = nullptr;
};