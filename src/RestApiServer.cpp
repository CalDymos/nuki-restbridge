#include "RestApiServer.h"
#include "CharBuffer.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "util/TaskUtils.h"
#include "util/AuthUtils.h"

// -----------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------

RestApiServer::RestApiServer(Preferences* preferences,
                             std::function<void()> disableHarFn,
                             std::function<void()> clearWifiFallbackFn)
    : _preferences(preferences),
      _disableHarFn(disableHarFn),
      _clearWifiFallbackFn(clearWifiFallbackFn)
{
    memset(_argsBuffer, 0, sizeof(_argsBuffer));
}

RestApiServer::~RestApiServer()
{
    stop();

    if (_apitoken)
    {
        delete _apitoken;
        _apitoken = nullptr;
    }
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

void RestApiServer::initialize()
{
    _enabled     = _preferences->getBool(preference_api_enabled, false);
    _apiPort     = _preferences->getInt(preference_api_port, REST_SERVER_PORT);
    _lockEnabled = _preferences->getBool(preference_lock_enabled, false);

    _apitoken = new BridgeApiToken(_preferences, preference_api_token);
    _allowedIp = _preferences->getString(preference_api_allowed_ip, "");
}

void RestApiServer::start(const String& localIP)
{
    if (!_enabled || localIP == "0.0.0.0")
        return;

    Log->println(F("[INFO] Starting REST API Server"));

    _server = new WebServer(_apiPort);
    if (_server)
    {
        // Use a capturing lambda so no static _inst pointer is needed.
        // The lambda captures 'this' and passes the request through to
        // onRequestReceived(), which is a regular (non-static) member method.
        _server->onNotFound([this]()
        {
            onRequestReceived(_server->uri().c_str(), *_server);
        });
        _server->begin();
        Log->println("[INFO] REST API Server started on http://" +
                     localIP + ":" + String(_apiPort));
    }
    else
    {
        Log->println(F("[ERROR] REST API Server could not be created"));
    }
}

void RestApiServer::handleClient()
{
    if (_server != nullptr)
    {
        _server->handleClient();
    }
}

bool RestApiServer::test(const String& localIP) const
{
    if (!_enabled)
        return true; // not enabled = not a failure

    // Check 1: server instance must exist
    if (_server == nullptr)
    {
        Log->println(F("[DEBUG] REST API: _server is NULL!"));
        return false;
    }

    // Check 2: local IP must be valid and parseable
    IPAddress ip;
    if (localIP == "0.0.0.0" || !ip.fromString(localIP.c_str()))
    {
        Log->printf(F("[ERROR] Invalid IP address for REST WebServer: %s\r\n"),
                    localIP.c_str());
        return false;
    }

    // Note: a direct HTTP loopback connection test is intentionally omitted.
    // On ESP32, LWIP does not route loopback traffic back to the same device
    // unless LWIP_NETIF_LOOPBACK is enabled — causing the connect() call to
    // always fail with a false negative. This matches the commented-out block
    // in the original NukiNetwork::testNetworkServices().
    //
    // WiFiClient client;
    // if (!client.connect(ip, _apiPort))
    // {
    //     Log->printf(F("[ERROR] REST WebServer is not responding (%s:%d)!\r\n"),
    //                 localIP.c_str(), _apiPort);
    //     return false;
    // }
    // client.stop();

    return true;
}

void RestApiServer::restart(const String& localIP)
{
    Log->println(F("[INFO] Restarting the REST API Server..."));

    // Phase 1: clean up existing server (only if allocated)
    if (_server)
    {
        _server->stop();
        delete _server;
        _server = nullptr;
        Log->println(F("[INFO] REST API: Old WebServer stopped and deleted"));
    }
    else
    {
        Log->println(F("[INFO] REST API: No existing server, creating new one"));
    }

    // Phase 2: always recreate (matches original restartNetworkServices behaviour)
    _server = new WebServer(_apiPort);
    if (_server)
    {
        _server->onNotFound([this]()
        {
            onRequestReceived(_server->uri().c_str(), *_server);
        });
        _server->begin();
        _isOk = true;
        Log->println("[INFO] REST API Server started on http://" +
                     localIP + ":" + String(_apiPort));
    }
    else
    {
        Log->println(F("[ERROR] REST API Server cannot be reinitialized"));
    }
}

void RestApiServer::stop()
{
    if (_server)
    {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
}

void RestApiServer::disable()
{
    _enabled = false;
    stop();
}

// -----------------------------------------------------------------------
// Status
// -----------------------------------------------------------------------

bool RestApiServer::isEnabled() const
{
    return _enabled;
}

bool RestApiServer::isOk() const
{
    return _isOk;
}

void RestApiServer::setOk(bool ok)
{
    _isOk = ok;
}

// -----------------------------------------------------------------------
// Response helpers
// -----------------------------------------------------------------------

void RestApiServer::sendResponse(const char* jsonResultStr)
{
    _server->send(200, F("application/json"), jsonResultStr);
}

void RestApiServer::sendResponse(JsonDocument& jsonResult,
                                 const char* message,
                                 int httpCode)
{
    jsonResult[F("code")]    = httpCode;
    jsonResult[F("message")] = message;

    CharBufferGuard buf(CHAR_BUFFER_HTTP_TIMEOUT);
    if (!buf)
    {
        _server->send(503, F("application/json"),
                      F("{\"code\":503,\"message\":\"buffer busy\"}"));
        return;
    }
    serializeJson(jsonResult, buf.get(), buf.size());
    _server->send(httpCode, F("application/json"), buf.get());
}

// -----------------------------------------------------------------------
// queryCommands
// -----------------------------------------------------------------------

uint8_t RestApiServer::queryCommands()
{
    uint8_t qc = _queryCommands;
    _queryCommands = 0;
    return qc;
}

// -----------------------------------------------------------------------
// Token management
// -----------------------------------------------------------------------

void RestApiServer::assignNewApiToken()
{
    _apitoken->assignNewToken();
}

char* RestApiServer::getApiToken()
{
    return _apitoken->get();
}

// -----------------------------------------------------------------------
// Callback registration
// -----------------------------------------------------------------------

void RestApiServer::setLockActionReceivedCallback(
    LockActionResult (*cb)(const char* value))
{
    _lockActionReceivedCallback = cb;
}

void RestApiServer::setConfigUpdateReceivedCallback(
    void (*cb)(const char* value))
{
    _configUpdateReceivedCallback = cb;
}

void RestApiServer::setKeypadCommandReceivedCallback(
    void (*cb)(const char* command, const uint& id, const String& name,
               const String& code, const int& enabled))
{
    _keypadCommandReceivedReceivedCallback = cb;
}

void RestApiServer::setTimeControlCommandReceivedCallback(
    void (*cb)(const char* value))
{
    _timeControlCommandReceivedReceivedCallback = cb;
}

void RestApiServer::setAuthCommandReceivedCallback(
    void (*cb)(const char* value))
{
    _authCommandReceivedReceivedCallback = cb;
}

// -----------------------------------------------------------------------
// Internal request handling
// -----------------------------------------------------------------------

void RestApiServer::onRequestReceived(const char* path, WebServer& server)
{
    // Guard: API must be enabled
    if (!_enabled)
        return;

    // Guard: server must not be in error state
    if (!_isOk)
        return;

    // Token authentication — required for ALL endpoints including shutdown
    if (!isAuthenticated(server))
    {
        server.send(401, F("text/html"), "");
        return;
    }

    // Route to the main request handler
    JsonDocument jsonResult;
    char* data = getArgs(server);

    // --- Bridge-level endpoints ---

    if (comparePrefixedPath(path, api_path_bridge, api_path_shutdown))
    {
        onShutdownReceived(path, server);
        return;
    }

    if (comparePrefixedPath(path, api_path_bridge, api_path_disable_api))
    {
        Log->println(F("[INFO] (REST API) Disable REST API"));
        _enabled = false;
        _preferences->putBool(preference_api_enabled, _enabled);
        sendResponse(jsonResult);
        return;
    }

    if (comparePrefixedPath(path, api_path_bridge, api_path_reboot))
    {
        Log->println(F("[INFO] (REST API) Reboot requested"));
        TaskWdtResetAndDelay(200);
        sendResponse(jsonResult);
        Log->disableFileLog();
        TaskWdtResetAndDelay(500);
        restartEsp(RestartReason::RequestedViaApi);
        return;
    }

    if (comparePrefixedPath(path, api_path_bridge, api_path_enable_web_server))
    {
        if (!data || !*data)
        {
            sendResponse(jsonResult, "missing data", 400);
            return;
        }

        if (atoi(data) == 0)
        {
            if (!_preferences->getBool(preference_webcfgserver_enabled, true)
                && !forceEnableWebCfgServer)
            {
                return;
            }
            Log->println(F("[INFO] (REST API) Disable Config Web Server, restarting"));
            _preferences->putBool(preference_webcfgserver_enabled, false);
        }
        else
        {
            if (_preferences->getBool(preference_webcfgserver_enabled, true)
                || forceEnableWebCfgServer)
            {
                return;
            }
            Log->println(F("[INFO] (REST API) Enable Config Web Server, restarting"));
            _preferences->putBool(preference_webcfgserver_enabled, true);
        }

        sendResponse(jsonResult);
        _clearWifiFallbackFn();
        Log->disableFileLog();
        TaskWdtResetAndDelay(200);
        restartEsp(RestartReason::ReconfigureWebCfgServer);
        return;
    }

    // --- Lock-level endpoints (only if lock is enabled) ---

    if (!_lockEnabled)
        return;

    if (comparePrefixedPath(path, api_path_lock, api_path_action))
    {
        if (!data || !*data)
        {
            sendResponse(jsonResult, "missing data", 400);
            return;
        }

        Log->println(F("[INFO] (REST API) Lock action received: "));
        Log->printf(F("[INFO] %s\n"), data);

        LockActionResult lockActionResult = LockActionResult::Failed;
        if (_lockActionReceivedCallback != nullptr)
        {
            lockActionResult = _lockActionReceivedCallback(data);
        }

        switch (lockActionResult)
        {
        case LockActionResult::Success:
            sendResponse(jsonResult);
            break;
        case LockActionResult::UnknownAction:
            sendResponse(jsonResult, "unknown_action", 404);
            break;
        case LockActionResult::AccessDenied:
            sendResponse(jsonResult, "denied", 403);
            break;
        case LockActionResult::Failed:
            sendResponse(jsonResult, "error", 500);
            break;
        }
        return;
    }

    if (comparePrefixedPath(path, api_path_lock, api_path_keypad_command))
    {
        if (_keypadCommandReceivedReceivedCallback != nullptr)
        {
            if (!data || !*data)
            {
                sendResponse(jsonResult, "missing data", 400);
                return;
            }

            JsonDocument json;
            DeserializationError jsonError = deserializeJson(json, data);

            if (jsonError)
            {
                sendResponse(jsonResult, "invalid data", 400);
                return;
            }

            const char* command = json.containsKey("command")
                ? json["command"].as<const char*>()
                : nullptr;
            _keypadCommandId      = json.containsKey("id")
                ? json["id"].as<unsigned int>() : 0;
            _keypadCommandName    = json.containsKey("name")
                ? json["name"].as<String>() : "";
            _keypadCommandEncCode = json.containsKey("code")
                ? json["code"].as<String>() : "";
            _keypadCommandEnabled = json.containsKey("enabled")
                ? json["enabled"].as<int>() : 0;

            if (!command || !*command)
            {
                sendResponse(jsonResult, "invalid data", 400);
                return;
            }

            _keypadCommandReceivedReceivedCallback(
                command, _keypadCommandId, _keypadCommandName,
                _keypadCommandEncCode, _keypadCommandEnabled);

            _keypadCommandId      = 0;
            _keypadCommandName    = "";
            _keypadCommandEncCode = "000000";
            _keypadCommandEnabled = 1;
            return;
        }
    }

    // Query commands — only processed when data == "1"
    if (strcmp(data, "1") == 0)
    {
        bool queryCmdSet = false;

        if (comparePrefixedPath(path, api_path_lock, api_path_query_config))
        {
            _queryCommands |= QUERY_COMMAND_CONFIG;
            queryCmdSet = true;
        }
        else if (comparePrefixedPath(path, api_path_lock, api_path_query_lockstate))
        {
            _queryCommands |= QUERY_COMMAND_LOCKSTATE;
            queryCmdSet = true;
        }
        else if (comparePrefixedPath(path, api_path_lock, api_path_query_keypad))
        {
            _queryCommands |= QUERY_COMMAND_KEYPAD;
            queryCmdSet = true;
        }
        else if (comparePrefixedPath(path, api_path_lock, api_path_query_battery))
        {
            _queryCommands |= QUERY_COMMAND_BATTERY;
            queryCmdSet = true;
        }

        if (queryCmdSet)
        {
            sendResponse(jsonResult);
            return;
        }
    }

    if (comparePrefixedPath(path, api_path_lock, api_path_config_action))
    {
        if (!data || !*data)
        {
            sendResponse(jsonResult, "missing data", 400);
            return;
        }
        if (_configUpdateReceivedCallback != nullptr)
            _configUpdateReceivedCallback(data);
        return;
    }

    if (comparePrefixedPath(path, api_path_lock, api_path_timecontrol_action))
    {
        if (!data || !*data)
        {
            sendResponse(jsonResult, "missing data", 400);
            return;
        }
        if (_timeControlCommandReceivedReceivedCallback != nullptr)
            _timeControlCommandReceivedReceivedCallback(data);
        return;
    }

    if (comparePrefixedPath(path, api_path_lock, api_path_auth_action))
    {
        if (!data || !*data)
        {
            sendResponse(jsonResult, "missing data", 400);
            return;
        }
        if (_authCommandReceivedReceivedCallback != nullptr)
            _authCommandReceivedReceivedCallback(data);
        return;
    }
}

void RestApiServer::onShutdownReceived(const char* path, WebServer& server)
{
    Log->println(F("[INFO] (REST API) Shutdown request received"));
    Log->disableFileLog();
    TaskWdtResetAndDelay(10);
    _disableHarFn();    // disable HarClient (avoids circular NukiNetwork dependency)
    disable();          // disable this REST API server
    _preferences->end();
    safeShutdownESP(RestartReason::SafeShutdownRequestViaApi);
}

char* RestApiServer::getArgs(WebServer& server)
{
    _argsBuffer[0] = '\0';

    // Count only data args — "token" may still appear as a legacy query param
    uint8_t dataArgCount = 0;
    for (uint8_t i = 0; i < server.args(); i++)
    {
        if (server.argName(i) != "token")
            dataArgCount++;
    }

    if (dataArgCount == 1)
    {
        // Single data arg: return value if named "val", otherwise return its name
        for (uint8_t i = 0; i < server.args(); i++)
        {
            if (server.argName(i) == "token") continue;
            if (server.argName(i) == "val")
                strlcpy(_argsBuffer, server.arg(i).c_str(), sizeof(_argsBuffer));
            else
                strlcpy(_argsBuffer, server.argName(i).c_str(), sizeof(_argsBuffer));
            break;
        }
    }
    else if (dataArgCount > 1)
    {
        // Multiple data args: serialize as JSON, skip any legacy "token" arg
        JsonDocument doc;
        for (uint8_t i = 0; i < server.args(); i++)
        {
            if (server.argName(i) != "token")
                doc[server.argName(i)] = server.arg(i);
        }
        serializeJson(doc, _argsBuffer, sizeof(_argsBuffer));
    }

    return _argsBuffer; // empty if dataArgCount == 0
}

// -----------------------------------------------------------------------
// Token authentication helper
// -----------------------------------------------------------------------

// constTimeStrEqual() is now in util/AuthUtils.h (inline, testable natively).

bool RestApiServer::isAuthenticated(WebServer& server) const
{
    // ── Check 1: IP allowlist ──────────────────────────────────────────────
    // If an allowed IP is configured (e.g. the Loxone Miniserver address),
    // reject any request that originates from a different host. This limits
    // the attack surface even though the token travels as a query parameter
    // over plain HTTP (Loxone Miniserver Gen 1 does not support HTTPS or
    // custom HTTP headers).
    if (_allowedIp.length() > 0 && _allowedIp != "0.0.0.0")
    {
        const String remoteIp = server.client().remoteIP().toString();
        if (remoteIp != _allowedIp)
        {
            Log->printf(F("[WARNING] REST API: request from non-allowlisted IP %s rejected\n"),
                        remoteIp.c_str());
            return false;
        }
    }

    // ── Check 2: Token (constant-time) ────────────────────────────────────
    // The Loxone Miniserver Gen 1 sends the token as a query parameter
    // (?token=...). Custom headers and HTTPS are not supported by that client.
    if (!server.hasArg("token"))
        return false;

    return constTimeStrEqual(server.arg("token").c_str(), _apitoken->get());
}

bool RestApiServer::comparePrefixedPath(const char* fullPath,
                                        const char* mainPath,
                                        const char* subPath)
{
    char prefixedPath[385];
    buildApiPath(mainPath, subPath, prefixedPath);
    return strcmp(fullPath, prefixedPath) == 0;
}

void RestApiServer::buildApiPath(const char* mainPath,
                                 const char* subPath,
                                 char* outPath)
{
    // Copy (mainPath) to outPath
    strncpy(outPath, mainPath, 384);
    outPath[384] = '\0'; // Zero terminate for safety

    // Append the (path) to outPath
    strncat(outPath, subPath, 384 - strlen(outPath));
}