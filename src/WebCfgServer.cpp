#include <Arduino.h>
#include <ESPmDNS.h>
#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "NetworkDeviceType.h"
#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
#include "esp_psram.h"
#endif

const char css[] PROGMEM = ":root{--nc-font-sans:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,'Open Sans','Helvetica Neue',sans-serif,'Apple Color Emoji','Segoe UI Emoji','Segoe UI Symbol';--nc-font-mono:Consolas,monaco,'Ubuntu Mono','Liberation Mono','Courier New',Courier,monospace;--nc-tx-1:#000;--nc-tx-2:#1a1a1a;--nc-bg-1:#fff;--nc-bg-2:#f6f8fa;--nc-bg-3:#e5e7eb;--nc-lk-1:#0070f3;--nc-lk-2:#0366d6;--nc-lk-tx:#fff;--nc-ac-1:#79ffe1;--nc-ac-tx:#0c4047}@media(prefers-color-scheme:dark){:root{--nc-tx-1:#fff;--nc-tx-2:#eee;--nc-bg-1:#000;--nc-bg-2:#111;--nc-bg-3:#222;--nc-lk-1:#3291ff;--nc-lk-2:#0070f3;--nc-lk-tx:#fff;--nc-ac-1:#7928ca;--nc-ac-tx:#fff}}*{margin:0;padding:0}img,input,option,p,table,textarea,ul{margin-bottom:1rem}button,html,input,select{font-family:var(--nc-font-sans)}body{margin:0 auto;max-width:750px;padding:2rem;border-radius:6px;overflow-x:hidden;word-break:normal;overflow-wrap:anywhere;background:var(--nc-bg-1);color:var(--nc-tx-2);font-size:1.03rem;line-height:1.5}::selection{background:var(--nc-ac-1);color:var(--nc-ac-tx)}h1,h2,h3,h4,h5,h6{line-height:1;color:var(--nc-tx-1);padding-top:.875rem}h1,h2,h3{color:var(--nc-tx-1);padding-bottom:2px;margin-bottom:8px;border-bottom:1px solid var(--nc-bg-2)}h4,h5,h6{margin-bottom:.3rem}h1{font-size:2.25rem}h2{font-size:1.85rem}h3{font-size:1.55rem}h4{font-size:1.25rem}h5{font-size:1rem}h6{font-size:.875rem}a{color:var(--nc-lk-1)}a:hover{color:var(--nc-lk-2) !important;}abbr{cursor:help}abbr:hover{cursor:help}a button,button,input[type=button],input[type=reset],input[type=submit]{font-size:1rem;display:inline-block;padding:6px 12px;text-align:center;text-decoration:none;white-space:nowrap;background:var(--nc-lk-1);color:var(--nc-lk-tx);border:0;border-radius:4px;box-sizing:border-box;cursor:pointer;color:var(--nc-lk-tx)}a button[disabled],button[disabled],input[type=button][disabled],input[type=reset][disabled],input[type=submit][disabled]{cursor:default;opacity:.5;cursor:not-allowed}.button:focus,.button:hover,button:focus,button:hover,input[type=button]:focus,input[type=button]:hover,input[type=reset]:focus,input[type=reset]:hover,input[type=submit]:focus,input[type=submit]:hover{background:var(--nc-lk-2)}table{border-collapse:collapse;width:100%}td,th{border:1px solid var(--nc-bg-3);text-align:left;padding:.5rem}th{background:var(--nc-bg-2)}tr:nth-child(even){background:var(--nc-bg-2)}textarea{max-width:100%}input,select,textarea{padding:6px 12px;margin-bottom:.5rem;background:var(--nc-bg-2);color:var(--nc-tx-2);border:1px solid var(--nc-bg-3);border-radius:4px;box-shadow:none;box-sizing:border-box}img{max-width:100%}td>input{margin-top:0;margin-bottom:0}td>textarea{margin-top:0;margin-bottom:0}td>select{margin-top:0;margin-bottom:0}.warning{color:red}@media only screen and (max-width:600px){.adapt td{display:block}.adapt input[type=text],.adapt input[type=password],.adapt input[type=submit],.adapt textarea,.adapt select{width:100%}.adapt td:has(input[type=checkbox]){text-align:center}.adapt input[type=checkbox]{width:1.5em;height:1.5em}.adapt table td:first-child{border-bottom:0}.adapt table td:last-child{border-top:0}#tblnav a li>span{max-width:140px}}#tblnav a{border:0;border-bottom:1px solid;display:block;font-size:1rem;font-weight:bold;padding:.6rem 0;line-height:1;color:var(--nc-tx-1);text-decoration:none;background:linear-gradient(to left,transparent 50%,rgba(255,255,255,0.4) 50%) right;background-size:200% 100%;transition:all .2s ease}#tblnav a{background:linear-gradient(to left,var(--nc-bg-2) 50%,rgba(255,255,255,0.4) 50%) right;background-size:200% 100%}#tblnav a:hover{background-position:left;transition:all .45s ease}#tblnav a:active{background:var(--nc-lk-1);transition:all .15s ease}#tblnav a li{list-style:none;padding:.5rem;display:inline-block;width:100%}#tblnav a li>span{float:right;text-align:right;margin-right:10px;color:#f70;font-weight:100;font-style:italic;display:block}.tdbtn{text-align:center;vertical-align:middle}.naventry{float:left;max-width:375px;width:100%}.tab-button.active{background-color: var(--nc-ac-1);color: var(--nc-ac-tx);font-weight: bold;}";
extern bool timeSynced;

WebCfgServer::WebCfgServer(NukiWrapper *nuki, NukiNetwork *network, Preferences *preferences)
    : _nuki(nuki),
      _network(network),
      _preferences(preferences)

{
    _webServer = new WebServer(WEBCFGSERVER_PORT);
    if (_webServer == nullptr)
    {
        Log->println(F("[ERROR] Failed to allocate memory for WebServer!"));
    }
    _hostname = _preferences->getString(preference_hostname, "");
    String str = _preferences->getString(preference_cred_user, "");
    str = _preferences->getString(preference_cred_user, "");
    _allowRestartToPortal = (network->networkDeviceType() == NetworkDeviceType::WiFi);

    if (str.length() > 0)
    {
        memset(&_credUser, 0, sizeof(_credUser));
        memset(&_credPassword, 0, sizeof(_credPassword));

        const char *user = str.c_str();
        memcpy(&_credUser, user, str.length());

        str = _preferences->getString(preference_cred_password, "");
        const char *pass = str.c_str();
        memcpy(&_credPassword, pass, str.length());

        if (_preferences->getInt(preference_http_auth_type, 0) == 2)
        {
            loadSessions();
        }
    }
    _confirmCode = generateConfirmCode();
}

WebCfgServer::~WebCfgServer()
{
    _webServer->close();
    delete _webServer;
    _webServer = nullptr;
}

bool WebCfgServer::isAuthenticated(WebServer *server)
{
    String cookieKey = "sessionId";

    // Get all cookies as a string
    String cookieHeader = server->header("Cookie");

    if (cookieHeader.length() > 0)
    {
        // Search the cookie value
        int startIndex = cookieHeader.indexOf(cookieKey + "=");
        if (startIndex != -1)
        {
            startIndex += cookieKey.length() + 1; // Position after the equal sign
            int endIndex = cookieHeader.indexOf(";", startIndex);
            if (endIndex == -1)
                endIndex = cookieHeader.length();

            String cookie = cookieHeader.substring(startIndex, endIndex);

            if (_httpSessions[cookie].is<JsonVariant>())
            {
                struct timeval time;
                gettimeofday(&time, NULL);
                int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;

                if (_httpSessions[cookie].as<signed long long>() > time_us)
                {
                    return true;
                }
                else
                {
                    Log->println(F("[DEBUG] Cookie found, but not valid anymore"));
                }
            }
        }
    }
    return false;
}

int WebCfgServer::doAuthentication(WebServer *server)
{
    if (!_network->isApOpen() && _preferences->getString(preference_bypass_proxy, "") != "" && server->client().localIP().toString() == _preferences->getString(preference_bypass_proxy, ""))
    {
        return 4;
    }
    else if (strlen(_credUser) > 0 && strlen(_credPassword) > 0)
    {
        int savedAuthType = _preferences->getInt(preference_http_auth_type, 0);
        if (savedAuthType == 2)
        {
            if (!isAuthenticated(server))
            {
                Log->println(F("[WARNING] Authentication Failed"));
                return savedAuthType;
            }
        }
        else
        {
            if (!server->authenticate(_credUser, _credPassword))
            {
                Log->println(F("[WARNING] Authentication Failed"));
                return savedAuthType;
            }
        }
    }

    return 4;
}

void WebCfgServer::initialize()
{
    Log->println(F("[DEBUG] WebCfgServer initializing..."));

    // Route für css Style
    _webServer->on("/style.css", HTTP_GET, [this]()
                   { sendCss(this->_webServer); });

    _webServer->onNotFound([this]()
                           { redirect(this->_webServer, "/"); });

    if (_network->isApOpen())
    {

        _webServer->on("/ssidlist", HTTP_GET, [this]()
                       { buildSSIDListHtml(this->_webServer); });

        _webServer->on("/saveconnset", HTTP_POST, [this]()
                       {            int authReq = doAuthentication(this->_webServer);
                       
                                   switch (authReq)
                                   {
                                       case 0:
                                           return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki REST Bridge", "You must log in.");
                                           break;
                                       case 1:
                                           return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki REST Bridge", "You must log in.");
                                           break;
                                       case 2:
                                           this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                                           return this->redirect(this->_webServer, "/get?page=login", 302);
                                           break;
                                       case 3:
                                       case 5:
                                       case 4:
                                       default:
                                           break;
                                   }
                       
                                   String message = "";
                                   int nwMode = 0;
                                   bool connected = processConnectionSettings(this->_webServer, message, nwMode);
                                   buildConfirmHtml(this->_webServer, message, 10, true);
                       
                                   if(connected)
                                   {
                                       Log->disableFileLog();
                                       waitAndProcess(true, 3000);
                                       if (nwMode > 1)
                                        restartEsp(RestartReason::ReconfigureETH);
                                       else
                                        restartEsp(RestartReason::ReconfigureWifi);
                                   }
                                   return; });

        _webServer->on("/reboot", HTTP_GET, [this]()
                       {
                                   int authReq = doAuthentication(this->_webServer);
                       
                                   switch (authReq)
                                   {
                                       case 0:
                                           return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki REST Bridge", "You must log in.");
                                           break;
                                       case 1:
                                           return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki REST Bridge", "You must log in.");
                                           break;
                                       case 2:
                                            this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                                            return this->redirect(this->_webServer, "/get?page=login", 302);
                                           break;
                                       case 3:
                                       case 5:
                                       case 4:
                                       default:
                                           break;
                                   }
                       
                                   String value = "";
                                   if (this->_webServer->hasArg("CONFIRMTOKEN")) 
                                   {
                                       value = this->_webServer->arg("CONFIRMTOKEN"); 
                               
                                       if (value == "")
                                       {
                                           buildConfirmHtml(this->_webServer, "No confirm code set.", 3, true);
                                           return;
                                       }
                                   }
                                   else
                                   {
                                       buildConfirmHtml(this->_webServer, "No confirm code set.", 3, true);
                                       return;
                                   }
                       
                                   if(value != _confirmCode)
                                   {
                                    this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                                    return this->redirect(this->_webServer, "/", 302);
                                   }
                                   buildConfirmHtml(this->_webServer, "Rebooting...", 2, true);
                                   Log->disableFileLog();
                                   waitAndProcess(true, 1000);
                                   restartEsp(RestartReason::RequestedViaWebCfgServer);
                                   return; });
    }
    else
    {
        _webServer->on("/get", HTTP_GET, [this]()
                       {
            String value = "";
            if(this->_webServer->hasArg("page"))
            {
                value = this->_webServer->arg("page");
            }

            bool adminKeyValid = false;
            if(timeSynced && this->_webServer->hasArg("adminkey"))
            {
                String value2 = "";
                if(this->_webServer->hasArg("adminkey"))
                {
                    value2 = this->_webServer->arg("adminkey");
                }
                if (value2.length() > 0 && value2 == _preferences->getString(preference_admin_secret, ""))
                {
                    adminKeyValid = true;
                }
            }

            if (!adminKeyValid && value != "status" && value != "login" && value != "bypass")
            {
                int authReq = doAuthentication(this->_webServer);

                switch (authReq)
                {
                    case 0:
                        return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki REST Bridge", "You must log in.");
                        break;
                    case 1:
                        return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki REST Bridge", "You must log in.");
                        break;
                    case 2:
                    this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                    return this->redirect(this->_webServer, "/get?page=login", 302);
                        break;
                    case 3:
                    case 5:
                    case 4:
                    default:
                        break;
                }
            }
            else if (value == "status")
            {
                if (doAuthentication(this->_webServer) != 4)
                {
                    this->_webServer->send(200, F("application/json"), "{}");
                }
            }
            if (value == "login")
            {
                return buildLoginHtml(this->_webServer);
            }
            else if (value == "logout")
            {
                return logoutSession(this->_webServer);
            }
            else if (value == "coredump")
            {
                return buildGetCoredumpFileHtml(this->_webServer);
            }
            else if (value == "logfile")
            {
                return buildGetLogFileHtml(this->_webServer);
            }
            else if (value == "clearlog")
            {
                return Log->clear();
            }
            else if (value == "reboot")
            {
                String value2 = "";
                if(this->_webServer->hasArg("CONFIRMTOKEN"))
                {
                    value2 = this->_webServer->arg("CONFIRMTOKEN");
                }
                else
                {
                    return buildConfirmHtml(this->_webServer, "No confirm code set.", 3, true);
                }

                if(value2 != _confirmCode)
                {
                    this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                    return this->redirect(this->_webServer, "/", 302);
                }
                buildConfirmHtml(this->_webServer, "Rebooting...", 2, true);
                Log->disableFileLog();
                waitAndProcess(true, 1000);
                restartEsp(RestartReason::RequestedViaWebCfgServer);
                return;
            }
            else if (value == "shutdown")
            {
                buildConfirmHtml(this->_webServer, "Shutting down...", 2, true);
                Log->disableFileLog();
                _network->disableHAR();
                _network->disableAPI();
                _preferences->end();
                waitAndProcess(true, 1000);
                return safeShutdownESP(RestartReason::SafeShutdownRequestViaWebCfgServer);   
            }
            else if (value == "info")
            {
                return buildInfoHtml(this->_webServer);
            }
            else if (value == "debugon")
            {
                _preferences->putBool(preference_enable_debug_mode, true);
                return buildConfirmHtml(this->_webServer, "Debug On", 3, true);
            }
            else if (value == "debugoff")
            {
                _preferences->putBool(preference_enable_debug_mode, false);
                return buildConfirmHtml(this->_webServer, "Debug Off", 3, true);
            }
            else if (value == "status")
            {
                return buildStatusHtml(this->_webServer);
            }
            else if (value == "acclvl")
            {
                return buildAccLvlHtml(this->_webServer);
            }
            else if (value == "advanced")
            {
                return buildAdvancedConfigHtml(this->_webServer);
            }
            else if (value == "cred")
            {
                return buildCredHtml(this->_webServer);
            }
            else if (value == "logging")
            {
                return buildLoggingHtml(this->_webServer);
            }
            else if (value == "ntwconfig")
            {
                return buildNetworkConfigHtml(this->_webServer);
            }
            else if (value == "apiconfig")
            {
                if (this->_webServer->hasArg("genapitoken") && this->_webServer->arg("genapitoken") == "1")
                {
                    _network->assignNewApiToken();
                    this->_webServer->sendHeader(F("Cache-Control"), F("no-cache"));
                    return this->redirect(this->_webServer, "/get?page=apiconfig", 302);
                }
                return buildApiConfigHtml(this->_webServer);
            }
            else if (value == "harconfig")
            {
                return buildHARConfigHtml(this->_webServer); 
            }
            else if (value == "nukicfg")
            {
                return buildNukiConfigHtml(this->_webServer);
            }
            else if (value == "wifi")
            {
                return buildConfigureWifiHtml(this->_webServer);
            }
            else if (value == "wifimanager")
            {
                String value2 = "";
                if(this->_webServer->hasArg("CONFIRMTOKEN"))
                {
                    value2 = this->_webServer->arg("CONFIRMTOKEN");
                }
                else
                {
                    return buildConfirmHtml(this->_webServer, "No confirm code set.", 3, true);
                }
                if(value2 != _confirmCode)
                {
                    this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                    return this->redirect(this->_webServer, "/", 302);
                }
                if(!_allowRestartToPortal)
                {
                    return buildConfirmHtml(this->_webServer, "Can't reset WiFi when network device is Ethernet", 3, true);
                }
                buildConfirmHtml(this->_webServer, "Restarting. Connect to ESP access point (\"NukiRestBridge\" with password \"NukiBridgeESP32\") to reconfigure Wi-Fi.", 0);
                waitAndProcess(false, 1000);
                return _network->reconfigure();
            }
            else
            {
                Log->println(F("[WARNING] Page not found, loading index"));
                this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                this->redirect(this->_webServer, "/", 302);
                return;
            } });

        _webServer->on("/post", HTTP_POST, [this]()
                       {
                String value = "";

                if(this->_webServer->hasArg("page"))
                {
                    value = this->_webServer->arg("page");
                }
    
                bool adminKeyValid = false;
                if(timeSynced && this->_webServer->hasArg("adminkey"))
                {
                    String value2 = "";
                    if(this->_webServer->hasArg("adminkey"))
                    {
                        value2 = this->_webServer->arg("adminkey");
                    }
                    if (value2.length() > 0 && value2 == _preferences->getString(preference_admin_secret, ""))
                    {
                        adminKeyValid = true;
                    }
                }
                if(!adminKeyValid && value != "login" && value != "bypass")
                {
                    int authReq = doAuthentication(this->_webServer);
    
                    switch (authReq)
                    {
                        case 0:
                            return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki REST Bridge", "You must log in.");
                            break;
                        case 1:
                            return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki REST Bridge", "You must log in.");
                            break;
                        case 2:
                        this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                        return this->redirect(this->_webServer, "/get?page=login", 302);
                            break;
                        case 3:
                        case 5:
                        case 4:
                        default:
                            break;
                    }
                }
    
                if (value == "login")
                {
                    bool loggedIn = processLogin(this->_webServer);
                    if (loggedIn)
                    {
                        this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                        return this->redirect(this->_webServer, "/", 302);
                    }
                    else
                    {
                        this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                        return this->redirect(this->_webServer, "/get?page=login", 302);
                    }
                }
                else if (value == "savecfg")
                {
                    String message = "";
                    bool restart = processArgs(this->_webServer, message);
                    return buildConfirmHtml(this->_webServer, message, 3, true);
                }
                else if (value == "unpairlock")
                {
                    processUnpair(this->_webServer);
                    return;
                }
                else if (value == "factoryreset")
                {
                    processFactoryReset(this->_webServer);
                    return;
                }
                else
                {
#ifndef CONFIG_IDF_TARGET_ESP32H2
                    if(!_network->isApOpen())
                    {
#endif
                        return buildHtml(this->_webServer);

#ifndef CONFIG_IDF_TARGET_ESP32H2
                    }
                    else
                    {
                        return buildConnectHtml(this->_webServer);
                    }
#endif
                } });
    }

    _webServer->on("/", HTTP_GET, [this]()
                   {
#ifdef DEBUG_NUKIBRIDGE
                       Log->println("[DEBUG] webCfgServer Anfrage erhalten : " + this->_webServer->uri());
#endif

                       int authReq = doAuthentication(this->_webServer);

                       switch (authReq)
                       {
                       case 0:
                           return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki REST Bridge", "You must log in.");
                           break;
                       case 1:
                           return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki REST BRidge", "You must log in.");
                           break;
                       case 2:
                           this->_webServer->sendHeader(F("Cache-Control"), F("no-cache"));
                           return this->redirect(this->_webServer, "/get?page=login", 302);
                           break;
                       case 3:
                       case 5:
                       case 4:
                       default:
                           break;
                       }

#ifndef CONFIG_IDF_TARGET_ESP32H2
                       if (!_network->isApOpen())
                       {
#endif
                           return buildHtml(this->_webServer);

#ifndef CONFIG_IDF_TARGET_ESP32H2
                       }
                       else
                       {
                           return buildConnectHtml(this->_webServer);
                       }
#endif
                   });

    // Actually starting the web server
    _webServer->begin();

    Log->println("[INFO] WebCfgServer started on http://" + _network->localIP() + ":" + String(WEBCFGSERVER_PORT));

    if (MDNS.begin(_preferences->getString(preference_hostname, "nukirestbridge").c_str()))
    {
        MDNS.addService("http", "tcp", WEBCFGSERVER_PORT);
    }
}

void WebCfgServer::redirect(WebServer *server, const char *url, int code)
{
    _webServer->sendHeader("Location", url, true); // HTTP Redirect
    _webServer->send(code, F("text/plain"), F("Redirecting to /"));
}

void WebCfgServer::handleClient()
{
    if (_webServer == nullptr)
    {
        Log->println(F("[ERROR] WebServer instance is NULL! Aborting handleClient."));
        return;
    }
    _webServer->handleClient();
}

void WebCfgServer::waitAndProcess(const bool blocking, const uint32_t duration)
{
    int64_t timeout = esp_timer_get_time() + (duration * 1000);
    while (esp_timer_get_time() < timeout)
    {
        if (blocking)
        {
            delay(10);
        }
        else
        {
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}

void WebCfgServer::buildAccLvlHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        21, // Checkboxes
                        0,  // Inputs
                        0,  // Dropdowns
                        0,  // Dropdown options
                        0,  // Textareas
                        0,  // Parameter rows
                        4   // Buttons
    );

    buildHtmlHeader(response);

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    response += F("<form method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<input type=\"hidden\" name=\"ACLLVLCHANGED\" value=\"1\">");
    response += F("<h3>Nuki General Access Control</h3><table><tr><th>Setting</th><th>Enabled</th></tr>");

    appendCheckBoxRow(response, "CONFNHCTRL", "Modify Nuki Bridge configuration over REST API", _preferences->getBool(preference_config_from_api, false));

    response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\">");

    if (_nuki)
    {
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[25];
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));

        response += F("<h3>Nuki Lock Access Control</h3>");
        response += F("<input type=\"button\" value=\"Allow all\" onclick=\"for(el of document.getElementsByClassName('chk_access_lock')){el.checked=true;}\">");
        response += F("<input type=\"button\" value=\"Disallow all\" onclick=\"for(el of document.getElementsByClassName('chk_access_lock')){el.checked=false;}\">");
        response += F("<table><tr><th>Action</th><th>Allowed</th></tr>");

        const char *lockActions[] = {
            "ACLLCKLCK", "ACLLCKUNLCK", "ACLLCKUNLTCH", "ACLLCKLNG", "ACLLCKLNGU",
            "ACLLCKFLLCK", "ACLLCKFOB1", "ACLLCKFOB2", "ACLLCKFOB3"};

        for (int i = 0; i < 9; i++)
        {
            appendCheckBoxRow(response, lockActions[i], lockActions[i] + 9, aclPrefs[i], "chk_access_lock");
        }

        response += F("</table><br><h3>Nuki Lock Config Control (Requires PIN to be set)</h3>");
        response += F("<input type=\"button\" value=\"Allow all\" onclick=\"for(el of document.getElementsByClassName('chk_config_lock')){el.checked=true;}\">");
        response += F("<input type=\"button\" value=\"Disallow all\" onclick=\"for(el of document.getElementsByClassName('chk_config_lock')){el.checked=false;}\">");
        response += F("<table><tr><th>Change</th><th>Allowed</th></tr>");

        const char *configActions[] = {
            "CONFLCKNAME", "CONFLCKLAT", "CONFLCKLONG", "CONFLCKAUNL", "CONFLCKPRENA",
            "CONFLCKBTENA", "CONFLCKLEDENA", "CONFLCKLEDBR", "CONFLCKTZOFF", "CONFLCKDSTM"};

        for (int i = 0; i < 10; i++)
        {
            appendCheckBoxRow(response, configActions[i], configActions[i] + 9, basicLockConfigAclPrefs[i], "chk_config_lock");
        }

        response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    }

    response += F("</form></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildNukiConfigHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        3, // Checkboxes
                        9  // Inputs
    );

    buildHtmlHeader(response);

    response += F("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<h3>Basic Nuki Configuration</h3><table>");

    appendCheckBoxRow(response, "LOCKENA", "Nuki Lock enabled", _preferences->getBool(preference_lock_enabled, true));
    appendCheckBoxRow(response, "CONNMODE", "New Nuki Bluetooth connection mode (disable if there are connection issues)", _preferences->getBool(preference_connect_mode, true));

    response += F("</table><br><h3>Advanced Nuki Configuration</h3><table>");

    appendInputFieldRow(response, "LSTINT", "Query interval lock state (seconds)", _preferences->getInt(preference_query_interval_lockstate), 10, "");
    appendInputFieldRow(response, "CFGINT", "Query interval configuration (seconds)", _preferences->getInt(preference_query_interval_configuration), 10, "");
    appendInputFieldRow(response, "BATINT", "Query interval battery (seconds)", _preferences->getInt(preference_query_interval_battery), 10, "");

    if ((_nuki && _nuki->hasKeypad()))
    {
        appendInputFieldRow(response, "KPINT", "Query interval keypad (seconds)", _preferences->getInt(preference_query_interval_keypad), 10, "");
    }

    appendInputFieldRow(response, "NRTRY", "Number of retries if command failed", _preferences->getInt(preference_command_nr_of_retries), 10, "");
    appendInputFieldRow(response, "TRYDLY", "Delay between retries (milliseconds)", _preferences->getInt(preference_command_retry_delay), 10, "");

    appendInputFieldRow(response, "RSBC", "Restart if Bluetooth beacons not received (seconds; -1 to disable)", _preferences->getInt(preference_restart_ble_beacon_lost), 10, "");

#if defined(CONFIG_IDF_TARGET_ESP32)
    appendInputFieldRow(response, "TXPWR", "BLE transmit power in dB (minimum -12, maximum 9)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");
#else
    appendInputFieldRow(response, "TXPWR", "BLE transmit power in dB (minimum -12, maximum 20)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");
#endif

    appendCheckBoxRow(response, "UPTIME", "Update Nuki Bridge and Lock time using NTP", _preferences->getBool(preference_update_time, false));
    appendInputFieldRow(response, "TIMESRV", "NTP server", _preferences->getString(preference_time_server, "pool.ntp.org").c_str(), 255, "");

    response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\"></form></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildAdvancedConfigHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        14,  // Checkboxes
                        10,  // Inputs
                        0,   // Dropdowns
                        0,   // Dropdown options
                        0,   // Textareas
                        2,   // Parameter rows
                        0,   // Buttons
                        0,   // menus
                        1024 // JavaScript/CSS
    );

    buildHtmlHeader(response);
    response += F("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<h3>Advanced Configuration</h3>");
    response += F("<h4 class=\"warning\">Warning: Changing these settings can lead to bootloops...</h4>");
    response += F("<table>");

    response += F("<tr><td>Current bootloop prevention state</td><td>");
    response += _preferences->getBool(preference_enable_bootloop_reset, false) ? F("Enabled") : F("Disabled");
    response += F("</td></tr>");

    appendCheckBoxRow(response, "DISNTWNOCON", "Disable Network if not connected within 60s", _preferences->getBool(preference_disable_network_not_connected, false));
    appendCheckBoxRow(response, "BTLPRST", "Enable Bootloop prevention (Try to reset these settings to default on bootloop)", true);

    appendInputFieldRow(response, "BUFFSIZE", "Char buffer size (min 4096, max 65536)", _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE), 6, "");
    response += F("<tr><td>Advised minimum char buffer size based on current settings</td><td id=\"mincharbuffer\"></td>");
    appendInputFieldRow(response, "TSKNTWK", "Task size Network (min 8192, max 65536)", _preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), 6, "");
    response += F("<tr><td>Advised minimum network task size based on current settings</td><td id=\"minnetworktask\"></td>");
    appendInputFieldRow(response, "TSKNUKI", "Task size Nuki (min 8192, max 65536)", _preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), 6, "");

    appendInputFieldRow(response, "ALMAX", "Max auth log entries (min 1, max 100)", _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 3, "id=\"inputmaxauthlog\"");
    appendInputFieldRow(response, "KPMAX", "Max keypad entries (min 1, max 200)", _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD), 3, "id=\"inputmaxkeypad\"");
    appendInputFieldRow(response, "TCMAX", "Max timecontrol entries (min 1, max 100)", _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL), 3, "id=\"inputmaxtimecontrol\"");
    appendInputFieldRow(response, "AUTHMAX", "Max authorization entries (min 1, max 100)", _preferences->getInt(preference_auth_max_entries, MAX_AUTH), 3, "id=\"inputmaxauth\"");

    appendCheckBoxRow(response, "SHOWSECRETS", "Show Pairing secrets on Info page", _preferences->getBool(preference_show_secrets));

    if (_preferences->getBool(preference_lock_enabled, true))
    {
        appendCheckBoxRow(response, "LCKMANPAIR", "Manually set lock pairing data (enable to save values below)", false);
        appendInputFieldRow(response, "LCKBLEADDR", "currentBleAddress", "", 12, "");
        appendInputFieldRow(response, "LCKSECRETK", "secretKeyK", "", 64, "");
        appendInputFieldRow(response, "LCKAUTHID", "authorizationId", "", 8, "");
        appendCheckBoxRow(response, "LCKISULTRA", "isUltra", false, "", "");
    }

    if (_nuki != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        appendCheckBoxRow(response, "LCKFORCEID", ((String) "Force Lock ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_lock_force_id, false));
        appendCheckBoxRow(response, "LCKFORCEKP", "Force Lock Keypad connected", _preferences->getBool(preference_lock_force_keypad, false));
        appendCheckBoxRow(response, "LCKFORCEDS", "Force Lock Doorsensor connected", _preferences->getBool(preference_lock_force_doorsensor, false));
    }

    appendCheckBoxRow(response, "DBGCONN", "Enable Nuki connect debug logging", _preferences->getBool(preference_debug_connect, false));
    appendCheckBoxRow(response, "DBGCOMMU", "Enable Nuki communication debug logging", _preferences->getBool(preference_debug_communication, false));
    appendCheckBoxRow(response, "DBGREAD", "Enable Nuki readable data debug logging", _preferences->getBool(preference_debug_readable_data, false));
    appendCheckBoxRow(response, "DBGHEX", "Enable Nuki hex data debug logging", _preferences->getBool(preference_debug_hex_data, false));
    appendCheckBoxRow(response, "DBGCOMM", "Enable Nuki command debug logging", _preferences->getBool(preference_debug_command, false));
    appendCheckBoxRow(response, "DBGHEAP", "Send free heap to Home Automation", _preferences->getBool(preference_send_debug_info, false));

    response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response += F("</form>");

    response += F("<script>window.onload = function() {"
                  "document.getElementById(\"inputmaxauthlog\").addEventListener(\"keyup\", calculate);"
                  "document.getElementById(\"inputmaxkeypad\").addEventListener(\"keyup\", calculate);"
                  "document.getElementById(\"inputmaxtimecontrol\").addEventListener(\"keyup\", calculate);"
                  "document.getElementById(\"inputmaxauth\").addEventListener(\"keyup\", calculate);"
                  "calculate(); };"
                  "function calculate() {"
                  "var auth = document.getElementById(\"inputmaxauth\").value;"
                  "var authlog = document.getElementById(\"inputmaxauthlog\").value;"
                  "var keypad = document.getElementById(\"inputmaxkeypad\").value;"
                  "var timecontrol = document.getElementById(\"inputmaxtimecontrol\").value;"
                  "var charbuf = 0, networktask = 0;"
                  "var sizeauth = 300 * auth, sizeauthlog = 280 * authlog, sizekeypad = 350 * keypad, sizetimecontrol = 120 * timecontrol;"
                  "charbuf = Math.max(sizeauth, sizeauthlog, sizekeypad, sizetimecontrol, 4096);"
                  "charbuf = Math.min(charbuf, 65536);"
                  "networktask = Math.max(4096 + charbuf, 8192);"
                  "networktask = Math.min(networktask, 65536);"
                  "document.getElementById(\"mincharbuffer\").innerHTML = charbuf;"
                  "document.getElementById(\"minnetworktask\").innerHTML = networktask;"
                  "}"
                  "</script></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildLoginHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        1,  // Checkbox
                        2,  // Input fields
                        0,  // Dropdowns
                        0,  // Dropdown options
                        0,  // Textareas
                        0,  // Parameter rows
                        0,  // Buttons
                        0,  // menus
                        704 // JS/CSS bytes
    );

    response += F("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    response += F("<style>form{border:3px solid #f1f1f1; max-width: 400px;}");
    response += F("input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;");
    response += F("display:inline-block;border:1px solid #ccc;box-sizing:border-box}");
    response += F("button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;");
    response += F("border:none;cursor:pointer;width:100%}button:hover{opacity:.8}");
    response += F(".container{padding:16px}span.password{float:right;padding-top:16px}");
    response += F("@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>");
    response += F("</head><body><center><h2>Nuki Bridge login</h2>");
    response += F("<form action=\"/post?page=login\" method=\"post\">");
    response += F("<div class=\"container\">");
    response += F("<label for=\"username\"><b>Username</b></label>");
    response += F("<input type=\"text\" placeholder=\"Enter Username\" name=\"username\" required>");
    response += F("<label for=\"password\"><b>Password</b></label>");
    response += F("<input type=\"password\" placeholder=\"Enter Password\" name=\"password\" required>");

    response += F("<button type=\"submit\">Login</button>");

    response += F("<label><input type=\"checkbox\" name=\"remember\"> Remember me</label></div>");
    response += F("</form></center></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildConfirmHtml(WebServer *server, const String &message, uint32_t redirectDelay, bool redirect, String redirectTo)
{
    String response;
    reserveHtmlResponse(response,
                        0,  // Checkboxes
                        0,  // Inputs
                        0,  // Dropdowns
                        0,  // Dropdown options
                        0,  // Textareas
                        0,  // Parameter rows
                        0,  // Buttons
                        0,  // menus
                        352 // extra bytes for JS/CSS
    );

    String header;
    header.reserve(384);
    if (!redirect)
    {
        header = F("<meta http-equiv=\"Refresh\" content=\"");
        header += String(redirectDelay);
        header += F("; url=/\" />");
    }
    else
    {
        header = F("<script type=\"text/JavaScript\">function Redirect() { window.location.href = \"");
        header += redirectTo;
        header += F("\"; } setTimeout(function() { Redirect(); }, ");
        header += String(redirectDelay * 1000);
        header += F("); </script>");
    }

    buildHtmlHeader(response, header);
    response += message;
    response += F("</body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildGetLogFileHtml(WebServer *server)
{
    if (!SPIFFS.begin(true))
    {
        Log->println(F("SPIFFS Mount Failed"));
        server->send(500, F("text/plain"), F("SPIFFS mount failed."));
        return;
    }

    File file = SPIFFS.open("/" + String(LOGGER_FILENAME), "r");

    if (!file || file.isDirectory())
    {
        Log->printf(F("%s not found\n"), LOGGER_FILENAME);
        server->send(404, F("text/plain"), F("Log file not found."));
        return;
    }

    server->sendHeader(F("Content-Disposition"), F("attachment; filename=\"Log.txt\""));
    server->streamFile(file, F("application/octet-stream"));
    file.close();
    return;
}

void WebCfgServer::buildGetCoredumpFileHtml(WebServer *server)
{
    if (!SPIFFS.begin(true))
    {
        Log->println(F("SPIFFS Mount Failed"));
        server->send(500, F("text/plain"), F("SPIFFS mount failed."));
        return;
    }

    File file = SPIFFS.open(F("/coredump.hex"), "r");

    if (!file || file.isDirectory())
    {
        Log->println(F("coredump.hex not found"));
        server->send(404, F("text/plain"), F("coredump.hex file not found."));
        return;
    }
    else
    {
        server->sendHeader(F("Content-Disposition"), F("attachment; filename=\"coredump.txt\""));
        server->streamFile(file, F("application/octet-stream"));
        file.close();
        return;
    }
}

void WebCfgServer::buildNetworkConfigHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        3, // Checkboxes
                        6, // Input fields
                        1, // Dropdowns
                        3  // Dropdown options
    );

    buildHtmlHeader(response);
    response += F("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<h3>Network Configuration</h3>");
    response += F("<table>");

    appendInputFieldRow(response, "HOSTNAME", "Hostname (needs to be unique, \"nukirestbridge\" is not allowed)", _preferences->getString(preference_hostname).c_str(), 100, "");
    appendDropDownRow(response, "NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions());

#ifndef CONFIG_IDF_TARGET_ESP32H2
    appendInputFieldRow(response, "RSSI", "RSSI send interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_send_interval), 6, "");
#endif

    appendCheckBoxRow(response, "RSTDISC", "Restart on disconnect", _preferences->getBool(preference_restart_on_disconnect), "", "");
    appendCheckBoxRow(response, "FINDBESTRSSI", "Find WiFi AP with strongest signal", _preferences->getBool(preference_find_best_rssi, false), "", "");

    response += F("</table>");
    response += F("<h3>IP Address assignment</h3>");
    response += F("<table>");

    appendCheckBoxRow(response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled), "", "");
    appendInputFieldRow(response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    appendInputFieldRow(response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    appendInputFieldRow(response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    appendInputFieldRow(response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");

    response += F("</table>");
    response += F("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response += F("</form>");
    response += F("</body></html>");

    server->send(200, F("text/html"), response);
}

#ifndef CONFIG_IDF_TARGET_ESP32H2
void WebCfgServer::buildConfigureWifiHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        0,  // checkboxCount
                        0,  // inputFieldCount
                        0,  // dropdownCount
                        0,  // dropdownOptionCountTotal
                        0,  // textareaCount
                        0,  // parameterRowCount
                        0,  // buttonCount
                        0,  // menus
                        400 // extra Bytes
    );

    buildHtmlHeader(response);

    response += F("<form method=\"get\" action=\"get\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"wifimanager\">");
    response += F("<h3>Wi-Fi</h3>");
    response += F("Click confirm to remove saved WiFi settings and restart ESP into Wi-Fi configuration mode. ");
    response += F("After restart, connect to ESP access point to reconfigure Wi-Fi.<br><br><br>");
    response += F("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"");
    response += _confirmCode + "\" />";
    response += F("<input type=\"submit\" value=\"Reboot\" /></form>");
    response += F("</form></body></html>");

    server->send(200, F("text/html"), response);
}
#endif

void WebCfgServer::buildCredHtml(WebServer *server)
{
    // Generate random strings for one-time bypass & admin key
    auto generateRandomString = [](char *buffer, size_t length, const char *chars, size_t charSize)
    {
        for (size_t i = 0; i < length; i++)
        {
            buffer[i] = chars[esp_random() % charSize];
        }
        buffer[length] = '\0';
    };

    const char chars[] = "234567ABCDEFGHJKLMNPQRSTUVWXYZ";
    const char chars2[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    char randomstr[17];
    generateRandomString(randomstr, 16, chars, sizeof(chars) - 1);

    char randomstr2[33];
    generateRandomString(randomstr2, 32, chars2, sizeof(chars2) - 1);

    char randomstr3[33];
    generateRandomString(randomstr3, 32, chars2, sizeof(chars2) - 1);

    // Build HTML response
    String response;
    reserveHtmlResponse(response,
                        1,  // Checkbox
                        11, // Input fields
                        1,  // Dropdown
                        3,  // Dropdown options
                        0,  // Textareas
                        0,  // Parameter rows
                        2,  // Buttons
                        0,  // menus
                        896 // extra bytes
    );

    buildHtmlHeader(response);
    response += F("<form id=\"credfrm\" class=\"adapt\" onsubmit=\"return testcreds();\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<h3>Web Configurator Credentials</h3><table>");

    appendInputFieldRow(response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 30, "id=\"inputuser\"", false, true);
    appendInputFieldRow(response, "CREDPASS", "Password", "*", 30, "id=\"inputpass\"", true, true);
    appendInputFieldRow(response, "CREDPASSRE", "Retype password", "*", 30, "id=\"inputpass2\"", true);

    std::vector<std::pair<String, String>> httpAuthOptions = {
        {"0", "Basic"},
        {"1", "Digest"},
        {"2", "Form"}};
    appendDropDownRow(response, "CREDDIGEST", "HTTP Authentication type", String(_preferences->getInt(preference_http_auth_type, 0)), httpAuthOptions);

    appendInputFieldRow(response, "CREDTRUSTPROXY", "Bypass authentication for reverse proxy with IP", _preferences->getString(preference_bypass_proxy, "").c_str(), 255, "");

    appendInputFieldRow(response, "CREDADMIN", "Admin key", "*", 32, "", true, false);
    response += F("<tr id=\"admingentr\" ><td><input type=\"button\" id=\"admingen\" onclick=\"document.getElementsByName('CREDADMIN')[0].type='text'; document.getElementsByName('CREDADMIN')[0].value='");
    response += randomstr3;
    response += F("'; document.getElementById('admingentr').style.display='none';\" value=\"Generate new Admin key\"></td></tr>");

    appendInputFieldRow(response, "CREDLFTM", "Session validity (in seconds)", _preferences->getInt(preference_cred_session_lifetime, 3600), 12, "");
    appendInputFieldRow(response, "CREDLFTMRMBR", "Session validity remember (in hours)", _preferences->getInt(preference_cred_session_lifetime_remember, 720), 12, "");

    response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\"></form>");

    // JavaScript validation
    response += F("<script>function testcreds() {"
                  "var input_user = document.getElementById(\"inputuser\").value;"
                  "var input_pass = document.getElementById(\"inputpass\").value;"
                  "var input_pass2 = document.getElementById(\"inputpass2\").value;"
                  "var pattern = /^[ -~]*$/;"
                  "if(input_user == '#' || input_user == '') { return true; }"
                  "if (input_pass != input_pass2) { alert('Passwords do not match'); return false;}"
                  "if(!pattern.test(input_user) || !pattern.test(input_pass)) { alert('Only non-unicode characters are allowed in username and password'); return false;}"
                  "return true; }</script>");

    // Nuki Lock settings
    if (_nuki != nullptr)
    {
        response += F("<br><br><form class=\"adapt\" method=\"post\" action=\"post\">");
        response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
        response += F("<h3>Nuki Lock PIN</h3><table>");
        appendInputFieldRow(response, "NUKIPIN", "PIN Code (# to clear)", "*", 20, "", true);
        response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\"></form>");
    }

    // Unpair Nuki Lock
    if (_nuki != nullptr)
    {
        response += F("<br><br><h3>Unpair Nuki Lock</h3><form class=\"adapt\" method=\"post\" action=\"/post\">");
        response += F("<input type=\"hidden\" name=\"page\" value=\"unpairlock\">");
        response += F("<table>");
        appendInputFieldRow(response, "CONFIRMTOKEN", ("Type " + _confirmCode + " to confirm unpair").c_str(), "", 10, "");
        response += F("</table><br><button type=\"submit\">OK</button></form>");
    }

    // Factory Reset
    response += F("<br><br><h3>Factory reset Nuki Bridge</h3><h4 class=\"warning\">This will reset all settings to default...</h4>");
    response += F("<form class=\"adapt\" method=\"post\" action=\"/post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"factoryreset\">");
    appendInputFieldRow(response, "CONFIRMTOKEN", ("Type " + _confirmCode + " to confirm factory reset").c_str(), "", 10, "");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    appendCheckBoxRow(response, "WIFI", "Also reset WiFi settings", false, "", "");
#endif
    response += F("</table><br><button type=\"submit\">OK</button></form></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildLoggingHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        1,   // Checkbox
                        10,  // Input fields
                        1,   // Dropdown
                        6,   // Dropdown options
                        0,   // Textareas
                        0,   // Parameter rows
                        2,   // Buttons
                        0,   // menus
                        1024 // extra bytes
    );

    buildHtmlHeader(response);
    response += F("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savelog\">");
    response += F("<h3>Logging Configuration</h3>");
    response += F("<table>");

    appendInputFieldRow(response, "LOGFILE", "Filename", LOGGER_FILENAME, 64, "readonly");
    appendInputFieldRow(response, "LOGMSGLEN", "Max. message length (min 1, max 1024)", _preferences->getInt(preference_log_max_msg_len, 128), 6, "min='1' max='1024'");
    appendInputFieldRow(response, "LOGMAXSIZE", "Max. log file size (min 256KB, max 1024KB)", _preferences->getInt(preference_log_max_file_size, 256), 6, "min='256' max='1024'");

    // Log level dropdown
    std::vector<std::pair<String, String>> lvlOptions;
    for (int i = 0; i <= 5; ++i)
    {
        Logger::msgtype lvl = static_cast<Logger::msgtype>(i);
        String key = String(i);
        String label = Log->levelToString(lvl);
        lvlOptions.emplace_back(key, label);
    }
    appendDropDownRow(response, "LOGLEVEL", "Log level for Nuki Bridge", String(_preferences->getInt(preference_log_level, 0)), lvlOptions);

    appendCheckBoxRow(response, "LOGBCKENA", "Enable FTP log backup", _preferences->getBool(preference_log_backup_enabled, false), "", "");
    appendInputFieldRow(response, "LOGBCKSRV", "FTP Server", _preferences->getString(preference_log_backup_ftp_server, "").c_str(), 64, "");
    appendInputFieldRow(response, "LOGBCKDIR", "FTP Directory", _preferences->getString(preference_log_backup_ftp_dir, "").c_str(), 64, "");
    appendInputFieldRow(response, "LOGBCKUSR", "FTP Username", _preferences->getString(preference_log_backup_ftp_user, "").c_str(), 32, "");
    appendInputFieldRow(response, "LOGBCKPWD", "FTP Password", _preferences->getString(preference_log_backup_ftp_pwd, "").c_str(), 32, "", true, true);

    response += F("</table><br>");
    response += F("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response += F("</form>");

    response += F("<div style=\"margin-top: 20px;\">");
    response += F("<input type=\"submit\" name=\"submit\" value=\"Save\" style=\"margin-right: 15px;\" />");

    response += F("<button type=\"button\" title=\"Download current log file\" ");
    response += F("onclick=\"window.open('/get?page=logfile'); return false;\" ");
    response += F("style=\"margin-right: 10px;\">Download Log</button>");

    response += F("<button type=\"button\" title=\"Clear log file\" ");
    response += F("onclick=\"if(confirm('Really clear log file?')) window.open('/get?page=clearlog'); return false;\" ");
    response += F("style=\"margin-right: 10px;\">Clear Log</button>");

    response += F("<button type=\"button\" title=\"Download current Coredump\" ");
    response += F("onclick=\"window.open('/get?page=coredump'); return false;\">Download Coredump</button>");

    response += F("</div>");
    response += F("</form>");
    response += F("</body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildApiConfigHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        1, // Checkbox
                        1, // Input fields
                        0, // Dropdown
                        0, // Dropdown options
                        0, // Textareas
                        1, // Parameter rows
                        1, // Buttons
                        0, // menus
                        0  // extra bytes
    );

    buildHtmlHeader(response);
    response += F("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<h3>REST API Configuration</h3><table>");

    appendCheckBoxRow(response, "APIENA", "Enable REST API", _preferences->getBool(preference_api_enabled, false), "", "");
    appendInputFieldRow(response, "APIPORT", "API Port", _preferences->getInt(preference_api_port, 8080), 6, "");

    const char *currentToken = _network->getApiToken();

    response += F("<tr><td>Access Token</td><td>");
    response += F("<input type=\"text\" value=\"");
    response += String(currentToken);
    response += F("\" readonly>");
    response += F("&nbsp;<a href=\"/get?page=apiconfig&genapitoken=1\"><input type=\"button\" value=\"Generate new token\"></a>");
    response += F("</td></tr>");

    response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\"></form></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildHARConfigHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        1,   // Checkbox
                        60,  // Input fields
                        2,   // Dropdown
                        4,   // Dropdown options
                        0,   // Textareas
                        0,   // Parameter rows
                        0,   // Buttons
                        0,   // menus
                        2048 // extra bytes
    );

    buildHtmlHeader(response);
    response += F("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<h3>Home Automation Report Configuration</h3>");
    response += F("<table>");

    appendCheckBoxRow(response, "HARENA", "Enable Home Automation Report", _preferences->getBool(preference_har_enabled, false), "", "");
    appendInputFieldRow(response, "HARHOST", "Address", _preferences->getString(preference_har_address, "").c_str(), 64, "");
    appendInputFieldRow(response, "HARPORT", "Port", _preferences->getInt(preference_har_port, 8081), 6, "");

    std::vector<std::pair<String, String>> modeOptions = {{"0", "UDP"}, {"1", "REST"}};
    appendDropDownRow(response, "HARMODE", "Mode", String(_preferences->getInt(preference_har_mode, 0)), modeOptions);

    // REST method selection (default GET)
    std::vector<std::pair<String, String>> restOptions = {{"0", "GET"}, {"1", "POST"}};
    appendDropDownRow(response, "HARRESTMODE", "REST Request Method", String(_preferences->getInt(preference_har_rest_mode, 0)), restOptions, "", "RestModeRow");

    appendInputFieldRow(response, "HARUSER", "Username", _preferences->getString(preference_har_user, "").c_str(), 32, "");
    appendInputFieldRow(response, "HARPASS", "Password", _preferences->getString(preference_har_password, "").c_str(), 32, "", true, true);

    response += F("</table><br>");

    // --- Tabs ---
    response += F("<div>");

    response += F("<button type=\"button\" class=\"tab-button\" onclick=\"showTab('");
    response += F(HAR_CAT_GENERAL);
    response += F("')\">General</button>");

    response += F("<button type=\"button\" class=\"tab-button\" onclick=\"showTab('");
    response += F(HAR_CAT_KEY_TURN_STATE);
    response += F("')\">Key Turner State</button>");

    response += F("<button type=\"button\" class=\"tab-button\" onclick=\"showTab('");
    response += F(HAR_CAT_BATTERY_REPORT);
    response += F("')\">Battery Report</button>");

    response += F("</div><br>");

    const struct
    {
        const char *tab;
        const char *id;
        const char *desc;
        const char *key;
        const char *param;
    } fields[] = {
        // --- General ---
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_STAT, "HA State", preference_har_key_state, nullptr},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_UPTM, "Uptime", preference_har_key_uptime, preference_har_param_uptime},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_RSTFW, "FW Restart Reason", preference_har_key_restart_reason_fw, preference_har_param_restart_reason_fw},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_RSTESP, "ESP Restart Reason", preference_har_key_restart_reason_esp, preference_har_param_restart_reason_esp},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_NBVER, "Bridge Version", preference_har_key_info_nuki_bridge_version, preference_har_param_info_nuki_bridge_version},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_NBBUILD, "Bridge Build", preference_har_key_info_nuki_bridge_build, preference_har_param_info_nuki_bridge_build},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_FREEHP, "Free Heap", preference_har_key_freeheap, preference_har_param_freeheap},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_WFRSSI, "Wi-Fi RSSI", preference_har_key_wifi_rssi, preference_har_param_wifi_rssi},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_BLEADDR, "BLE Address", preference_har_key_ble_address, preference_har_param_ble_address},
        {HAR_CAT_GENERAL, TOKEN_SUFFIX_BLERSSI, "BLE RSSI", preference_har_key_ble_rssi, preference_har_param_ble_rssi},

        // --- KeyTurnerState ---
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_LCKSTAT, "Lock State", preference_har_key_lock_state, preference_har_param_lock_state},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_LCKNGOSTAT, "Lock 'N' Go State", preference_har_key_lockngo_state, preference_har_param_lockngo_state},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_LCKTRIG, "Lock Trigger", preference_har_key_lock_trigger, preference_har_param_lock_trigger},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_LCKNMOD, "Night Mode", preference_har_key_lock_night_mode, preference_har_param_lock_night_mode},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_LCKCMPLSTAT, "Process Status", preference_har_key_lock_completionStatus, preference_har_param_lock_completionStatus},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_LCKBATCRIT, "Lock battery Critical", preference_har_key_lock_battery_critical, preference_har_param_lock_battery_critical},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_LCKBATLVL, "Lock battery Level", preference_har_key_lock_battery_level, preference_har_param_lock_battery_level},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_LCKBATCHRG, "Lock battery Charging", preference_har_key_lock_battery_charging, preference_har_param_lock_battery_charging},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_DOORSTAT, "Doorsensor State", preference_har_key_doorsensor_state, preference_har_param_doorsensor_state},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_DOORSCRIT, "Doorsensor Critical", preference_har_key_doorsensor_critical, preference_har_param_doorsensor_critical},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_KPCRIT, "Keypad Critical", preference_har_key_keypad_critical, preference_har_param_keypad_critical},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_REMACCSTAT, "Remote Access State", preference_har_key_remote_access_state, preference_har_param_remote_access_state},
        {HAR_CAT_KEY_TURN_STATE, TOKEN_SUFFIX_BLESTR, "BLE Strength", preference_har_key_ble_strength, preference_har_param_ble_strength},

        // --- BatteryReport ---
        {HAR_CAT_BATTERY_REPORT, TOKEN_SUFFIX_BATVOLT, "Voltage", preference_har_key_battery_voltage, preference_har_param_battery_voltage},
        {HAR_CAT_BATTERY_REPORT, TOKEN_SUFFIX_BATDRAIN, "Drain", preference_har_key_battery_drain, preference_har_param_battery_drain},
        {HAR_CAT_BATTERY_REPORT, TOKEN_SUFFIX_BATMAXTURNCUR, "Max Turn Current", preference_har_key_battery_max_turn_current, preference_har_param_battery_max_turn_current},
        {HAR_CAT_BATTERY_REPORT, TOKEN_SUFFIX_BATLCKDIST, "Lock Distance", preference_har_key_battery_lock_distance, preference_har_param_battery_lock_distance},
    };

    for (const char *cat : {HAR_CAT_GENERAL, HAR_CAT_KEY_TURN_STATE, HAR_CAT_BATTERY_REPORT})
    {
        response += "<div id='tab_" + String(cat) + "' style='display:none'>";
        response += F("<table>");
        for (const auto &f : fields)
        {
            if (strcmp(f.tab, cat) == 0)
            {
                appendInputFieldRow(response,
                                    ("KEY_" + String(f.id)).c_str(),
                                    (String(f.desc) + " Path:").c_str(),
                                    _preferences->getString(f.key, "").c_str(),
                                    64,
                                    "class='key-row'");
                if (f.param)
                    appendInputFieldRow(response,
                                        ("PARAM_" + String(f.id)).c_str(),
                                        (String(f.desc) + " Query:").c_str(),
                                        _preferences->getString(f.param, "").c_str(),
                                        64,
                                        "class='param-row' title='Query/Param (e.g. '?io=Q1&value=')'");
            }
        }
        response += F("</table></div>");
    }

    response += F("<br><input type=\"submit\" name=\"submit\" value=\"Save\"></form>");

    response += F("<script>"
                  "function showTab(tab) {");
    response += F("['");
    response += F(HAR_CAT_GENERAL);
    response += F("','");
    response += F(HAR_CAT_KEY_TURN_STATE);
    response += F("','");
    response += F(HAR_CAT_BATTERY_REPORT);
    response += F("']");
    response += F(".forEach((id, idx) => {"
                  "document.getElementById('tab_'+id).style.display=(id===tab)?'block':'none';"
                  "document.querySelectorAll('.tab-button')[idx].classList.toggle('active',id===tab);"
                  "});"
                  "}"
                  "function updateHarFields(){"
                  "var m=document.getElementsByName('HARMODE')[0].value;"
                  "var u=document.getElementsByName('HARUSER')[0];"
                  "var p=document.getElementsByName('HARPASS')[0];"
                  "var r=document.getElementById('RestModeRow');"
                  "var k=document.querySelectorAll('.key-row');"
                  "var pl=document.querySelectorAll('.param-label');"
                  "u.disabled=p.disabled=(m==='0');"
                  "r.style.display=(m==='0')?'none':'';"
                  "k.forEach(e=>e.style.display=(m==='0')?'none':'');"
                  "pl.forEach(l=>{l.innerHTML=l.innerHTML.replace(/:.*$/,m==='0'?'Param:':'Query:');});}"
                  "document.getElementsByName('HARMODE')[0].addEventListener('change', updateHarFields);"
                  "window.addEventListener('load',()=>{updateHarFields();showTab('general');});"
                  "</script></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildHtml(WebServer *server)
{
    String header = F(
        "<script>"
        "let intervalId;"
        "window.onload = function() {"
        "updateInfo();"
        "intervalId = setInterval(updateInfo, 3000);"
        "};"
        "function updateInfo() {"
        "var request = new XMLHttpRequest();"
        "request.open('GET', '/get?page=status', true);"
        "request.onload = () => {"
        "const obj = JSON.parse(request.responseText);"
        "if (obj.stop == 1) {"
        "clearInterval(intervalId);"
        "}"
        "for (var key of Object.keys(obj)) {"
        "if (key == 'ota' && document.getElementById(key) !== null) {"
        "document.getElementById(key).innerText = \"<a href='/ota'>\" + obj[key] + \"</a>\";"
        "} else if (document.getElementById(key) !== null) {"
        "document.getElementById(key).innerText = obj[key];"
        "}"
        "}"
        "};"
        "request.send();"
        "}"
        "</script>");

    String response;
    reserveHtmlResponse(response,
                        0,              // Checkbox
                        0,              // Input fields
                        0,              // Dropdown
                        0,              // Dropdown options
                        0,              // Textareas
                        7,              // Parameter rows
                        0,              // Buttons
                        14,             // Naviagtion menus
                        header.length() // extra bytes ()
    );

    buildHtmlHeader(response, header);

    if (_rebootRequired)
    {
        response += F(
            "<table>"
            "<tbody>"
            "<tr>"
            "<td colspan=\"2\" style=\""
            "border: 0; "
            "color: red; "
            "font-size: 32px; "
            "font-weight: bold; "
            "text-align: center;"
            "\">"
            "REBOOT REQUIRED TO APPLY SETTINGS"
            "</td>"
            "</tr>"
            "</tbody>"
            "</table>");
    }
#ifdef DEBUG_NUKIBRIDGE
    response += F("<table>"
                  "<tbody>"
                  "<tr>"
                  "<td colspan=\"2\" style=\""
                  "border: 0; "
                  "color: red; "
                  "font-size: 32px; "
                  "font-weight: bold; "
                  "text-align: center;"
                  "\">"
                  "RUNNING DEBUG BUILD, SWITCH TO RELEASE BUILD ASAP"
                  "</td>"
                  "</tr>"
                  "</tbody>"
                  "</table>");

#endif

    response += F("<h3>Info</h3><br><table>");
    appendParameterRow(response, "Hostname", _hostname.c_str(), "", "hostname");
    appendParameterRow(response, "REST API active", (_network->networkServicesState() == NetworkServiceState::OK || _network->networkServicesState() == NetworkServiceState::ERROR_REST_API_SERVER) ? "Yes" : "No", "", "APIState");
    appendParameterRow(response, "HAR active", (_network->networkServicesState() == NetworkServiceState::OK || _network->networkServicesState() == NetworkServiceState::ERROR_HAR_CLIENT) ? "Yes" : "No", "", "HARState");

    if (_nuki != nullptr)
    {
        char lockStateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockStateArr);
        appendParameterRow(response, "Nuki Lock paired", _nuki->isPaired() ? ("Yes (BLE Address " + _nuki->getBleAddress().toString() + ")").c_str() : "No", "", "lockPaired");
        appendParameterRow(response, "Nuki Lock state", lockStateArr, "", "lockState");

        if (_nuki->isPaired())
        {
            const String lockState = pinStateToString((NukiPinState)_preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured));
            appendParameterRow(response, "Nuki Lock PIN status", lockState.c_str(), "", "lockPin");
        }
    }

    appendParameterRow(response, "Firmware", NUKI_REST_BRIDGE_VERSION, "/get?page=info", "firmware");
    response += F("</table><br>"
                  "<ul id=\"tblnav\">");

    appendNavigationMenuEntry(response, "Network Configuration", "/get?page=ntwconfig");
    appendNavigationMenuEntry(response, "REST API Configuration", "/get?page=apiconfig");
    appendNavigationMenuEntry(response, "HAR Configuration", "/get?page=harconfig");
    appendNavigationMenuEntry(response, "Nuki Configuration", "/get?page=nukicfg");
    appendNavigationMenuEntry(response, "Access Level Configuration", "/get?page=acclvl");
    appendNavigationMenuEntry(response, "Credentials", "/get?page=cred");
    appendNavigationMenuEntry(response, "Log Configuration", "/get?page=logging");

    if (_preferences->getInt(preference_network_hardware, 0) == 11)
    {
        appendNavigationMenuEntry(response, "Custom Ethernet Configuration", "/get?page=custntw");
    }

    if (_preferences->getBool(preference_enable_debug_mode, false))
    {
        appendNavigationMenuEntry(response, "Advanced Configuration", "/get?page=advanced");
    }

#ifndef CONFIG_IDF_TARGET_ESP32H2
    if (_allowRestartToPortal)
    {
        appendNavigationMenuEntry(response, "Configure Wi-Fi", "/get?page=wifi");
    }
#endif

    appendNavigationMenuEntry(response, "Info page", "/get?page=info");
    String rebooturl = "/get?page=reboot&CONFIRMTOKEN=" + _confirmCode;
    appendNavigationMenuEntry(response, "Reboot Nuki Bridge", rebooturl.c_str());
    appendNavigationMenuEntry(response, "Shutdown", "/get?page=shutdown", "return confirm('Really Shutdown Nuki Bridge?');");


    if (_preferences->getInt(preference_http_auth_type, 0) == 2)
    {
        appendNavigationMenuEntry(response, "Logout", "/get?page=logout");
    }

    response += F("</ul></body></html>");

    server->send(200, F("text/html"), response);
}

#ifndef CONFIG_IDF_TARGET_ESP32H2
void WebCfgServer::buildSSIDListHtml(WebServer *server)
{
    _network->scan(true, false);
    createSsidList();

    String response;
    reserveHtmlResponse(response,
                        0,                     // Checkbox
                        0,                     // Input fields
                        0,                     // Dropdown
                        0,                     // Dropdown options
                        0,                     // Textareas
                        0,                     // Parameter rows
                        0,                     // Buttons
                        0,                     // Naviagtion menus
                        _ssidList.size() * 160 // extra bytes ()
    );

    for (size_t i = 0; i < _ssidList.size(); i++)
    {
        response += F("<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '");
        response += _ssidList[i];
        response += F("';\"><td colspan=\"2\">");
        response += _ssidList[i];
        response += F(" (");
        response += String(_rssiList[i]);
        response += F(" %)</td></tr>");
    }

    server->send(200, F("text/html"), response);
}
#endif

void WebCfgServer::buildConnectHtml(WebServer *server)
{
    const int currentHw = _preferences->getInt(preference_network_hardware, 1);
    auto hwOptions = getNetworkDetectionOptions();

    String header = F("<style>.trssid:hover { cursor: pointer; color: blue; }</style>"
                      "<script>"
                      "let intervalId = null;"
                      "function updateSSID() {"
                      "var request = new XMLHttpRequest();"
                      "request.open('GET', '/ssidlist', true);"
                      "request.onload = () => {"
                      "const aplist = document.getElementById(\"aplist\");"
                      "if (aplist !== null) { aplist.innerHTML = request.responseText; }"
                      "};"
                      "request.send();"
                      "}"
                      "function startScan() {"
                      "if (!intervalId) { intervalId = setInterval(updateSSID, 5000); }"
                      "}"
                      "function stopScan() {"
                      "if (intervalId) { clearInterval(intervalId); intervalId = null; }"
                      "}"
                      "function toggleMode() {"
                      "const isWifi = document.getElementById('nwmode').value === '1';"
                      "document.getElementById('wlanConfig').style.display = isWifi ? 'block' : 'none';"
                      "document.getElementById('inputssid').disabled = !isWifi;"
                      "document.getElementById('inputpass').disabled = !isWifi;"
                      "document.getElementById('cbfindbestrssi').disabled = !isWifi;"
                      "if (isWifi) { startScan(); updateSSID(); } else { stopScan(); }"
                      "}"
                      "window.onload = function() { toggleMode(); if (document.getElementById('nwmode').value === '1') { updateSSID(); } };"
                      "</script>");

    if (currentHw == 1)
        createSsidList();

    String response;
    reserveHtmlResponse(response,
                        2,                                         // Checkbox
                        7,                                         // Input fields
                        1,                                         // Dropdown
                        hwOptions.size(),                          // Dropdown options
                        0,                                         // Textareas
                        0,                                         // Parameter rows
                        2,                                         // Buttons
                        0,                                         // Navigation menus
                        (_ssidList.size() * 160) + header.length() // extra content
    );

    buildHtmlHeader(response, header);

    response += F("<form class=\"adapt\" method=\"post\" action=\"saveconnset\">");

    response += F("<h3>Connection Type</h3><table>");
    appendDropDownRow(response, "NWHW", "Network hardware", String(currentHw), hwOptions, "", "nwmode", "toggleMode()");
    response += F("</table>");

    // WLAN config (is shown/hidden via JS)
    response += F("<div id=\"wlanConfig\"><h3>Available WiFi networks</h3><table id=\"aplist\">");
    for (size_t i = 0; i < _ssidList.size(); i++)
    {
        response += F("<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '");
        response += _ssidList[i];
        response += F("';\"><td colspan=\"2\">");
        response += _ssidList[i];
        response += F(" (");
        response += String(_rssiList[i]);
        response += F(" %)</td></tr>");
    }
    response += F("</table><h3>WiFi credentials</h3><table>");
    appendInputFieldRow(response, "WIFISSID", "SSID", "", 32, "id=\"inputssid\"", false, true);
    appendInputFieldRow(response, "WIFIPASS", "Secret key", "", 63, "id=\"inputpass\"", false, true);
    appendCheckBoxRow(response, "FINDBESTRSSI", "Find AP with best signal (disable for hidden SSID)", _preferences->getBool(preference_find_best_rssi, true), "", "cbfindbestrssi");
    response += F("</table></div>");
    // --------- End WLAN config -----------

    response += F("<h3>IP Address assignment</h3><table>");
    appendCheckBoxRow(response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled));
    appendInputFieldRow(response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    appendInputFieldRow(response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    appendInputFieldRow(response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    appendInputFieldRow(response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");
    response += F("</table>");

    response += F("<br><input type=\"submit\" name=\"submit\" value=\"Save\"></form>");
    response += F("<form action=\"/reboot\" method=\"get\"><br>");
    response += F("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"");
    response += _confirmCode;
    response += F("\" />");
    response += F("<input type=\"submit\" value=\"Reboot\" />");
    response += F("</form></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildInfoHtml(WebServer *server)
{
    String devType;
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    String response;
    response.reserve(12288);

    buildHtmlHeader(response);
    response += F("<h3>System Information</h3><pre>");
    response += F("\n----------- NUKI BRIDGE ------------");
    response += "\nDevice: " + String(NUKI_REST_BRIDGE_HW);
    response += "\nVersion: " + String(NUKI_REST_BRIDGE_VERSION);
    response += "\nBuild: " + String(NUKI_REST_BRIDGE_BUILD);
#ifndef DEBUG_NUKIBRIDGE
    response += F("\nBuild type: Release");
#else
    response += F("\nBuild type: Debug");
#endif
    response += "\nBuild date: " + String(NUKI_REST_BRIDGE_DATE);
    response += "\nUptime (min): " + String(espMillis() / 1000 / 60);
    response += "\nLast restart reason FW: " + getRestartReason();
    response += "\nLast restart reason ESP: " + getEspRestartReason();
    response += "\nFree internal heap: " + String(ESP.getFreeHeap());
    response += "\nTotal internal heap: " + String(ESP.getHeapSize());

#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
    if (psramFound() && esp_psram_get_size() > 0)
    {
        response += F("\nPSRAM Available: Yes");
        response += F("\nFree usable PSRAM: ");
        response += String(ESP.getFreePsram());
        response += F("\nTotal usable PSRAM: ");
        response += String(ESP.getPsramSize());
        response += F("\nTotal PSRAM: ");
        response += String(esp_psram_get_size());
        response += F("\nTotal free heap: ");
        response += String(esp_get_free_heap_size());
    }
    else
    {
        response += F("\nPSRAM Available: No");
    }
#else
    response += F("\nPSRAM Available: No");
#endif

    response += "\nNetwork task stack high watermark: " + String(uxTaskGetStackHighWaterMark(networkTaskHandle));
    response += "\nNuki task stack high watermark: " + String(uxTaskGetStackHighWaterMark(nukiTaskHandle));
    response += "\nWeb configurator task stack high watermark: " + String(uxTaskGetStackHighWaterMark(webCfgTaskHandle));

    response += F("\n\n------------NUKI BRIDGE LOG ------------");
    response += F("\nMax message length: ");
    response += String(_preferences->getInt(preference_log_max_msg_len), 128);
    response += F("\nFilename: ");
    response += LOGGER_FILENAME;
    response += F("\nLevel: ");
    response += Log->levelToString(Log->getLevel());
    response += F("\nCurrent file size: ");
    response += String(Log->getFileSize());
    response += F(" kb");
    response += F("\nMax file size: ");
    response += String(_preferences->getInt(preference_log_max_file_size, 256));
    response += F(" kb");
    response += F("\nEnable backup to ftp server: ");
    response += _preferences->getBool(preference_log_backup_enabled, false) ? F("Yes") : F("No");
    response += F("\nBackup ftp server address: ");
    response += _preferences->getString(preference_log_backup_ftp_server, F("Not set"));
    response += F("\nBackup ftp server username: ");
    response += _preferences->getString(preference_log_backup_ftp_user, "").length() > 0 ? F("***") : F("Not set");
    response += F("\nBackup ftp server password: ");
    response += _preferences->getString(preference_log_backup_ftp_pwd, "").length() > 0 ? F("***") : F("Not set");
    response += F("\nBackup ftp server directory: ");
    response += _preferences->getString(preference_log_backup_ftp_dir, "");
    response += F("\nCurrent backup file index: ");
    response += String(_preferences->getInt(preference_log_backup_file_index, 1));
    response += F("\nMax backup file index before rollover: 100");

    response += F("\n\n------------ SPIFFS ------------");
    if (!SPIFFS.begin(true))
    {
        response += F("\nSPIFFS mount failed");
    }
    else
    {
        response += F("\nSPIFFS Total Bytes: ");
        response += String(SPIFFS.totalBytes());
        response += F("\nSPIFFS Used Bytes: ");
        response += String(SPIFFS.usedBytes());
        response += F("\nSPIFFS Free Bytes: ");
        response += String(SPIFFS.totalBytes() - SPIFFS.usedBytes());
    }

    response += F("\n\n------------ GENERAL SETTINGS ------------");
    response += F("\nNetwork task stack size: ");
    response += String(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE));
    response += F("\nNuki task stack size: ");
    response += String(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE));
    response += F("\nWeb Configurator task stack size: ");
    response += String(WEBCFGSERVER_TASK_SIZE);
    response += F("\nUpdate Nuki Bridge and Nuki devices time using NTP: ");
    response += _preferences->getBool(preference_update_time, false) ? F("Yes") : F("No");

    response += F("\nWeb configurator enabled: ");
    response += _preferences->getBool(preference_webcfgserver_enabled, true) ? F("Yes") : F("No");
    response += F("\nWeb configurator username: ");
    response += _preferences->getString(preference_cred_user, "").length() > 0 ? F("***") : F("Not set");
    response += F("\nWeb configurator password: ");
    response += _preferences->getString(preference_cred_password, "").length() > 0 ? F("***") : F("Not set");
    response += F("\nWeb configurator bypass for proxy IP: ");
    response += _preferences->getString(preference_bypass_proxy, "").length() > 0 ? F("***") : F("Not set");
    response += F("\nWeb configurator authentication: ");
    response += _preferences->getInt(preference_http_auth_type, 0) == 0 ? F("Basic") : _preferences->getInt(preference_http_auth_type, 0) == 1 ? F("Digest")
                                                                                                                                               : F("Form");
    response += F("\nSession validity (in seconds): ");
    response += String(_preferences->getInt(preference_cred_session_lifetime, 3600));
    response += F("\nSession validity remember (in hours): ");
    response += String(_preferences->getInt(preference_cred_session_lifetime_remember, 720));

    response += F("\nAdvanced menu enabled: ");
    response += _preferences->getBool(preference_enable_debug_mode, false) ? F("Yes") : F("No");
    response += F("\nSend free heap to HA: ");
    response += _preferences->getBool(preference_send_debug_info, false) ? F("Yes") : F("No");
    response += F("\nNuki connect debug logging enabled: ");
    response += _preferences->getBool(preference_debug_connect, false) ? F("Yes") : F("No");
    response += F("\nNuki communication debug logging enabled: ");
    response += _preferences->getBool(preference_debug_communication, false) ? F("Yes") : F("No");
    response += F("\nNuki readable data debug logging enabled: ");
    response += _preferences->getBool(preference_debug_readable_data, false) ? F("Yes") : F("No");
    response += F("\nNuki hex data debug logging enabled: ");
    response += _preferences->getBool(preference_debug_hex_data, false) ? F("Yes") : F("No");
    response += F("\nNuki command debug logging enabled: ");
    response += _preferences->getBool(preference_debug_command, false) ? F("Yes") : F("No");
    response += F("\nBootloop protection enabled: ");
    response += _preferences->getBool(preference_enable_bootloop_reset, false) ? F("Yes") : F("No");

    // Netzwerk-Infos
    response += F("\n\n------------ NETWORK ------------");
    if (_network->networkDeviceType() == NetworkDeviceType::ETH)
        devType = "LAN";
    else if (_network->networkDeviceType() == NetworkDeviceType::WiFi)
        devType = "WLAN";
    else
        devType = "Unbekannt";
    response += F("\nNetwork device: ");
    response += devType;
    response += F("\nNetwork connected: ");
    response += _network->isConnected() ? F("Yes") : F("No");
    if (_network->isConnected())
    {
        response += F("\nIP Address: ");
        response += _network->localIP();
        if (devType == "WLAN")
        {
            response += F("\nSSID: ");
            response += WiFi.SSID();
            response += F("\nESP32 MAC address: ");
            response += WiFi.macAddress();
        }
    }

    response += F("\n\n------------ NETWORK SETTINGS ------------");
    response += F("\nNuki Bridge hostname: ");
    response += _preferences->getString(preference_hostname, "");

    if (_preferences->getBool(preference_ip_dhcp_enabled, true))
    {
        response += F("\nDHCP enabled: Yes");
    }
    else
    {
        response += F("\nDHCP enabled: No");
        response += F("\nStatic IP address: ");
        response += _preferences->getString(preference_ip_address, "");
        response += F("\nStatic IP subnet: ");
        response += _preferences->getString(preference_ip_subnet, "");
        response += F("\nStatic IP gateway: ");
        response += _preferences->getString(preference_ip_gateway, "");
        response += F("\nStatic IP DNS server: ");
        response += _preferences->getString(preference_ip_dns_server, "");
    }

    if (devType == "WLAN")
    {
        response += F("\nRSSI send interval (s): ");
        if (_preferences->getInt(preference_rssi_send_interval, 60) < 0)
        {
            response += F("Disabled");
        }
        else
        {
            response += String(_preferences->getInt(preference_rssi_send_interval, 60));
        }

        response += F("\nFind WiFi AP with strongest signal: ");
        response += _preferences->getBool(preference_find_best_rssi, false) ? F("Yes") : F("No");
    }
    response += F("\nRestart ESP32 on network disconnect enabled: ");
    response += _preferences->getBool(preference_restart_on_disconnect, false) ? F("Yes") : F("No");
    response += F("\nDisable Network if not connected within 60s: ");
    response += _preferences->getBool(preference_disable_network_not_connected, false) ? F("Yes") : F("No");
    response += F("\nHA & API Timeout until restart (s): ");
    if (_preferences->getInt(preference_network_timeout, 60) < 0)
    {
        response += F("Disabled");
    }
    else
    {
        response += String(_preferences->getInt(preference_network_timeout, 60));
    }

    // REST Api Infos
    response += F("\n\n------------ REST API ------------");
    response += F("\nAPI enabled: ");
    response += _preferences->getBool(preference_api_enabled, false) != false ? F("Yes") : F("No");
    response += F("\nAPI connected: ");
    response += (_network->networkServicesState() == NetworkServiceState::OK || _network->networkServicesState() == NetworkServiceState::ERROR_HAR_CLIENT) ? F("Yes") : F("No");
    response += F("\nAPI Port: ");
    response += String(_preferences->getInt(preference_api_port, 0));
    response += F("\nAPI auth token: ");
    response += _preferences->getString(preference_api_token).length() > 0 ? F("***") : F("Not set");

    // HomeAutomation
    response += F("\n\n------------ HOME AUTOMATION REPORTING ------------");
    response += F("\nHAR enabled: ");
    response += _preferences->getBool(preference_har_enabled, false) != false ? F("Yes") : F("No");
    response += F("\nHA reachable: ");
    response += (_network->networkServicesState() == NetworkServiceState::OK || _network->networkServicesState() == NetworkServiceState::ERROR_REST_API_SERVER) ? F("Yes") : F("No");
    response += F("\nHA address: ");
    response += _preferences->getString(preference_har_address, F("Not set"));
    response += F("\nHA user: ");
    response += _preferences->getString(preference_har_user).length() > 0 ? F("***") : F("Not set");
    response += F("\nHA password: ");
    response += _preferences->getString(preference_har_password).length() > 0 ? F("***") : F("Not set");
    response += F("\nHAR mode: ");
    response += _preferences->getString(preference_har_mode, F("Not set"));

    // Bluetooth Infos
    response += F("\n\n------------ BLUETOOTH ------------");
    response += F("\nBluetooth connection mode: ");
    response += _preferences->getBool(preference_connect_mode, true) ? F("New") : F("Old");
    response += F("\nBluetooth TX power (dB): ");
    response += String(_preferences->getInt(preference_ble_tx_power, 9));
    response += F("\nBluetooth command nr of retries: ");
    response += String(_preferences->getInt(preference_command_nr_of_retries, 3));
    response += F("\nBluetooth command retry delay (ms): ");
    response += String(_preferences->getInt(preference_command_retry_delay, 100));
    response += F("\nSeconds until reboot when no BLE beacons received: ");
    response += String(_preferences->getInt(preference_restart_ble_beacon_lost, 60));

    response += F("\n\n------------ QUERY / REPORT SETTINGS ------------");
    response += F("\nLock state query interval (s): ");
    response += String(_preferences->getInt(preference_query_interval_lockstate, 1800));
    response += F("\nBattery state query interval (s): ");
    response += String(_preferences->getInt(preference_query_interval_battery, 1800));
    response += F("\nConfig query interval (s): ");
    response += String(_preferences->getInt(preference_query_interval_configuration, 3600));
    response += F("\nKeypad query interval (s): ");
    response += String(_preferences->getInt(preference_query_interval_keypad, 1800));
    response += F("\nEnable Keypad control: ");
    response += _preferences->getBool(preference_keypad_control_enabled, false) ? F("Yes") : F("No");
    response += F("\nAllow checking Keypad codes: ");
    response += _preferences->getBool(preference_keypad_check_code_enabled, false) ? F("Yes") : F("No");
    response += F("\nMax keypad entries to retrieve: ");
    response += String(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
    response += F("\nKeypad query interval (s): ");
    response += String(_preferences->getInt(preference_query_interval_keypad, 1800));
    response += F("\nEnable timecontrol control: ");
    response += _preferences->getBool(preference_timecontrol_control_enabled, false) ? F("Yes") : F("No");
    response += F("\nMax timecontrol entries to retrieve: ");
    response += String(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL));
    response += F("\nEnable authorization control: ");
    response += _preferences->getBool(preference_auth_info_enabled, false) ? F("Yes") : F("No");
    response += F("\nMax authorization entries to retrieve: ");
    response += String(_preferences->getInt(preference_auth_max_entries, MAX_AUTH));

    // Nuki Lock Infos
    response += F("\n\n------------ NUKI LOCK ------------");
    if (_nuki == nullptr || !_preferences->getBool(preference_lock_enabled, true))
    {
        response += F("\nLock enabled: No");
    }
    else
    {
        response += F("\nLock enabled: Yes");
        response += F("\nPaired: ");
        response += _nuki->isPaired() ? F("Yes") : F("No");
        response += F("\nNuki Bridge device ID: ");
        response += String(_preferences->getUInt(preference_device_id_lock, 0));
        response += F("\nNuki device ID: ");
        response += _preferences->getUInt(preference_nuki_id_lock, 0) > 0 ? F("***") : F("Not set");
        response += F("\nFirmware version: ");
        response += _nuki->firmwareVersion();
        response += F("\nHardware version: ");
        response += _nuki->hardwareVersion();
        response += F("\nValid PIN set: ");
        response += _nuki->isPaired() ? (_nuki->isPinValid() ? F("Yes") : F("No")) : F("-");
        response += F("\nHas door sensor: ");
        response += _nuki->hasDoorSensor() ? F("Yes") : F("No");
        response += F("\nHas keypad: ");
        response += _nuki->hasKeypad() ? F("Yes") : F("No");
        if (_nuki->hasKeypad())
        {
            response += F("\nKeypad highest entries count: ");
            response += String(_preferences->getInt(preference_lock_max_keypad_code_count, 0));
        }
        response += F("\nTimecontrol highest entries count: ");
        response += String(_preferences->getInt(preference_lock_max_timecontrol_entry_count, 0));
        response += F("\nAuthorizations highest entries count: ");
        response += String(_preferences->getInt(preference_lock_max_auth_entry_count, 0));
        response += F("\nRegister as: Bridge");

        response += F("\nForce Lock ID: ");
        response += _preferences->getBool(preference_lock_force_id, false) ? F("Yes") : F("No");
        response += F("\nForce Lock Keypad: ");
        response += _preferences->getBool(preference_lock_force_keypad, false) ? F("Yes") : F("No");
        response += F("\nForce Lock Doorsensor: ");
        response += _preferences->getBool(preference_lock_force_doorsensor, false) ? F("Yes") : F("No");
    }

    uint32_t basicLockConfigAclPrefs[16];
    _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
    uint32_t advancedLockConfigAclPrefs[25];
    _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
    response += F("\n\n------------ NUKI LOCK ACL ------------");
    response += F("\nLock: ");
    response += ((int)aclPrefs[0]) ? F("Allowed") : F("Disallowed");
    response += F("\nUnlock: ");
    response += ((int)aclPrefs[1]) ? F("Allowed") : F("Disallowed");
    response += F("\nUnlatch: ");
    response += ((int)aclPrefs[2]) ? F("Allowed") : F("Disallowed");
    response += F("\nLock N Go: ");
    response += ((int)aclPrefs[3]) ? F("Allowed") : F("Disallowed");
    response += F("\nLock N Go Unlatch: ");
    response += ((int)aclPrefs[4]) ? F("Allowed") : F("Disallowed");
    response += F("\nFull Lock: ");
    response += ((int)aclPrefs[5]) ? F("Allowed") : F("Disallowed");
    response += F("\nFob Action 1: ");
    response += ((int)aclPrefs[6]) ? F("Allowed") : F("Disallowed");
    response += F("\nFob Action 2: ");
    response += ((int)aclPrefs[7]) ? F("Allowed") : F("Disallowed");
    response += F("\nFob Action 3: ");
    response += ((int)aclPrefs[8]) ? F("Allowed") : F("Disallowed");

    response += F("\n\n------------ NUKI LOCK CONFIG ACL ------------");
    response += F("\nName: ");
    response += ((int)basicLockConfigAclPrefs[0]) ? F("Allowed") : F("Disallowed");
    response += F("\nLatitude: ");
    response += ((int)basicLockConfigAclPrefs[1]) ? F("Allowed") : F("Disallowed");
    response += F("\nLongitude: ");
    response += ((int)basicLockConfigAclPrefs[2]) ? F("Allowed") : F("Disallowed");
    response += F("\nAuto Unlatch: ");
    response += ((int)basicLockConfigAclPrefs[3]) ? F("Allowed") : F("Disallowed");
    response += F("\nPairing enabled: ");
    response += ((int)basicLockConfigAclPrefs[4]) ? F("Allowed") : F("Disallowed");
    response += F("\nButton enabled: ");
    response += ((int)basicLockConfigAclPrefs[5]) ? F("Allowed") : F("Disallowed");
    response += F("\nLED flash enabled: ");
    response += ((int)basicLockConfigAclPrefs[6]) ? F("Allowed") : F("Disallowed");
    response += F("\nLED brightness: ");
    response += ((int)basicLockConfigAclPrefs[7]) ? F("Allowed") : F("Disallowed");
    response += F("\nTimezone offset: ");
    response += ((int)basicLockConfigAclPrefs[8]) ? F("Allowed") : F("Disallowed");
    response += F("\nDST mode: ");
    response += ((int)basicLockConfigAclPrefs[9]) ? F("Allowed") : F("Disallowed");
    response += F("\nFob Action 1: ");
    response += ((int)basicLockConfigAclPrefs[10]) ? F("Allowed") : F("Disallowed");
    response += F("\nFob Action 2: ");
    response += ((int)basicLockConfigAclPrefs[11]) ? F("Allowed") : F("Disallowed");
    response += F("\nFob Action 3: ");
    response += ((int)basicLockConfigAclPrefs[12]) ? F("Allowed") : F("Disallowed");
    response += F("\nSingle Lock: ");
    response += ((int)basicLockConfigAclPrefs[13]) ? F("Allowed") : F("Disallowed");
    response += F("\nAdvertising Mode: ");
    response += ((int)basicLockConfigAclPrefs[14]) ? F("Allowed") : F("Disallowed");
    response += F("\nTimezone ID: ");
    response += ((int)basicLockConfigAclPrefs[15]) ? F("Allowed") : F("Disallowed");

    response += F("\nUnlocked Position Offset Degrees: ");
    response += ((int)advancedLockConfigAclPrefs[0]) ? F("Allowed") : F("Disallowed");
    response += F("\nLocked Position Offset Degrees: ");
    response += ((int)advancedLockConfigAclPrefs[1]) ? F("Allowed") : F("Disallowed");
    response += F("\nSingle Locked Position Offset Degrees: ");
    response += ((int)advancedLockConfigAclPrefs[2]) ? F("Allowed") : F("Disallowed");
    response += F("\nUnlocked To Locked Transition Offset Degrees: ");
    response += ((int)advancedLockConfigAclPrefs[3]) ? F("Allowed") : F("Disallowed");
    response += F("\nLock n Go timeout: ");
    response += ((int)advancedLockConfigAclPrefs[4]) ? F("Allowed") : F("Disallowed");
    response += F("\nSingle button press action: ");
    response += ((int)advancedLockConfigAclPrefs[5]) ? F("Allowed") : F("Disallowed");
    response += F("\nDouble button press action: ");
    response += ((int)advancedLockConfigAclPrefs[6]) ? F("Allowed") : F("Disallowed");
    response += F("\nDetached cylinder: ");
    response += ((int)advancedLockConfigAclPrefs[7]) ? F("Allowed") : F("Disallowed");
    response += F("\nBattery type: ");
    response += ((int)advancedLockConfigAclPrefs[8]) ? F("Allowed") : F("Disallowed");
    response += F("\nAutomatic battery type detection: ");
    response += ((int)advancedLockConfigAclPrefs[9]) ? F("Allowed") : F("Disallowed");
    response += F("\nUnlatch duration: ");
    response += ((int)advancedLockConfigAclPrefs[10]) ? F("Allowed") : F("Disallowed");
    response += F("\nAuto lock timeout: ");
    response += ((int)advancedLockConfigAclPrefs[11]) ? F("Allowed") : F("Disallowed");
    response += F("\nAuto unlock disabled: ");
    response += ((int)advancedLockConfigAclPrefs[12]) ? F("Allowed") : F("Disallowed");
    response += F("\nNightmode enabled: ");
    response += ((int)advancedLockConfigAclPrefs[13]) ? F("Allowed") : F("Disallowed");
    response += F("\nNightmode start time: ");
    response += ((int)advancedLockConfigAclPrefs[14]) ? F("Allowed") : F("Disallowed");
    response += F("\nNightmode end time: ");
    response += ((int)advancedLockConfigAclPrefs[15]) ? F("Allowed") : F("Disallowed");
    response += F("\nNightmode auto lock enabled: ");
    response += ((int)advancedLockConfigAclPrefs[16]) ? F("Allowed") : F("Disallowed");
    response += F("\nNightmode auto unlock disabled: ");
    response += ((int)advancedLockConfigAclPrefs[17]) ? F("Allowed") : F("Disallowed");
    response += F("\nNightmode immediate lock on start: ");
    response += ((int)advancedLockConfigAclPrefs[18]) ? F("Allowed") : F("Disallowed");
    response += F("\nAuto lock enabled: ");
    response += ((int)advancedLockConfigAclPrefs[19]) ? F("Allowed") : F("Disallowed");
    response += F("\nImmediate auto lock enabled: ");
    response += ((int)advancedLockConfigAclPrefs[20]) ? F("Allowed") : F("Disallowed");
    response += F("\nAuto update enabled: ");
    response += ((int)advancedLockConfigAclPrefs[21]) ? F("Allowed") : F("Disallowed");
    response += F("\nReboot Nuki: ");
    response += ((int)advancedLockConfigAclPrefs[22]) ? F("Allowed") : F("Disallowed");
    response += F("\nMotor speed: ");
    response += ((int)advancedLockConfigAclPrefs[23]) ? F("Allowed") : F("Disallowed");
    response += F("\nEnable slow speed during nightmode: ");
    response += ((int)advancedLockConfigAclPrefs[24]) ? F("Allowed") : F("Disallowed");

    if (_preferences->getBool(preference_show_secrets))
    {
        char tmp[16];
        unsigned char currentBleAddress[6];
        unsigned char authorizationId[4] = {0x00};
        unsigned char secretKeyK[32] = {0x00};

        Preferences nukiBlePref;
        nukiBlePref.begin("NukiBridge", false);
        nukiBlePref.getBytes("bleAddress", currentBleAddress, 6);
        nukiBlePref.getBytes("secretKeyK", secretKeyK, 32);
        nukiBlePref.getBytes("authorizationId", authorizationId, 4);
        nukiBlePref.end();

        response += F("\n\n------------ NUKI LOCK PAIRING ------------");
        response += F("\nBLE Address: ");
        for (int i = 0; i < 6; i++)
        {
            sprintf(tmp, "%02x", currentBleAddress[i]);
            response += tmp;
        }

        response += F("\nSecretKeyK: ");
        for (int i = 0; i < 32; i++)
        {
            sprintf(tmp, "%02x", secretKeyK[i]);
            response += tmp;
        }

        response += F("\nAuthorizationId: ");
        for (int i = 0; i < 4; i++)
        {
            sprintf(tmp, "%02x", authorizationId[i]);
            response += tmp;
        }

        uint32_t authorizationIdInt = authorizationId[0] + 256U * authorizationId[1] + 65536U * authorizationId[2] + 16777216U * authorizationId[3];
        response += F("\nAuthorizationId (UINT32_T): ");
        response += String(authorizationIdInt);
    }

    response += F("</pre></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildHtmlHeader(String &response, const String &additionalHeader)
{
    response += F("<html><head>");
    response += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");

    if (!additionalHeader.isEmpty())
    {
        response += additionalHeader;
    }

    response += F("<link rel='stylesheet' href='/style.css'>");
    response += F("<title>Nuki Bridge</title></head><body>");
}

void WebCfgServer::buildStatusHtml(WebServer *server)
{
    JsonDocument json;
    String jsonStr;
    bool APIDone = false;
    bool HARDone = false;
    bool lockDone = false;

    json[F("stop")] = 0;

    // MQTT
    if (_network->networkServicesState() == NetworkServiceState::OK)
    {
        json[F("APIState")] = F("Yes");
        APIDone = true;
        json[F("HARState")] = F("Yes");
        HARDone = true;
    }
    else if (_network->networkServicesState() == NetworkServiceState::ERROR_HAR_CLIENT)
    {
        json[F("APIState")] = F("Yes");
        APIDone = true;
        json[F("HARState")] = F("No");
    }
    else if (_network->networkServicesState() == NetworkServiceState::ERROR_REST_API_SERVER)
    {
        json[F("APIState")] = F("No");
        json[F("HARState")] = F("Yes");
        HARDone = true;
    }
    else if (_network->networkServicesState() == NetworkServiceState::ERROR_BOTH)
    {
        json[F("APIState")] = F("No");
        json[F("HARState")] = F("No");
    }

    // Nuki Lock
    if (_nuki != nullptr)
    {
        char lockStateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockStateArr);

        String paired = (_nuki->isPaired()
                             ? ("Yes (BLE Address " + _nuki->getBleAddress().toString() + ")").c_str()
                             : "No");
        json[F("lockPaired")] = paired;
        json[F("lockState")] = String(lockStateArr);

        if (_nuki->isPaired())
        {
            json[F("lockPin")] = pinStateToString((NukiPinState)_preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured));
            if (strcmp(lockStateArr, "undefined") != 0)
                lockDone = true;
        }
        else
        {
            json[F("lockPin")] = F("Not Paired");
        }
    }
    else
    {
        lockDone = true;
    }

    if (APIDone && lockDone && HARDone)
    {
        json[F("stop")] = 1;
    }

    serializeJson(json, jsonStr);
    return server->send(200, F("application/json"), jsonStr.c_str());
}

void WebCfgServer::appendNavigationMenuEntry(String &response, const char *title, const char *targetPath, const char *warningMessage)
{
    response += F("<a class=\"naventry\" href=\"");
    response += targetPath;
    response += F("\"><li>");
    response += title;

    if (*warningMessage) 
    {
        response += F("<span>");
        response += warningMessage;
        response += F("</span>");
    }

    response += F("</li></a>");
}

void WebCfgServer::appendParameterRow(String &response, const char *description, const char *value, const char *link, const char *id)
{
    response += F("<tr><td>");
    response += description;
    response += F("</td>");

    if (*id)
    {
        response += F("<td id=\"");
        response += id;
        response += F("\">");
    }
    else
    {
        response += F("<td>");
    }

    if (*link)
    {
        response += F("<a href=\"");
        response += link;
        response += F("\"> ");
        response += value;
        response += F("</a>");
    }
    else
    {
        response += value;
    }

    response += F("</td></tr>");
}

void WebCfgServer::appendInputFieldRow(String &response,
                                       const char *token,
                                       const char *description,
                                       const char *value,
                                       const size_t &maxLength,
                                       const char *args,
                                       const bool &isPassword,
                                       const bool &showLengthRestriction)
{
    char maxLengthStr[20];
    itoa(maxLength, maxLengthStr, 10);

    response += F("<tr><td>");
    response += description;

    if (showLengthRestriction)
    {
        response += F(" (Max. ");
        response += maxLengthStr;
        response += F(" characters)");
    }

    response += F("</td><td>");
    response += F("<input type=");
    response += isPassword ? F("\"password\"") : F("\"text\"");

    if (strcmp(args, "") != 0)
    {
        response += F(" ");
        response += args;
    }

    if (strcmp(value, "") != 0)
    {
        response += F(" value=\"");
        response += value;
        response += F("\"");
    }

    response += F(" name=\"");
    response += token;
    response += F("\" size=\"25\" maxlength=\"");
    response += maxLengthStr;
    response += F("\"/></td></tr>");
}

void WebCfgServer::appendInputFieldRow(String &response,
                                       const char *token,
                                       const char *description,
                                       const int value,
                                       size_t maxLength,
                                       const char *args)
{
    char valueStr[20];
    itoa(value, valueStr, 10);
    appendInputFieldRow(response, token, description, valueStr, maxLength, args);
}

void WebCfgServer::appendDropDownRow(String &response,
                                     const char *token,
                                     const char *description,
                                     const String preselectedValue,
                                     const std::vector<std::pair<String, String>> &options,
                                     const char *className,
                                     const char *id,
                                     const char *onChange)
{
    response += F("<tr><td>");
    response += description;
    response += F("</td><td>");

    response += F("<select ");
    if (strlen(className) > 0)
    {
        response += F("class=\"");
        response += className;
        response += F("\" ");
    }
    if (strlen(id) > 0)
    {
        response += F("id=\"");
        response += id;
        response += F("\" ");
    }
    if (strlen(onChange) > 0)
    {
        response += F("onchange=\"");
        response += onChange;
        response += F("\" ");
    }
    response += F("name=\"");
    response += token;
    response += F("\">");

    for (const auto &option : options)
    {
        response += F("<option ");
        if (option.first == preselectedValue)
        {
            response += F("selected=\"selected\" ");
        }
        response += F("value=\"");
        response += option.first;
        response += F("\">");
        response += option.second;
        response += F("</option>");
    }

    response += F("</select>");
    response += F("</td></tr>");
}

void WebCfgServer::appendTextareaRow(String &response,
                                     const char *token,
                                     const char *description,
                                     const char *value,
                                     const size_t &maxLength,
                                     const bool &enabled,
                                     const bool &showLengthRestriction)
{
    response += F("<tr><td>");
    response += description;

    if (showLengthRestriction)
    {
        response += F(" (Max. ");
        response += String(maxLength);
        response += F(" characters)");
    }

    response += F("</td><td> <textarea ");
    if (!enabled)
    {
        response += F("disabled ");
    }

    response += F("name=\"");
    response += token;
    response += F("\" maxlength=\"");
    response += String(maxLength);
    response += F("\">");
    response += value;
    response += F("</textarea></td></tr>");
}

void WebCfgServer::appendCheckBoxRow(String &response,
                                     const char *token,
                                     const char *description,
                                     const bool value,
                                     const char *className,
                                     const char *id)
{
    response += F("<tr><td>");
    response += description;
    response += F("</td><td>");

    // Hidden field to force "0" if unchecked
    response += F("<input type=\"hidden\" name=\"");
    response += token;
    response += F("\" value=\"0\"/>");

    // Begin checkbox input
    response += F("<input type=\"checkbox\" name=\"");
    response += token;
    response += F("\"");

    // Optional class
    if (strlen(className) > 0)
    {
        response += F(" class=\"");
        response += className;
        response += F("\"");
    }

    // Optional id
    if (strlen(id) > 0)
    {
        response += F(" id=\"");
        response += id;
        response += F("\"");
    }

    // Value and checked state
    response += F(" value=\"1\"");
    if (value)
    {
        response += F(" checked=\"checked\"");
    }

    response += F("/></td></tr>");
}

const std::vector<std::pair<String, String>> WebCfgServer::getNetworkDetectionOptions() const
{
    std::vector<std::pair<String, String>> options;

    options.push_back(std::make_pair("1", "Wi-Fi"));
    options.push_back(std::make_pair("2", "LAN module"));

    return options;
}

void WebCfgServer::sendCss(WebServer *server)
{
    // Setze den Cache-Control Header
    server->sendHeader(F("Cache-Control"), "public, max-age=3600");

    // Setze den Content-Type auf text/css
    server->setContentLength(CONTENT_LENGTH_UNKNOWN); // Länge muss nicht im Voraus bekannt sein
    server->send(200, F("text/css"), css);            // Antwortstatus 200 und Content-Type "text/css"
}

String WebCfgServer::generateConfirmCode()
{
    int code = random(1000, 9999);
    return String(code);
}

size_t WebCfgServer::estimateHtmlSize(
    int checkboxCount,
    int inputFieldCount,
    int dropdownCount,
    int dropdownOptionCountTotal,
    int textareaCount,
    int parameterRowCount,
    int buttonCount,
    int navigationMenuCount,
    int extraContentBytes)
{
    const int checkboxSize = 320;
    const int inputFieldSize = 368;
    const int dropdownBaseSize = 192;
    const int dropdownOptionSize = 96;
    const int textareaSize = 384;
    const int paramRowSize = 192;
    const int buttonSize = 160;
    const int navigationMenuSize = 160;
    const int footerSize = 160;
    const int headerSize = 368;
    const int extraStatic = 160; // additional <input hidden> / <td> / <form> etc.

    size_t total = headerSize + extraStatic + extraContentBytes + footerSize;
    total += checkboxCount * checkboxSize;
    total += inputFieldCount * inputFieldSize;
    total += dropdownCount * dropdownBaseSize + dropdownOptionCountTotal * dropdownOptionSize;
    total += textareaCount * textareaSize;
    total += parameterRowCount * paramRowSize;
    total += buttonCount * buttonSize;
    total += navigationMenuCount * navigationMenuSize;

    return total;
}

#define HANDLE_STRING_PREF_ARG(KEYNAME, PREFNAME, CONFIG_CHANGED) \
    else if (key == KEYNAME)                                      \
    {                                                             \
        if (_preferences->getString(PREFNAME, "") != value)       \
        {                                                         \
            _preferences->putString(PREFNAME, value);             \
            Log->print(F("[DEBUG] Setting changed: "));           \
            Log->println(KEYNAME);                                \
            if (CONFIG_CHANGED)                                   \
            {                                                     \
                configChanged = true;                             \
            }                                                     \
        }                                                         \
    }
#define HANDLE_BOOL_PREF_ARG(KEYNAME, PREFNAME, CONFIG_CHANGED)       \
    else if (key == KEYNAME)                                          \
    {                                                                 \
        if (_preferences->getBool(PREFNAME, false) != (value == "1")) \
        {                                                             \
            _preferences->putBool(PREFNAME, (value == "1"));          \
            Log->print(F("[DEBUG] Setting changed: "));               \
            Log->println(KEYNAME);                                    \
            if (CONFIG_CHANGED)                                       \
            {                                                         \
                configChanged = true;                                 \
            }                                                         \
        }                                                             \
    }

#define HANDLE_INT_PREF_ARG(KEYNAME, PREFNAME, CONFIG_CHANGED)  \
    else if (key == KEYNAME)                                    \
    {                                                           \
        if (_preferences->getInt(PREFNAME, 0) != value.toInt()) \
        {                                                       \
            _preferences->putInt(PREFNAME, value.toInt());      \
            Log->print(F("[DEBUG] Setting changed: "));         \
            Log->println(KEYNAME);                              \
            if (CONFIG_CHANGED)                                 \
            {                                                   \
                configChanged = true;                           \
            }                                                   \
        }                                                       \
    }

bool WebCfgServer::processArgs(WebServer *server, String &message)
{
    bool configChanged = false;
    bool aclLvlChanged = false;
    bool clearHARCredentials = false;
    bool clearCredentials = false;
    bool manPairLck = false;
    bool networkReconfigure = false;
    bool clearSession = false;

    unsigned char currentBleAddress[6];
    unsigned char authorizationId[4] = {0x00};
    unsigned char secretKeyK[32] = {0x00};
    unsigned char pincode[2] = {0x00};

    uint32_t aclPrefs[17] = {0};
    uint32_t basicLockConfigAclPrefs[16] = {0};
    uint32_t advancedLockConfigAclPrefs[25] = {0};

    String pass1 = "";
    String pass2 = "";

    int paramCount = server->args();

    for (int i = 0; i < paramCount; ++i)
    {
        String key = server->argName(i);
        String value = server->arg(i);

        if (key == "HARHOST")
        {
            if (_preferences->getString(preference_har_address, "") != value)
            {
                _preferences->putString(preference_har_address, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        HANDLE_INT_PREF_ARG("HARPORT", preference_har_port, true)
        else if (key == "HARUSER")
        {
            if (value == "#")
            {
                clearHARCredentials = true;
            }
            else
            {
                if (_preferences->getString(preference_har_user, "") != value)
                {
                    _preferences->putString(preference_har_user, value);
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        HANDLE_STRING_PREF_ARG("HARPASS", preference_har_password, true)
        else if (key == "HARENA")
        {
            if (_preferences->getBool(preference_har_enabled, false) != (value == "1"))
            {
                _network->disableHAR();
                _preferences->putBool(preference_har_enabled, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "HARMODE")
        {
            if (_preferences->getInt(preference_har_mode, 0) != value.toInt())
            {
                if (value.toInt() >= 0 && value.toInt() <= 1)
                {
                    Log->setLevel((Logger::msgtype)value.toInt());
                }
                _preferences->putInt(preference_har_mode, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        HANDLE_STRING_PREF_ARG("HARRESTMODE", preference_har_rest_mode, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_STAT, preference_har_key_state, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_REMACCSTAT, preference_har_key_remote_access_state, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_REMACCSTAT, preference_har_param_remote_access_state, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_WFRSSI, preference_har_key_wifi_rssi, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_WFRSSI, preference_har_param_wifi_rssi, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_UPTM, preference_har_key_uptime, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_UPTM, preference_har_param_uptime, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_RSTFW, preference_har_key_restart_reason_fw, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_RSTFW, preference_har_param_restart_reason_fw, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_RSTESP, preference_har_key_restart_reason_esp, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_RSTESP, preference_har_param_restart_reason_esp, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_NBVER, preference_har_key_info_nuki_bridge_version, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_NBVER, preference_har_param_info_nuki_bridge_version, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_NBBUILD, preference_har_key_info_nuki_bridge_build, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_NBBUILD, preference_har_param_info_nuki_bridge_build, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_FREEHP, preference_har_key_freeheap, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_FREEHP, preference_har_param_freeheap, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_BLEADDR, preference_har_key_ble_address, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_BLEADDR, preference_har_param_ble_address, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_BLESTR, preference_har_key_ble_strength, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_BLESTR, preference_har_param_ble_strength, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_BLERSSI, preference_har_key_ble_rssi, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_BLERSSI, preference_har_param_ble_rssi, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_LCKSTAT, preference_har_key_lock_state, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_LCKSTAT, preference_har_param_lock_state, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_LCKNGOSTAT, preference_har_key_lockngo_state, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_LCKNGOSTAT, preference_har_param_lockngo_state, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_LCKTRIG, preference_har_key_lock_trigger, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_LCKTRIG, preference_har_param_lock_trigger, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_LCKNMOD, preference_har_key_lock_night_mode, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_LCKNMOD, preference_har_param_lock_night_mode, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_LCKCMPLSTAT, preference_har_key_lock_completionStatus, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_LCKCMPLSTAT, preference_har_param_lock_completionStatus, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_DOORSTAT, preference_har_key_doorsensor_state, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_DOORSTAT, preference_har_param_doorsensor_state, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_DOORSCRIT, preference_har_key_doorsensor_critical, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_DOORSCRIT, preference_har_param_doorsensor_critical, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_KPCRIT, preference_har_key_keypad_critical, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_KPCRIT, preference_har_param_keypad_critical, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_LCKBATCRIT, preference_har_key_lock_battery_critical, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_LCKBATCRIT, preference_har_param_lock_battery_critical, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_LCKBATLVL, preference_har_key_lock_battery_level, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_LCKBATLVL, preference_har_param_lock_battery_level, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_LCKBATCHRG, preference_har_key_lock_battery_charging, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_LCKBATCHRG, preference_har_param_lock_battery_charging, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_BATVOLT, preference_har_key_battery_voltage, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_BATVOLT, preference_har_param_battery_voltage, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_BATDRAIN, preference_har_key_battery_drain, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_BATDRAIN, preference_har_param_battery_drain, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_BATMAXTURNCUR, preference_har_key_battery_max_turn_current, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_BATMAXTURNCUR, preference_har_param_battery_max_turn_current, true)
        HANDLE_STRING_PREF_ARG("KEY_" TOKEN_SUFFIX_BATLCKDIST, preference_har_key_battery_lock_distance, true)
        HANDLE_STRING_PREF_ARG("PARAM_" TOKEN_SUFFIX_BATLCKDIST, preference_har_param_battery_lock_distance, true)
        else if (key == "UPTIME")
        {
            if (_preferences->getBool(preference_update_time, false) != (value == "1"))
            {
                _preferences->putBool(preference_update_time, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        HANDLE_STRING_PREF_ARG("TIMESRV", preference_time_server, true)
        else if (key == "NWHW")
        {
            if (_preferences->getInt(preference_network_hardware, 0) != value.toInt())
            {
                if (value.toInt() > 1)
                {
                    networkReconfigure = true;
                    if (value.toInt() != 11)
                    {
                        _preferences->putInt(preference_network_custom_phy, 0);
                    }
                }
                _preferences->putInt(preference_network_hardware, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTPHY")
        {
            if (_preferences->getInt(preference_network_custom_phy, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_phy, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTADDR")
        {
            if (_preferences->getInt(preference_network_custom_addr, -1) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_addr, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTIRQ")
        {
            if (_preferences->getInt(preference_network_custom_irq, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_irq, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTRST")
        {
            if (_preferences->getInt(preference_network_custom_rst, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_rst, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTCS")
        {
            if (_preferences->getInt(preference_network_custom_cs, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_cs, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTSCK")
        {
            if (_preferences->getInt(preference_network_custom_sck, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_sck, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTMISO")
        {
            if (_preferences->getInt(preference_network_custom_miso, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_miso, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTMOSI")
        {
            if (_preferences->getInt(preference_network_custom_mosi, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_mosi, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTPWR")
        {
            if (_preferences->getInt(preference_network_custom_pwr, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_pwr, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTMDIO")
        {
            if (_preferences->getInt(preference_network_custom_mdio, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_mdio, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTMDC")
        {
            if (_preferences->getInt(preference_network_custom_mdc, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_mdc, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NWCUSTCLK")
        {
            if (_preferences->getInt(preference_network_custom_clk, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_clk, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "RSSI")
        {
            if (_preferences->getInt(preference_rssi_send_interval, 60) != value.toInt())
            {
                _preferences->putInt(preference_rssi_send_interval, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "CREDLFTM")
        {
            if (_preferences->getInt(preference_cred_session_lifetime, 3600) != value.toInt())
            {
                _preferences->putInt(preference_cred_session_lifetime, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
                clearSession = true;
            }
        }
        else if (key == "CREDLFTMRMBR")
        {
            if (_preferences->getInt(preference_cred_session_lifetime_remember, 720) != value.toInt())
            {
                _preferences->putInt(preference_cred_session_lifetime_remember, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
                clearSession = true;
            }
        }

        else if (key == "LOGBCKENA")
        {
            if (_preferences->getBool(preference_log_backup_enabled, false) != (value == "1"))
            {
                Log->disableBackup();
                _preferences->putBool(preference_log_backup_enabled, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "LOGMSGLEN")
        {
            if (_preferences->getInt(preference_log_max_msg_len, 1) != value.toInt())
            {
                _preferences->putInt(preference_log_max_msg_len, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "LOGMAXSIZE")
        {
            if (_preferences->getInt(preference_log_max_file_size, 256) != value.toInt())
            {
                _preferences->putInt(preference_log_max_file_size, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                // configChanged = true;
            }
        }
        HANDLE_STRING_PREF_ARG("LOGBCKSRV", preference_log_backup_ftp_server, true)
        HANDLE_STRING_PREF_ARG("LOGBCKDIR", preference_log_backup_ftp_dir, true)
        HANDLE_STRING_PREF_ARG("LOGBCKUSR", preference_log_backup_ftp_user, true)
        HANDLE_STRING_PREF_ARG("LOGBCKPWD", preference_log_backup_ftp_pwd, true)
        else if (key == "LOGLEVEL")
        {
            if (_preferences->getInt(preference_log_level, 0) != value.toInt())
            {
                if (value.toInt() >= 0 && value.toInt() <= 5)
                {
                    Log->setLevel((Logger::msgtype)value.toInt());
                }
                _preferences->putInt(preference_log_level, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "APIENA")
        {
            if (_preferences->getBool(preference_api_enabled, false) != (value == "1"))
            {
                _network->disableAPI();
                _preferences->putBool(preference_api_enabled, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "APIPORT")
        {
            if (_preferences->getInt(preference_api_port, 0) != value.toInt())
            {
                _preferences->putInt(preference_api_port, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "HOSTNAME")
        {
            if (_preferences->getString(preference_hostname, "") != value && value != "nukirestbridge")
            {
                _preferences->putString(preference_hostname, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "NETTIMEOUT")
        {
            if (_preferences->getInt(preference_network_timeout, 60) != value.toInt())
            {
                _preferences->putInt(preference_network_timeout, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "FINDBESTRSSI")
        {
            if (_preferences->getBool(preference_find_best_rssi, false) != (value == "1"))
            {
                _preferences->putBool(preference_find_best_rssi, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "RSTDISC")
        {
            if (_preferences->getBool(preference_restart_on_disconnect, false) != (value == "1"))
            {
                _preferences->putBool(preference_restart_on_disconnect, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "DHCPENA")
        {
            if (_preferences->getBool(preference_ip_dhcp_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_ip_dhcp_enabled, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "IPADDR")
        {
            if (_preferences->getString(preference_ip_address, "") != value)
            {
                _preferences->putString(preference_ip_address, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "IPSUB")
        {
            if (_preferences->getString(preference_ip_subnet, "") != value)
            {
                _preferences->putString(preference_ip_subnet, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "IPGTW")
        {
            if (_preferences->getString(preference_ip_gateway, "") != value)
            {
                _preferences->putString(preference_ip_gateway, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "DNSSRV")
        {
            if (_preferences->getString(preference_ip_dns_server, "") != value)
            {
                _preferences->putString(preference_ip_dns_server, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "LSTINT")
        {
            if (_preferences->getInt(preference_query_interval_lockstate, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_lockstate, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "CFGINT")
        {
            if (_preferences->getInt(preference_query_interval_configuration, 3600) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_configuration, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "BATINT")
        {
            if (_preferences->getInt(preference_query_interval_battery, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_battery, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "KPINT")
        {
            if (_preferences->getInt(preference_query_interval_keypad, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_keypad, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "NRTRY")
        {
            if (_preferences->getInt(preference_command_nr_of_retries, 3) != value.toInt())
            {
                _preferences->putInt(preference_command_nr_of_retries, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "TRYDLY")
        {
            if (_preferences->getInt(preference_command_retry_delay, 100) != value.toInt())
            {
                _preferences->putInt(preference_command_retry_delay, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "TXPWR")
        {
#if defined(CONFIG_IDF_TARGET_ESP32)
            if (value.toInt() >= -12 && value.toInt() <= 9)
#else
            if (value.toInt() >= -12 && value.toInt() <= 20)
#endif
            {
                if (_preferences->getInt(preference_ble_tx_power, 9) != value.toInt())
                {
                    _preferences->putInt(preference_ble_tx_power, value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    // configChanged = true;
                }
            }
        }
        else if (key == "RSBC")
        {
            if (_preferences->getInt(preference_restart_ble_beacon_lost, 60) != value.toInt())
            {
                _preferences->putInt(preference_restart_ble_beacon_lost, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "TSKNTWK")
        {
            if (value.toInt() > 12287 && value.toInt() < 65537)
            {
                if (_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_task_size_network, value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if (key == "TSKNUKI")
        {
            if (value.toInt() > 8191 && value.toInt() < 65537)
            {
                if (_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_task_size_nuki, value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if (key == "ALMAX")
        {
            if (value.toInt() > 0 && value.toInt() < 101)
            {
                if (_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG) != value.toInt())
                {
                    _preferences->putInt(preference_authlog_max_entries, value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    // configChanged = true;
                }
            }
        }
        else if (key == "KPMAX")
        {
            if (value.toInt() > 0 && value.toInt() < 201)
            {
                if (_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD) != value.toInt())
                {
                    _preferences->putInt(preference_keypad_max_entries, value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    // configChanged = true;
                }
            }
        }
        else if (key == "TCMAX")
        {
            if (value.toInt() > 0 && value.toInt() < 101)
            {
                if (_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL) != value.toInt())
                {
                    _preferences->putInt(preference_timecontrol_max_entries, value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    // configChanged = true;
                }
            }
        }
        else if (key == "AUTHMAX")
        {
            if (value.toInt() > 0 && value.toInt() < 101)
            {
                if (_preferences->getInt(preference_auth_max_entries, MAX_AUTH) != value.toInt())
                {
                    _preferences->putInt(preference_auth_max_entries, value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    // configChanged = true;
                }
            }
        }
        else if (key == "BUFFSIZE")
        {
            if (value.toInt() > 4095 && value.toInt() < 65537)
            {
                if (_preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_buffer_size, value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if (key == "BTLPRST")
        {
            if (_preferences->getBool(preference_enable_bootloop_reset, false) != (value == "1"))
            {
                _preferences->putBool(preference_enable_bootloop_reset, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "DISNTWNOCON")
        {
            if (_preferences->getBool(preference_disable_network_not_connected, false) != (value == "1"))
            {
                _preferences->putBool(preference_disable_network_not_connected, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "SHOWSECRETS")
        {
            if (_preferences->getBool(preference_show_secrets, false) != (value == "1"))
            {
                _preferences->putBool(preference_show_secrets, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                // configChanged = true;
            }
        }
        else if (key == "DBGCONN")
        {
            if (_preferences->getBool(preference_debug_connect, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_connect, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "DBGCOMMU")
        {
            if (_preferences->getBool(preference_debug_communication, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_communication, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "DBGHEAP")
        {
            if (_preferences->getBool(preference_send_debug_info, false) != (value == "1"))
            {
                _preferences->putBool(preference_send_debug_info, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "DBGREAD")
        {
            if (_preferences->getBool(preference_debug_readable_data, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_readable_data, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "DBGHEX")
        {
            if (_preferences->getBool(preference_debug_hex_data, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_hex_data, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "DBGCOMM")
        {
            if (_preferences->getBool(preference_debug_command, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_command, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "LCKFORCEID")
        {
            if (_preferences->getBool(preference_lock_force_id, false) != (value == "1"))
            {
                _preferences->putBool(preference_lock_force_id, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
            }
        }
        else if (key == "LCKFORCEKP")
        {
            if (_preferences->getBool(preference_lock_force_keypad, false) != (value == "1"))
            {
                _preferences->putBool(preference_lock_force_keypad, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
            }
        }
        else if (key == "LCKFORCEDS")
        {
            if (_preferences->getBool(preference_lock_force_doorsensor, false) != (value == "1"))
            {
                _preferences->putBool(preference_lock_force_doorsensor, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
            }
        }
        else if (key == "ACLLVLCHANGED")
        {
            aclLvlChanged = true;
        }
        else if (key == "CREDDIGEST")
        {
            if (_preferences->getInt(preference_http_auth_type, 0) != value.toInt())
            {
                _preferences->putInt(preference_http_auth_type, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
                clearSession = true;
            }
        }
        else if (key == "CREDTRUSTPROXY")
        {
            if (value != "*")
            {
                if (_preferences->getString(preference_bypass_proxy, "") != value)
                {
                    _preferences->putString(preference_bypass_proxy, value);
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                    clearSession = true;
                }
            }
        }
        else if (key == "ACLLCKLCK")
        {
            aclPrefs[0] = ((value == "1") ? 1 : 0);
        }
        else if (key == "ACLLCKUNLCK")
        {
            aclPrefs[1] = ((value == "1") ? 1 : 0);
        }
        else if (key == "ACLLCKUNLTCH")
        {
            aclPrefs[2] = ((value == "1") ? 1 : 0);
        }
        else if (key == "ACLLCKLNG")
        {
            aclPrefs[3] = ((value == "1") ? 1 : 0);
        }
        else if (key == "ACLLCKLNGU")
        {
            aclPrefs[4] = ((value == "1") ? 1 : 0);
        }
        else if (key == "ACLLCKFLLCK")
        {
            aclPrefs[5] = ((value == "1") ? 1 : 0);
        }
        else if (key == "ACLLCKFOB1")
        {
            aclPrefs[6] = ((value == "1") ? 1 : 0);
        }
        else if (key == "ACLLCKFOB2")
        {
            aclPrefs[7] = ((value == "1") ? 1 : 0);
        }
        else if (key == "ACLLCKFOB3")
        {
            aclPrefs[8] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKNAME")
        {
            basicLockConfigAclPrefs[0] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKLAT")
        {
            basicLockConfigAclPrefs[1] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKLONG")
        {
            basicLockConfigAclPrefs[2] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKAUNL")
        {
            basicLockConfigAclPrefs[3] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKPRENA")
        {
            basicLockConfigAclPrefs[4] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKBTENA")
        {
            basicLockConfigAclPrefs[5] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKLEDENA")
        {
            basicLockConfigAclPrefs[6] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKLEDBR")
        {
            basicLockConfigAclPrefs[7] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKTZOFF")
        {
            basicLockConfigAclPrefs[8] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKDSTM")
        {
            basicLockConfigAclPrefs[9] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKFOB1")
        {
            basicLockConfigAclPrefs[10] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKFOB2")
        {
            basicLockConfigAclPrefs[11] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKFOB3")
        {
            basicLockConfigAclPrefs[12] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKSGLLCK")
        {
            basicLockConfigAclPrefs[13] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKADVM")
        {
            basicLockConfigAclPrefs[14] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKTZID")
        {
            basicLockConfigAclPrefs[15] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKUPOD")
        {
            advancedLockConfigAclPrefs[0] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKLPOD")
        {
            advancedLockConfigAclPrefs[1] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKSLPOD")
        {
            advancedLockConfigAclPrefs[2] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKUTLTOD")
        {
            advancedLockConfigAclPrefs[3] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKLNGT")
        {
            advancedLockConfigAclPrefs[4] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKSBPA")
        {
            advancedLockConfigAclPrefs[5] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKDBPA")
        {
            advancedLockConfigAclPrefs[6] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKDC")
        {
            advancedLockConfigAclPrefs[7] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKBATT")
        {
            advancedLockConfigAclPrefs[8] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKABTD")
        {
            advancedLockConfigAclPrefs[9] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKUNLD")
        {
            advancedLockConfigAclPrefs[10] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKALT")
        {
            advancedLockConfigAclPrefs[11] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKAUNLD")
        {
            advancedLockConfigAclPrefs[12] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKNMENA")
        {
            advancedLockConfigAclPrefs[13] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKNMST")
        {
            advancedLockConfigAclPrefs[14] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKNMET")
        {
            advancedLockConfigAclPrefs[15] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKNMALENA")
        {
            advancedLockConfigAclPrefs[16] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKNMAULD")
        {
            advancedLockConfigAclPrefs[17] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKNMLOS")
        {
            advancedLockConfigAclPrefs[18] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKALENA")
        {
            advancedLockConfigAclPrefs[19] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKIALENA")
        {
            advancedLockConfigAclPrefs[20] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKAUENA")
        {
            advancedLockConfigAclPrefs[21] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKRBTNUKI")
        {
            advancedLockConfigAclPrefs[22] = ((value == "1") ? 1 : 0);
        }
        else if (key == "CONFLCKMTRSPD")
        {
            advancedLockConfigAclPrefs[23] = ((value == "1") ? 1 : 0);
        }
        else if (key == "LOCKENA")
        {
            if (_preferences->getBool(preference_lock_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_lock_enabled, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "CONNMODE")
        {
            if (_preferences->getBool(preference_connect_mode, true) != (value == "1"))
            {
                _preferences->putBool(preference_connect_mode, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "CREDUSER")
        {
            if (value == "#")
            {
                clearCredentials = true;
            }
            else
            {
                if (_preferences->getString(preference_cred_user, "") != value)
                {
                    _preferences->putString(preference_cred_user, value);
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                    clearSession = true;
                }
            }
        }
        else if (key == "CREDPASS")
        {
            pass1 = value;
        }
        else if (key == "CREDPASSRE")
        {
            pass2 = value;
        }
        else if (key == "CREDADMIN")
        {
            if (value != "*")
            {
                if (_preferences->getString(preference_admin_secret, "") != value)
                {
                    _preferences->putString(preference_admin_secret, value);
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if (key == "NUKIPIN" && _nuki != nullptr)
        {
            if (value == "#")
            {

                message = "Nuki Lock PIN cleared";
                _nuki->setPin(0xffff);

                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
            else
            {

                if (_nuki->getPin() != value.toInt())
                {
                    message = "Nuki Lock PIN saved";
                    _nuki->setPin(value.toInt());
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if (key == "LCKMANPAIR" && (value == "1"))
        {
            manPairLck = true;
        }
        else if (key == "LCKBLEADDR")
        {
            if (value.length() == 12)
                for (int i = 0; i < value.length(); i += 2)
                {
                    currentBleAddress[(i / 2)] = std::stoi(value.substring(i, i + 2).c_str(), nullptr, 16);
                }
        }
        else if (key == "LCKSECRETK")
        {
            if (value.length() == 64)
                for (int i = 0; i < value.length(); i += 2)
                {
                    secretKeyK[(i / 2)] = std::stoi(value.substring(i, i + 2).c_str(), nullptr, 16);
                }
        }
        else if (key == "LCKAUTHID")
        {
            if (value.length() == 8)
                for (int i = 0; i < value.length(); i += 2)
                {
                    authorizationId[(i / 2)] = std::stoi(value.substring(i, i + 2).c_str(), nullptr, 16);
                }
        }

        // TODO: further else if for other parameters, analogous to above...
    }

    if (networkReconfigure)
    {
        _preferences->putBool(preference_ntw_reconfigure, true);
    }

    if (manPairLck)
    {
        Log->println(F("[DEBUG] Changing lock pairing"));
        Preferences nukiBlePref;
        nukiBlePref.begin("NukiBridge", false);
        nukiBlePref.putBytes("bleAddress", currentBleAddress, 6);
        nukiBlePref.putBytes("secretKeyK", secretKeyK, 32);
        nukiBlePref.putBytes("authorizationId", authorizationId, 4);
        nukiBlePref.putBytes("securityPinCode", pincode, 2);

        nukiBlePref.end();
        Log->print(F("[DEBUG] Setting changed: "));
        Log->println(F("Lock pairing data"));
        configChanged = true;
    }

    if (pass1 != "" && pass1 != "*" && pass1 == pass2)
    {
        if (_preferences->getString(preference_cred_password, "") != pass1)
        {
            _preferences->putString(preference_cred_password, pass1);
            Log->print(F("[DEBUG] Setting changed: "));
            Log->println(F("CREDPASS"));
            configChanged = true;
            clearSession = true;
        }
    }

    if (clearHARCredentials)
    {
        if (_preferences->getString(preference_har_user, "") != "")
        {
            _preferences->putString(preference_har_user, "");
            Log->print(F("[DEBUG] Setting changed: "));
            Log->println(F("HARUSER"));
            configChanged = true;
        }
        if (_preferences->getString(preference_har_password, "") != "")
        {
            _preferences->putString(preference_har_password, "");
            Log->print(F("[DEBUG] Setting changed: "));
            Log->println("HARPASS");
            configChanged = true;
        }
    }

    if (clearCredentials)
    {
        if (_preferences->getString(preference_cred_user, "") != "")
        {
            _preferences->putString(preference_cred_user, "");
            Log->print(F("[DEBUG] Setting changed: "));
            Log->println("CREDUSER");
            configChanged = true;
            clearSession = true;
        }
        if (_preferences->getString(preference_cred_password, "") != "")
        {
            _preferences->putString(preference_cred_password, "");
            Log->print(F("[DEBUG] Setting changed: "));
            Log->println("CREDPASS");
            configChanged = true;
            clearSession = true;
        }
    }

    // Save ACL changes (only if flag is set)
    if (aclLvlChanged)
    {
        uint32_t curAclPrefs[17] = {0};
        uint32_t curBasicLockConfigAclPrefs[16] = {0};
        uint32_t curAdvancedLockConfigAclPrefs[25] = {0};
        uint32_t curBasicOpenerConfigAclPrefs[14] = {0};
        uint32_t curAdvancedOpenerConfigAclPrefs[21] = {0};

        _preferences->getBytes(preference_acl, curAclPrefs, sizeof(curAclPrefs));
        _preferences->getBytes(preference_conf_lock_basic_acl, curBasicLockConfigAclPrefs, sizeof(curBasicLockConfigAclPrefs));
        _preferences->getBytes(preference_conf_lock_advanced_acl, curAdvancedLockConfigAclPrefs, sizeof(curAdvancedLockConfigAclPrefs));

        for (int i = 0; i < 17; ++i)
        {
            if (curAclPrefs[i] != aclPrefs[i])
            {
                _preferences->putBytes(preference_acl, (byte *)aclPrefs, sizeof(aclPrefs));
                Log->println("Setting changed: ACLPREFS");
                break;
            }
        }
        for (int i = 0; i < 16; ++i)
        {
            if (curBasicLockConfigAclPrefs[i] != basicLockConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_lock_basic_acl, (byte *)basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
                Log->println("Setting changed: ACLCONFBASICLOCK");
                break;
            }
        }
        for (int i = 0; i < 25; ++i)
        {
            if (curAdvancedLockConfigAclPrefs[i] != advancedLockConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_lock_advanced_acl, (byte *)advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
                Log->println("Setting changed: ACLCONFADVANCEDLOCK");
                break;
            }
        }
    }

    if (clearSession)
    {
        clearSessions();
    }

    if (configChanged)
    {
        message = F("Configuration saved, reboot required to apply");
        _rebootRequired = true;
    }
    else
    {
        message = F("Configuration saved.");
    }

    _network->readSettings();
    if (_nuki)
        _nuki->readSettings();

    return configChanged;
}

bool WebCfgServer::processLogin(WebServer *server)
{
    if (server->hasArg("username") && server->hasArg("password"))
    {
        String username = server->arg("username");
        String password = server->arg("password");

        if (!username.isEmpty() && !password.isEmpty())
        {
            if (username == _preferences->getString(preference_cred_user, "") &&
                password == _preferences->getString(preference_cred_password, ""))
            {
                char buffer[33];
                int64_t durationLength = 60 * 60 * _preferences->getInt(preference_cred_session_lifetime_remember, 720);

                for (int i = 0; i < 4; i++)
                {
                    snprintf(buffer + (i * 8), 9, "%08lx", (unsigned long)esp_random());
                }

                if (!server->hasArg("remember"))
                {
                    durationLength = _preferences->getInt(preference_cred_session_lifetime, 3600);
                }

                WiFiClient &client = server->client();

                server->sendHeader("Set-Cookie", "sessionId=" + String(buffer) + "; Max-Age=" + String(durationLength) + "; HttpOnly");

                struct timeval time;
                gettimeofday(&time, NULL);
                int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
                _httpSessions[String(buffer)] = time_us + (durationLength * 1000000L);
                saveSessions();

                return true;
            }
        }
    }
    return false;
}

bool WebCfgServer::processConnectionSettings(WebServer *server, String &message, int &netMode)
{
    bool result = false;
    int nwMode = _preferences->getInt(preference_network_hardware, 1); // Default: Wi-Fi

    // 1. Netzwerkmodus (LAN/WLAN) prüfen und ggf. speichern
    if (server->hasArg("NWHW"))
    {
        int newMode = server->arg("NWHW").toInt();
        if (newMode != nwMode)
        {
            _preferences->putInt(preference_network_hardware, newMode);
            nwMode = newMode;
            netMode = nwMode;
            if (nwMode > 1)
                _preferences->putBool(preference_ntw_reconfigure, true);
        }
    }

    // 2. Gemeinsame IP-Einstellungen
    if (server->hasArg("DHCPENA"))
        _preferences->putBool(preference_ip_dhcp_enabled, server->arg("DHCPENA") == "1");

    if (server->hasArg("IPADDR"))
        _preferences->putString(preference_ip_address, server->arg("IPADDR"));

    if (server->hasArg("IPSUB"))
        _preferences->putString(preference_ip_subnet, server->arg("IPSUB"));

    if (server->hasArg("IPGTW"))
        _preferences->putString(preference_ip_gateway, server->arg("IPGTW"));

    if (server->hasArg("DNSSRV"))
        _preferences->putString(preference_ip_dns_server, server->arg("DNSSRV"));

    // 3. Nur bei WLAN → SSID + Passwort + best RSSI
    if (nwMode == 1)
    {
        String ssid = server->arg("WIFISSID");
        String pass = server->arg("WIFIPASS");

        ssid.trim();
        pass.trim();

        if (server->hasArg("FINDBESTRSSI"))
            _preferences->putBool(preference_find_best_rssi, server->arg("FINDBESTRSSI") == "1");

        if (ssid.length() > 0 && pass.length() > 0)
        {
            if (_preferences->getBool(preference_ip_dhcp_enabled, true) &&
                _preferences->getString(preference_ip_address, "").isEmpty())
            {
                const IPConfiguration *_ipConfig = new IPConfiguration(_preferences);

                if (!_ipConfig->dhcpEnabled())
                {
                    WiFi.config(_ipConfig->ipAddress(), _ipConfig->dnsServer(), _ipConfig->defaultGateway(), _ipConfig->subnet());
                }
                delete _ipConfig;
            }

            WiFi.begin(ssid, pass);

            int loop = 0;
            while (!_network->isConnected() && loop < 150)
            {
                delay(100);
                loop++;
            }

            if (_network->isConnected())
            {
                _preferences->putString(preference_wifi_ssid, ssid);
                _preferences->putString(preference_wifi_pass, pass);
                message = "Connection successful. Rebooting Nuki Bridge.<br/>";
                result = true;
            }
            else
            {
                message = "Failed to connect to the given SSID with the given secret key, credentials not saved<br/>";
                result = false;
            }
        }
        else
        {
            message = "No SSID or secret key entered, credentials not saved<br/>";
            result = false;
        }
    }
    else
    {
        // LAN does not require an SSID test
        message = "LAN configuration saved. Rebooting Nuki Bridge.<br/>";
        result = true;
    }

    return result;
}

bool WebCfgServer::processFactoryReset(WebServer *server)
{
    bool res = false;
    String value = server->hasArg("CONFIRMTOKEN") ? server->arg("CONFIRMTOKEN") : "";

    bool resetWifi = false;
    if (value.isEmpty() || value != _confirmCode)
    {
        buildConfirmHtml(server, "Confirm code is invalid.", 3, true);
        return false;
    }
    else
    {
        if (server->hasArg("WIFI") && server->arg("WIFI") == "1")
        {
            resetWifi = true;
            buildConfirmHtml(server, "Factory resetting Nuki Bridge, unpairing Nuki Lock and resetting WiFi.", 3, true);
        }
        else
        {
            buildConfirmHtml(server, "Factory resetting Nuki Bridge, unpairing Nuki Lock.", 3, true);
        }
    }

    waitAndProcess(false, 2000);

    if (_nuki != nullptr)
    {
        _nuki->unpair();
    }

    String ssid = _preferences->getString(preference_wifi_ssid, "");
    String pass = _preferences->getString(preference_wifi_pass, "");

    _network->disableAPI();
    _network->disableHAR();
    _preferences->clear();

#ifndef CONFIG_IDF_TARGET_ESP32H2
    if (!resetWifi)
    {
        _preferences->putString(preference_wifi_ssid, ssid);
        _preferences->putString(preference_wifi_pass, pass);
    }
#endif

    Log->disableFileLog();
    waitAndProcess(false, 3000);
    restartEsp(RestartReason::NukiBridgeReset);
    return res;
}

bool WebCfgServer::processUnpair(WebServer *server)
{
    bool res = false;
    String value = server->hasArg("CONFIRMTOKEN") ? server->arg("CONFIRMTOKEN") : "";

    if (value != _confirmCode)
    {
        buildConfirmHtml(server, "Confirm code is invalid.", 3, true);
        return false;
    }

    buildConfirmHtml(server, "Unpairing Nuki Lock and restarting.", 3, true);

    if (_nuki)
    {
        _nuki->unpair();
        _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured);
    }

    _network->disableHAR();
    Log->disableFileLog();
    waitAndProcess(false, 1000);
    restartEsp(RestartReason::DeviceUnpaired);
    return res;
}

void WebCfgServer::createSsidList()
{
    int foundNetworks = WiFi.scanComplete();
    std::vector<String> _tmpSsidList;
    std::vector<int> _tmpRssiList;

    for (int i = 0; i < foundNetworks; i++)
    {
        int rssi = constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100);
        auto it1 = std::find(_ssidList.begin(), _ssidList.end(), WiFi.SSID(i));
        auto it2 = std::find(_tmpSsidList.begin(), _tmpSsidList.end(), WiFi.SSID(i));

        if (it1 == _ssidList.end())
        {
            _ssidList.push_back(WiFi.SSID(i));
            _rssiList.push_back(rssi);
            _tmpSsidList.push_back(WiFi.SSID(i));
            _tmpRssiList.push_back(rssi);
        }
        else if (it2 == _tmpSsidList.end())
        {
            _tmpSsidList.push_back(WiFi.SSID(i));
            _tmpRssiList.push_back(rssi);
            int index = it1 - _ssidList.begin();
            _rssiList[index] = rssi;
        }
        else
        {
            int index = it1 - _ssidList.begin();
            int index2 = it2 - _tmpSsidList.begin();
            if (_tmpRssiList[index2] < rssi)
            {
                _tmpRssiList[index2] = rssi;
                _rssiList[index] = rssi;
            }
        }
    }
}

void WebCfgServer::logoutSession(WebServer *server)
{
    Log->println(F("[DEBUG] Logging out from Web configurator"));

    // Set the cookies to empty and expiry time to 0 to delete them
    server->sendHeader("Set-Cookie", "sessionId=; path=/; HttpOnly");

    String cookieHeader = server->header("Cookie");
    if (cookieHeader.indexOf("sessionId=") != -1)
    {
        int startIndex = cookieHeader.indexOf("sessionId=") + 10;
        int endIndex = cookieHeader.indexOf(";", startIndex);
        if (endIndex == -1)
            endIndex = cookieHeader.length();
        String cookie = cookieHeader.substring(startIndex, endIndex);

        _httpSessions.remove(cookie);
        saveSessions();
    }
    else
    {
        Log->println(F("[INFO] No session cookie found"));
    }

    buildConfirmHtml(server, "Logging out", 3, true, "/");
}

const String WebCfgServer::pinStateToString(const NukiPinState &value) const
{
    switch (value)
    {
    case NukiPinState::NotSet:
        return String("PIN not set");
    case NukiPinState::Valid:
        return String("PIN valid");
    case NukiPinState::Invalid:
        return String("PIN set but invalid");
    case NukiPinState::NotConfigured:
    default:
        return String("Unknown");
    }
}

void WebCfgServer::saveSessions()
{
    if (_preferences->getBool(preference_update_time, false))
    {
        if (!SPIFFS.begin(true))
        {
            Log->println(F("[ERROR]SPIFFS Mount Failed"));
        }
        else
        {
            File file;

            file = SPIFFS.open("/sessions.json", "w");
            serializeJson(_httpSessions, file);

            file.close();
        }
    }
}

void WebCfgServer::loadSessions()
{
    if (_preferences->getBool(preference_update_time, false))
    {
        if (!SPIFFS.begin(true))
        {
            Log->println(F("[ERROR] SPIFFS Mount Failed"));
        }
        else
        {
            File file;

            file = SPIFFS.open("/sessions.json", "r");

            if (!file || file.isDirectory())
            {
                Log->println(F("[WARNING] sessions.json not found"));
            }
            else
            {
                deserializeJson(_httpSessions, file);
            }

            file.close();
        }
    }
}

void WebCfgServer::clearSessions()
{
    if (!SPIFFS.begin(true))
    {
        Log->println(F("[ERROR] SPIFFS Mount Failed"));
    }
    else
    {
        _httpSessions.clear();
        File file;
        file = SPIFFS.open("/sessions.json", "w");
        serializeJson(_httpSessions, file);
        file.close();
    }
}
