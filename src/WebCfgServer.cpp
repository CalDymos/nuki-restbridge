#include <Arduino.h>
#include <ESPmDNS.h>
#include "WebCfgServer.h"
#include "Logger.hpp"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "NetworkDeviceType.h"
#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
#include "esp_psram.h"
#endif

const char css[] PROGMEM = ":root{--nc-font-sans:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,'Open Sans','Helvetica Neue',sans-serif,'Apple Color Emoji','Segoe UI Emoji','Segoe UI Symbol';--nc-font-mono:Consolas,monaco,'Ubuntu Mono','Liberation Mono','Courier New',Courier,monospace;--nc-tx-1:#000;--nc-tx-2:#1a1a1a;--nc-bg-1:#fff;--nc-bg-2:#f6f8fa;--nc-bg-3:#e5e7eb;--nc-lk-1:#0070f3;--nc-lk-2:#0366d6;--nc-lk-tx:#fff;--nc-ac-1:#79ffe1;--nc-ac-tx:#0c4047}@media(prefers-color-scheme:dark){:root{--nc-tx-1:#fff;--nc-tx-2:#eee;--nc-bg-1:#000;--nc-bg-2:#111;--nc-bg-3:#222;--nc-lk-1:#3291ff;--nc-lk-2:#0070f3;--nc-lk-tx:#fff;--nc-ac-1:#7928ca;--nc-ac-tx:#fff}}*{margin:0;padding:0}img,input,option,p,table,textarea,ul{margin-bottom:1rem}button,html,input,select{font-family:var(--nc-font-sans)}body{margin:0 auto;max-width:750px;padding:2rem;border-radius:6px;overflow-x:hidden;word-break:normal;overflow-wrap:anywhere;background:var(--nc-bg-1);color:var(--nc-tx-2);font-size:1.03rem;line-height:1.5}::selection{background:var(--nc-ac-1);color:var(--nc-ac-tx)}h1,h2,h3,h4,h5,h6{line-height:1;color:var(--nc-tx-1);padding-top:.875rem}h1,h2,h3{color:var(--nc-tx-1);padding-bottom:2px;margin-bottom:8px;border-bottom:1px solid var(--nc-bg-2)}h4,h5,h6{margin-bottom:.3rem}h1{font-size:2.25rem}h2{font-size:1.85rem}h3{font-size:1.55rem}h4{font-size:1.25rem}h5{font-size:1rem}h6{font-size:.875rem}a{color:var(--nc-lk-1)}a:hover{color:var(--nc-lk-2) !important;}abbr{cursor:help}abbr:hover{cursor:help}a button,button,input[type=button],input[type=reset],input[type=submit]{font-size:1rem;display:inline-block;padding:6px 12px;text-align:center;text-decoration:none;white-space:nowrap;background:var(--nc-lk-1);color:var(--nc-lk-tx);border:0;border-radius:4px;box-sizing:border-box;cursor:pointer;color:var(--nc-lk-tx)}a button[disabled],button[disabled],input[type=button][disabled],input[type=reset][disabled],input[type=submit][disabled]{cursor:default;opacity:.5;cursor:not-allowed}.button:focus,.button:hover,button:focus,button:hover,input[type=button]:focus,input[type=button]:hover,input[type=reset]:focus,input[type=reset]:hover,input[type=submit]:focus,input[type=submit]:hover{background:var(--nc-lk-2)}table{border-collapse:collapse;width:100%}td,th{border:1px solid var(--nc-bg-3);text-align:left;padding:.5rem}th{background:var(--nc-bg-2)}tr:nth-child(even){background:var(--nc-bg-2)}textarea{max-width:100%}input,select,textarea{padding:6px 12px;margin-bottom:.5rem;background:var(--nc-bg-2);color:var(--nc-tx-2);border:1px solid var(--nc-bg-3);border-radius:4px;box-shadow:none;box-sizing:border-box}img{max-width:100%}td>input{margin-top:0;margin-bottom:0}td>textarea{margin-top:0;margin-bottom:0}td>select{margin-top:0;margin-bottom:0}.warning{color:red}@media only screen and (max-width:600px){.adapt td{display:block}.adapt input[type=text],.adapt input[type=password],.adapt input[type=submit],.adapt textarea,.adapt select{width:100%}.adapt td:has(input[type=checkbox]){text-align:center}.adapt input[type=checkbox]{width:1.5em;height:1.5em}.adapt table td:first-child{border-bottom:0}.adapt table td:last-child{border-top:0}#tblnav a li>span{max-width:140px}}#tblnav a{border:0;border-bottom:1px solid;display:block;font-size:1rem;font-weight:bold;padding:.6rem 0;line-height:1;color:var(--nc-tx-1);text-decoration:none;background:linear-gradient(to left,transparent 50%,rgba(255,255,255,0.4) 50%) right;background-size:200% 100%;transition:all .2s ease}#tblnav a{background:linear-gradient(to left,var(--nc-bg-2) 50%,rgba(255,255,255,0.4) 50%) right;background-size:200% 100%}#tblnav a:hover{background-position:left;transition:all .45s ease}#tblnav a:active{background:var(--nc-lk-1);transition:all .15s ease}#tblnav a li{list-style:none;padding:.5rem;display:inline-block;width:100%}#tblnav a li>span{float:right;text-align:right;margin-right:10px;color:#f70;font-weight:100;font-style:italic;display:block}.tdbtn{text-align:center;vertical-align:middle}.naventry{float:left;max-width:375px;width:100%}";
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

        _webServer->on("/savewifi", HTTP_POST, [this]()
                       {            int authReq = doAuthentication(this->_webServer);
                       
                                   switch (authReq)
                                   {
                                       case 0:
                                           return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                                           break;
                                       case 1:
                                           return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
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
                                   bool connected = processWiFi(this->_webServer, message);
                                   buildConfirmHtml(this->_webServer, message, 10, true);
                       
                                   if(connected)
                                   {
                                       waitAndProcess(true, 3000);
                                       restartEsp(RestartReason::ReconfigureWifi);
                                   }
                                   return; });

        _webServer->on("/reboot", HTTP_GET, [this]()
                       {
                                   int authReq = doAuthentication(this->_webServer);
                       
                                   switch (authReq)
                                   {
                                       case 0:
                                           return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                                           break;
                                       case 1:
                                           return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
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
                                   if (this->_webServer->hasArg("CONFIRMTOKEN")) // WebServer ersetzt PsychicRequest
                                   {
                                       value = this->_webServer->arg("CONFIRMTOKEN"); // Abrufen des Wertes aus dem GET/POST-Parameter
                               
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
                        return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                        break;
                    case 1:
                        return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
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
                    this->_webServer->send(200, "application/json", "{}");
                }
            }
            if (value == "login")
            {
                return buildLoginHtml(this->_webServer);
            }
            else if (value == "bypass")
            {
                return buildBypassHtml(this->_webServer);
            }
            else if (value == "newbypass" && _newBypass)
            {
                _newBypass = false;
                return buildConfirmHtml(this->_webServer, "Logged in using Bypass. New bypass: " + _preferences->getString(preference_bypass_secret, "") + " <br/><br/><a href=\"/\">Home page</a>", 3, false);
            }
            else if (value == "logout")
            {
                return logoutSession(this->_webServer);
            }
            else if (value == "coredump")
            {
                return buildCoredumpHtml(this->_webServer);
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
                waitAndProcess(true, 1000);
                restartEsp(RestartReason::RequestedViaWebCfgServer);
                return;
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
                //return buildHARConfigHtml(this->_webServer); // Site to Configure the Home Automation (enable, Adress, Port, Mode [udp, rest], user, password)
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
                buildConfirmHtml(this->_webServer, "Restarting. Connect to ESP access point (\"NukiBridge\" with password \"NukiBridgeESP32\") to reconfigure Wi-Fi.", 0);
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
                            return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                            break;
                        case 1:
                            return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
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
                else if (value == "bypass")
                {
                    bool loggedIn = processBypass(this->_webServer);
                    if (loggedIn)
                    {
                        this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                        _newBypass = true;
                        return this->redirect(this->_webServer, "/get?page=newbypass", 302);
                    }
                    else
                    {
                        this->_webServer->sendHeader(F("Cache-Control"),  F("no-cache"));
                        return this->redirect(this->_webServer, "/", 302);
                    }
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
                        return buildWifiConnectHtml(this->_webServer);
                    }
#endif
                } });
    }

    _webServer->on("/", HTTP_GET, [this]()
                   {
                       Log->println("[DEBUG] Anfrage an / erhalten!");

                       int authReq = doAuthentication(this->_webServer);

                       switch (authReq)
                       {
                       case 0:
                           return this->_webServer->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                           break;
                       case 1:
                           return this->_webServer->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
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
                           return buildWifiConnectHtml(this->_webServer);
                       }
#endif
                   });

    // Eigentliches Starten des Webservers
    _webServer->begin();
    Log->println("[INFO] WebCfgServer started on http://" + WiFi.softAPIP().toString() + ":" + String(WEBCFGSERVER_PORT));

    if (MDNS.begin(_preferences->getString(preference_hostname, "nukibridge").c_str()))
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

    appendCheckBoxRow(response, "CONFNHCTRL", "Modify Nuki Hub configuration over REST API", _preferences->getBool(preference_config_from_api, false), "");

    if ((_nuki && _nuki->hasKeypad()))
    {
        appendCheckBoxRow(response, "KPENA", "Add, modify and delete keypad codes", _preferences->getBool(preference_keypad_control_enabled), "");
    }

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

    appendCheckBoxRow(response, "LOCKENA", "Nuki Lock enabled", _preferences->getBool(preference_lock_enabled, true), "");
    appendCheckBoxRow(response, "CONNMODE", "New Nuki Bluetooth connection mode (disable if there are connection issues)", _preferences->getBool(preference_connect_mode, true), "");

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

    appendCheckBoxRow(response, "UPTIME", "Update Nuki Hub and Lock/Opener time using NTP", _preferences->getBool(preference_update_time, false), "");
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

    appendCheckBoxRow(response, "DISNTWNOCON", "Disable Network if not connected within 60s", _preferences->getBool(preference_disable_network_not_connected, false), "");
    appendCheckBoxRow(response, "BTLPRST", "Enable Bootloop prevention (Try to reset these settings to default on bootloop)", true, "");

    appendInputFieldRow(response, "BUFFSIZE", "Char buffer size (min 4096, max 65536)", _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE), 6, "");
    response += F("<tr><td>Advised minimum char buffer size based on current settings</td><td id=\"mincharbuffer\"></td>");
    appendInputFieldRow(response, "TSKNTWK", "Task size Network (min 8192, max 65536)", _preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), 6, "");
    response += F("<tr><td>Advised minimum network task size based on current settings</td><td id=\"minnetworktask\"></td>");
    appendInputFieldRow(response, "TSKNUKI", "Task size Nuki (min 8192, max 65536)", _preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), 6, "");

    appendInputFieldRow(response, "ALMAX", "Max auth log entries (min 1, max 100)", _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 3, "id=\"inputmaxauthlog\"");
    appendInputFieldRow(response, "KPMAX", "Max keypad entries (min 1, max 200)", _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD), 3, "id=\"inputmaxkeypad\"");
    appendInputFieldRow(response, "TCMAX", "Max timecontrol entries (min 1, max 100)", _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL), 3, "id=\"inputmaxtimecontrol\"");
    appendInputFieldRow(response, "AUTHMAX", "Max authorization entries (min 1, max 100)", _preferences->getInt(preference_auth_max_entries, MAX_AUTH), 3, "id=\"inputmaxauth\"");

    appendCheckBoxRow(response, "SHOWSECRETS", "Show Pairing secrets on Info page", _preferences->getBool(preference_show_secrets), "");

    if (_preferences->getBool(preference_lock_enabled, true))
    {
        appendCheckBoxRow(response, "LCKMANPAIR", "Manually set lock pairing data (enable to save values below)", false, "");
        appendInputFieldRow(response, "LCKBLEADDR", "currentBleAddress", "", 12, "");
        appendInputFieldRow(response, "LCKSECRETK", "secretKeyK", "", 64, "");
        appendInputFieldRow(response, "LCKAUTHID", "authorizationId", "", 8, "");
        appendCheckBoxRow(response, "LCKISULTRA", "isUltra", false, "");
    }

    if (_nuki != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        appendCheckBoxRow(response, "LCKFORCEID", ((String) "Force Lock ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_lock_force_id, false), "");
        appendCheckBoxRow(response, "LCKFORCEKP", "Force Lock Keypad connected", _preferences->getBool(preference_lock_force_keypad, false), "");
        appendCheckBoxRow(response, "LCKFORCEDS", "Force Lock Doorsensor connected", _preferences->getBool(preference_lock_force_doorsensor, false), "");
    }

    appendCheckBoxRow(response, "DBGCONN", "Enable Nuki connect debug logging", _preferences->getBool(preference_debug_connect, false), "");
    appendCheckBoxRow(response, "DBGCOMMU", "Enable Nuki communication debug logging", _preferences->getBool(preference_debug_communication, false), "");
    appendCheckBoxRow(response, "DBGREAD", "Enable Nuki readable data debug logging", _preferences->getBool(preference_debug_readable_data, false), "");
    appendCheckBoxRow(response, "DBGHEX", "Enable Nuki hex data debug logging", _preferences->getBool(preference_debug_hex_data, false), "");
    appendCheckBoxRow(response, "DBGCOMM", "Enable Nuki command debug logging", _preferences->getBool(preference_debug_command, false), "");
    appendCheckBoxRow(response, "DBGHEAP", "Send free heap to Home Automation", _preferences->getBool(preference_send_debug_info, false), "");

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
                  "networktask = Math.max(10240 + charbuf, 12288);"
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
    response += F("</head><body><center><h2>Nuki Hub login</h2>");
    response += F("<form action=\"/post?page=login\" method=\"post\">");
    response += F("<div class=\"container\">");
    response += F("<label for=\"username\"><b>Username</b></label>");
    response += F("<input type=\"text\" placeholder=\"Enter Username\" name=\"username\" required>");
    response += F("<label for=\"password\"><b>Password</b></label>");
    response += F("<input type=\"password\" placeholder=\"Enter Password\" name=\"password\" required>");

    response += F("<button type=\"submit\">Login</button>");

    response += F("<label><input type=\"checkbox\" name=\"remember\"> Remember me</label></div>");
    response += F("</form></center></body></html>");

    server->send(200, "text/html", response);
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

void WebCfgServer::buildCoredumpHtml(WebServer *server)
{
    if (!SPIFFS.begin(true))
    {
        Log->println(F("SPIFFS Mount Failed"));
    }
    else
    {
        File file = SPIFFS.open(F("/coredump.hex"), "r");

        if (!file || file.isDirectory())
        {
            Log->println(F("coredump.hex not found"));
        }
        else
        {
            server->sendHeader(F("Content-Disposition"), F("attachment; filename=\"coredump.txt\""));
            server->streamFile(file, F("application/octet-stream"));
            file.close();
            return;
        }
    }

    server->sendHeader(F("Cache-Control"), F("no-cache"));
    server->sendHeader(F("Location"), F("/"));
    server->send(302, F("text/plain"), "");
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

    appendInputFieldRow(response, "HOSTNAME", "Host name", _preferences->getString(preference_hostname).c_str(), 100, "");
    appendDropDownRow(response, "NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions(), "");

#ifndef CONFIG_IDF_TARGET_ESP32H2
    appendInputFieldRow(response, "RSSI", "RSSI Publish interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_send_interval), 6, "");
#endif

    appendCheckBoxRow(response, "RSTDISC", "Restart on disconnect", _preferences->getBool(preference_restart_on_disconnect), "");
    appendCheckBoxRow(response, "FINDBESTRSSI", "Find WiFi AP with strongest signal", _preferences->getBool(preference_find_best_rssi, false), "");

    response += F("</table>");
    response += F("<h3>IP Address assignment</h3>");
    response += F("<table>");

    appendCheckBoxRow(response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled), "");
    appendInputFieldRow(response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    appendInputFieldRow(response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    appendInputFieldRow(response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    appendInputFieldRow(response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");

    response += F("</table>");
    response += F("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response += F("</form>");
    response += F("</body></html>");

    server->send(200, "text/html", response);
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
    // Generiere zufällige Strings für One-Time-Bypass & Admin-Schlüssel
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

    // HTML-Antwort aufbauen
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
    response += F("<h3>Credentials</h3><table>");

    appendInputFieldRow(response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 30, "id=\"inputuser\"", false, true);
    appendInputFieldRow(response, "CREDPASS", "Password", "*", 30, "id=\"inputpass\"", true, true);
    appendInputFieldRow(response, "CREDPASSRE", "Retype password", "*", 30, "id=\"inputpass2\"", true);

    std::vector<std::pair<String, String>> httpAuthOptions = {
        {"0", "Basic"},
        {"1", "Digest"},
        {"2", "Form"}};
    appendDropDownRow(response, "CREDDIGEST", "HTTP Authentication type", String(_preferences->getInt(preference_http_auth_type, 0)), httpAuthOptions, "");

    appendInputFieldRow(response, "CREDTRUSTPROXY", "Bypass authentication for reverse proxy with IP", _preferences->getString(preference_bypass_proxy, "").c_str(), 255, "");
    appendInputFieldRow(response, "CREDBYPASS", "One-time MFA Bypass", "*", 32, "", true, false);
    response += F("<tr id=\"bypassgentr\"><td><input type=\"button\" id=\"bypassgen\" onclick=\"document.getElementsByName('CREDBYPASS')[0].type='text'; document.getElementsByName('CREDBYPASS')[0].value='");
    response += randomstr2;
    response += F("'; document.getElementById('bypassgentr').style.display='none';\" value=\"Generate new Bypass\"></td></tr>");

    appendInputFieldRow(response, "CREDADMIN", "Admin key", "*", 32, "", true, false);
    response += F("<tr id=\"admingentr\"><td><input type=\"button\" id=\"admingen\" onclick=\"document.getElementsByName('CREDADMIN')[0].type='text'; document.getElementsByName('CREDADMIN')[0].value='");
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
    response += F("<br><br><h3>Factory reset Nuki Hub</h3><h4 class=\"warning\">This will reset all settings to default...</h4>");
    response += F("<form class=\"adapt\" method=\"post\" action=\"/post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"factoryreset\">");
    appendInputFieldRow(response, "CONFIRMTOKEN", ("Type " + _confirmCode + " to confirm factory reset").c_str(), "", 10, "");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    appendCheckBoxRow(response, "WIFI", "Also reset WiFi settings", false, "");
#endif
    response += F("</table><br><button type=\"submit\">OK</button></form></body></html>");

    server->send(200, "text/html", response);
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

    appendCheckBoxRow(response, "APIENA", "Enable REST API", _preferences->getBool(preference_api_enabled, false), "");
    appendInputFieldRow(response, "APIPORT", "API Port", _preferences->getInt(preference_api_port, 8080), 6, "");

    const char *currentToken = _network->getApiToken();

    response += F("<tr><td>Access Token</td><td>");
    response += "<input type=\"text\" value=\"" + String(currentToken) + "\" readonly>";
    response += F("&nbsp;<a href=\"/get?page=apiconfig&genapitoken=1\"><input type=\"button\" value=\"Generate new token\"></a>");
    response += F("</td></tr>");

    response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\"></form></body></html>");

    server->send(200, "text/html", response);
}

void WebCfgServer::buildHARConfigHtml(WebServer *server)
{
    String response;
    reserveHtmlResponse(response,
                        1, // Checkbox
                        4, // Input fields
                        1, // Dropdown
                        2, // Dropdown options
                        0, // Textareas
                        0, // Parameter rows
                        0, // Buttons (Generate Bypass/Admin)
                        0, // menus
                        0  // extra bytes
    );

    buildHtmlHeader(response);
    response += F("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<h3>Home Automation Configuration</h3><table>");

    appendCheckBoxRow(response, "HAENA", "Enable Home Automation", _preferences->getBool(preference_ha_enabled, false), "");
    appendInputFieldRow(response, "HAHOST", "Address", _preferences->getString(preference_ha_address, "").c_str(), 64, "");
    appendInputFieldRow(response, "HAPORT", "Port", _preferences->getInt(preference_ha_port, 8081), 6, "");

    std::vector<std::pair<String, String>> modeOptions = {
        {"0", "UDP"},
        {"1", "REST"}};
    appendDropDownRow(response, "HAMODE", "Mode", String(_preferences->getInt(preference_ha_mode, 0)), modeOptions, "");

    appendInputFieldRow(response, "HAUSER", "Username", _preferences->getString(preference_ha_user, "").c_str(), 32, "");
    appendInputFieldRow(response, "HAPASS", "Password", _preferences->getString(preference_ha_password, "").c_str(), 32, "", true, true);

    response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\"></form></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildHtml(WebServer *server)
{
    String header = F("<script>let intervalId; window.onload = function() { updateInfo(); intervalId = setInterval(updateInfo, 3000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/get?page=status', true); request.onload = () => { const obj = JSON.parse(request.responseText); if (obj.stop == 1) { clearInterval(intervalId); } for (var key of Object.keys(obj)) { if(key=='ota' && document.getElementById(key) !== null) { document.getElementById(key).innerText = \"<a href='/ota'>\" + obj[key] + \"</a>\"; } else if(document.getElementById(key) !== null) { document.getElementById(key).innerText = obj[key]; } } }; request.send(); }</script>");

    String response;
    reserveHtmlResponse(response,
                        0,  // Checkbox
                        0,  // Input fields
                        0,  // Dropdown
                        0,  // Dropdown options
                        0,  // Textareas
                        7,  // Parameter rows
                        0,  // Buttons (Generate Bypass/Admin)
                        11, // Naviagtion menus
                        0   // extra bytes ()
    );

    buildHtmlHeader(response, header);

    if (_rebootRequired)
    {
        response += F("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">REBOOT REQUIRED TO APPLY SETTINGS</td></tr></tbody></table>");
    }

    response += F("<h3>Info</h3><br><table>");
    appendParameterRow(response, "Hostname", _hostname.c_str(), "", "hostname");
    appendParameterRow(response, "REST API reachable", (_network->networkServicesState() == NetworkServiceStates::OK || _network->networkServicesState() == NetworkServiceStates::WEBSERVER_NOT_REACHABLE) ? "Yes" : "No", "", "APIState");
    appendParameterRow(response, "Home Automation reachable", (_network->networkServicesState() == NetworkServiceStates::OK || _network->networkServicesState() == NetworkServiceStates::HTTPCLIENT_NOT_REACHABLE) ? "Yes" : "No", "", "HAState");

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
    response += F("</table><br><ul id=\"tblnav\">");

    appendNavigationMenuEntry(response, "Network Configuration", "/get?page=ntwconfig");
    appendNavigationMenuEntry(response, "REST API Configuration", "/get?page=apiconfig", _APIConfigured ? "" : "Please configure REST API");
    appendNavigationMenuEntry(response, "Home Automation Configuration", "/get?page=harconfig", _HAConfigured ? "" : "Please configure Home Automation");
    appendNavigationMenuEntry(response, "Nuki Configuration", "/get?page=nukicfg");
    appendNavigationMenuEntry(response, "Access Level Configuration", "/get?page=acclvl");
    appendNavigationMenuEntry(response, "Credentials", "/get?page=cred");

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
    appendNavigationMenuEntry(response, "Reboot Nuki Hub", rebooturl.c_str());

    if (_preferences->getInt(preference_http_auth_type, 0) == 2)
    {
        appendNavigationMenuEntry(response, "Logout", "/get?page=logout");
    }

    response += F("</ul></body></html>");

    server->send(200, F("text/html"), response);
}

void WebCfgServer::buildBypassHtml(WebServer *server)
{
    if (timeSynced)
    {
        buildConfirmHtml(server, "One-time bypass is only available if NTP time is not synced</a>", 3, true);
        return;
    }

    String response;
    reserveHtmlResponse(response,
                        0,  // Checkbox
                        1,  // Input fields
                        0,  // Dropdown
                        0,  // Dropdown options
                        0,  // Textareas
                        0,  // Parameter rows
                        1,  // Buttons
                        0,  // Naviagtion menus
                        768 // extra CSS
    );

    response += F("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    response += F("<style>form{border:3px solid #f1f1f1; max-width: 400px;}");
    response += F("input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;");
    response += F("display:inline-block;border:1px solid #ccc;box-sizing:border-box}");
    response += F("button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;");
    response += F("border:none;cursor:pointer;width:100%}button:hover{opacity:.8}");
    response += F(".container{padding:16px}span.password{float:right;padding-top:16px}");
    response += F("@media screen and (max-width:300px){span.psw{display:block;float:none}}");
    response += F("</style></head><body><center><h2>Nuki Hub One-time Bypass</h2>");
    response += F("<form action=\"/post?page=bypass\" method=\"post\">");
    response += F("<div class=\"container\">");
    response += F("<label for=\"bypass\"><b>Bypass code</b></label>");
    response += F("<input type=\"text\" placeholder=\"Enter bypass code\" name=\"bypass\">");
    response += F("<button type=\"submit\">Login</button></div>");
    response += F("</form></center></body></html>");

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

void WebCfgServer::buildWifiConnectHtml(WebServer *server)
{
    String header = F("<style>.trssid:hover { cursor: pointer; color: blue; }</style>"
                      "<script>let intervalId; window.onload = function() { intervalId = setInterval(updateSSID, 5000); };"
                      "function updateSSID() { var request = new XMLHttpRequest(); request.open('GET', '/ssidlist', true);"
                      "request.onload = () => { if (document.getElementById(\"aplist\") !== null) { document.getElementById(\"aplist\").innerHTML = request.responseText; } }; request.send(); }</script>");

    String response;
    reserveHtmlResponse(response,
                        2,                              // Checkbox
                        5,                              // Input fields
                        0,                              // Dropdown
                        0,                              // Dropdown options
                        0,                              // Textareas
                        0,                              // Parameter rows
                        2,                              // Buttons
                        0,                              // Naviagtion menus
                        (_ssidList.size() * 160) + 1024 // extra bytes ()
    );

    buildHtmlHeader(response, header);

    response += F("<h3>Available WiFi networks</h3><table id=\"aplist\">");

    createSsidList();
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

    response += F("</table><form class=\"adapt\" method=\"post\" action=\"savewifi\">"
                  "<h3>WiFi credentials</h3><table>");

    appendInputFieldRow(response, "WIFISSID", "SSID", "", 32, "id=\"inputssid\"", false, true);
    appendInputFieldRow(response, "WIFIPASS", "Secret key", "", 63, "id=\"inputpass\"", false, true);
    appendCheckBoxRow(response, "FINDBESTRSSI", "Find AP with best signal (disable for hidden SSID)", _preferences->getBool(preference_find_best_rssi, true), "");

    response += F("</table><h3>IP Address assignment</h3><table>");

    appendCheckBoxRow(response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled), "");
    appendInputFieldRow(response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    appendInputFieldRow(response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    appendInputFieldRow(response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    appendInputFieldRow(response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");

    response += F("</table><br><input type=\"submit\" name=\"submit\" value=\"Save\"></form>"
                  "<form action=\"/reboot\" method=\"get\"><br>"
                  "<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"");
    response += _confirmCode;
    response += F("\" />");
    response += F("<input type=\"submit\" value=\"Reboot\" /></form></body></html>");

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
    response += "<h3>System Information</h3><pre>";
    response += "\n----------- NUKI BRIDGE ------------";
    response += "\nDevice: " + String(NUKI_REST_BRIDGE_HW);
    response += "\nVersion: " + String(NUKI_REST_BRIDGE_VERSION);
    response += "\nBuild: " + String(NUKI_REST_BRIDGE_BUILD);
#ifndef DEBUG
    response += "\nBuild type: Release";
#else
    response += "\nBuild type: Debug";
#endif
    response += "\nBuild date: " + String(NUKI_REST_BRIDGE_DATE);
    response += "\nUptime (min): " + String(espMillis() / 1000 / 60);
    response += "\nLast restart reason FW: " + getRestartReason();
    response += "\nLast restart reason ESP: " + getEspRestartReason();
    response += "\nFree internal heap: " + String(ESP.getFreeHeap());
    response += "\nTotal internal heap: " + String(ESP.getHeapSize());

#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
    if (esp_psram_get_size() > 0)
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
    response += F("\nUpdate Nuki Hub and Nuki devices time using NTP: ");
    response += String(_preferences->getBool(preference_update_time, false) ? "Yes" : "No");

    response += F("\nWeb configurator enabled: ");
    response += String(_preferences->getBool(preference_webcfgserver_enabled, true) ? "Yes" : "No");
    response += F("\nWeb configurator username: ");
    response += String(_preferences->getString(preference_cred_user, "").length() > 0 ? "***" : "Not set");
    response += F("\nWeb configurator password: ");
    response += String(_preferences->getString(preference_cred_password, "").length() > 0 ? "***" : "Not set");
    response += F("\nWeb configurator bypass for proxy IP: ");
    response += String(_preferences->getString(preference_bypass_proxy, "").length() > 0 ? "***" : "Not set");
    response += F("\nWeb configurator authentication: ");
    response += String(_preferences->getInt(preference_http_auth_type, 0) == 0 ? "Basic" : _preferences->getInt(preference_http_auth_type, 0) == 1 ? "Digest"
                                                                                                                                                   : "Form");
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
    response += String(_network->isConnected() ? "Yes" : "No");
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
    response += F("\nNuki Hub hostname: ");
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
        response += F("\nRSSI Publish interval (s): ");
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
    response += String(_preferences->getBool(preference_api_enabled, false) != false ? "Yes" : "No");
    response += F("\nAPI connected: ");
    response += String((_network->networkServicesState() == NetworkServiceStates::OK || _network->networkServicesState() == NetworkServiceStates::HTTPCLIENT_NOT_REACHABLE) ? "Yes" : "No");
    response += F("\nAPI Port: ");
    response += String(_preferences->getInt(preference_api_port, 0));
    response += F("\nAPI auth token: ");
    response += _preferences->getString(preference_api_token, "not defined");

    // HomeAutomation
    response += F("\n\n------------ HOME AAUTOMATION REPORTING ------------");
    response += F("\nHAR enabled: ");
    response += String(_preferences->getBool(preference_ha_enabled, false) != false ? "Yes" : "No");
    response += F("\nHA reachable: ");
    response += String((_network->networkServicesState() == NetworkServiceStates::OK || _network->networkServicesState() == NetworkServiceStates::WEBSERVER_NOT_REACHABLE) ? "Yes" : "No");
    response += F("\nHA address: ");
    response += _preferences->getString(preference_ha_address, "not defined");
    response += F("\nHA user: ");
    response += _preferences->getString(preference_ha_user, "not defined");
    response += F("\nHA password: ");
    response += _preferences->getString(preference_ha_password, "not defined");
    response += F("\nHAR mode: ");
    response += _preferences->getString(preference_ha_mode, "not defined");

    // Bluetooth Infos
    response += F("\n\n------------ BLUETOOTH ------------");
    response += F("\nBluetooth connection mode: ");
    response += String(_preferences->getBool(preference_connect_mode, true) ? "New" : "Old");
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
        response += F("\nNuki Hub device ID: ");
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
        nukiBlePref.begin("NukiHub", false);
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

void WebCfgServer::appendNavigationMenuEntry(String &response, const char *title, const char *targetPath, const char *warningMessage)
{
    response += F("<a class=\"naventry\" href=\"");
    response += targetPath;
    response += F("\"><li>");
    response += title;

    if (*warningMessage) // Statt strcmp(warningMessage, "") != 0
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
                                     const String className)
{
    response += F("<tr><td>");
    response += description;
    response += F("</td><td>");

    response += F("<select ");
    if (className.length() > 0)
    {
        response += F("class=\"");
        response += className;
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
                                     const char *htmlClass)
{
    response += F("<tr><td>");
    response += description;
    response += F("</td><td>");

    response += F("<input type=\"hidden\" name=\"");
    response += token;
    response += F("\" value=\"0\"/>");

    response += F("<input type=\"checkbox\" name=\"");
    response += token;
    response += F("\" class=\"");
    response += htmlClass;
    response += F("\" value=\"1\"");

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

        if (key == "HARADDRESS")
        {
            if (_preferences->getString(preference_ha_address, "") != value)
            {
                _preferences->putString(preference_ha_address, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "HARPORT")
        {
            if (_preferences->getInt(preference_ha_port, 0) != value.toInt())
            {
                _preferences->putInt(preference_ha_port, value.toInt());
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "HARUSER")
        {
            if (value == "#")
            {
                clearHARCredentials = true;
            }
            else
            {
                if (_preferences->getString(preference_ha_user, "") != value)
                {
                    _preferences->putString(preference_ha_user, value);
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if (key == "HARPASS")
        {
            if (_preferences->getString(preference_ha_password, "") != value)
            {
                _preferences->putString(preference_ha_password, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if (key == "ENHAR")
        {
            if (_preferences->getBool(preference_ha_enabled, false) != (value == "1"))
            {
                _network->disableHAR();
                _preferences->putBool(preference_ha_enabled, (value == "1"));
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
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
        else if (key == "TIMESRV")
        {
            if (_preferences->getString(preference_time_server, "pool.ntp.org") != value)
            {
                _preferences->putString(preference_time_server, value);
                Log->print(F("[DEBUG] Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
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
        else if (key == "HOSTNAME")
        {
            if (_preferences->getString(preference_hostname, "") != value)
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
        else if (key == "CREDBYPASS")
        {
            if (value != "*")
            {
                if (_preferences->getString(preference_bypass_secret, "") != value)
                {
                    _preferences->putString(preference_bypass_secret, value);
                    Log->print(F("[DEBUG] Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
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
        if (_preferences->getString(preference_ha_user, "") != "")
        {
            _preferences->putString(preference_ha_user, "");
            Log->print(F("[DEBUG] Setting changed: "));
            Log->println(F("HARUSER"));
            configChanged = true;
        }
        if (_preferences->getString(preference_ha_password, "") != "")
        {
            _preferences->putString(preference_ha_password, "");
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

bool WebCfgServer::processBypass(WebServer *server)
{
    if (!timeSynced && server->hasArg("bypass"))
    {
        String bypass = server->arg("bypass");
        if (!bypass.isEmpty())
        {
            char buffer[33];
            for (int i = 0; i < 4; i++)
            {
                snprintf(buffer + (i * 8), 9, "%08lx", (unsigned long)esp_random());
            }

            server->sendHeader("Set-Cookie", "bypassId=" + String(buffer) + "; Max-Age=3600; HttpOnly");

            struct timeval time;
            gettimeofday(&time, NULL);
            int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;

            char randomstr2[33];
            randomSeed(esp_random()); // Safer method for initializing the random number generator
            const char chars[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";

            for (int i = 0; i < 32; i++)
            {
                randomstr2[i] = chars[random(36)];
            }
            randomstr2[32] = '\0';

            _preferences->putString(preference_bypass_secret, randomstr2);

            return true;
        }
    }
    return false;
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

bool WebCfgServer::processWiFi(WebServer *server, String &message)
{
    bool res = false;
    String ssid;
    String pass;

    for (uint8_t i = 0; i < server->args(); i++) // WebServer nutzt args()
    {
        String key = server->argName(i);
        String value = server->arg(i);

        if (key == "WIFISSID")
        {
            ssid = value;
        }
        else if (key == "WIFIPASS")
        {
            pass = value;
        }
        else if (key == "DHCPENA")
        {
            if (_preferences->getBool(preference_ip_dhcp_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_ip_dhcp_enabled, (value == "1"));
            }
        }
        else if (key == "IPADDR")
        {
            if (_preferences->getString(preference_ip_address, "") != value)
            {
                _preferences->putString(preference_ip_address, value);
            }
        }
        else if (key == "IPSUB")
        {
            if (_preferences->getString(preference_ip_subnet, "") != value)
            {
                _preferences->putString(preference_ip_subnet, value);
            }
        }
        else if (key == "IPGTW")
        {
            if (_preferences->getString(preference_ip_gateway, "") != value)
            {
                _preferences->putString(preference_ip_gateway, value);
            }
        }
        else if (key == "DNSSRV")
        {
            if (_preferences->getString(preference_ip_dns_server, "") != value)
            {
                _preferences->putString(preference_ip_dns_server, value);
            }
        }
        else if (key == "FINDBESTRSSI")
        {
            if (_preferences->getBool(preference_find_best_rssi, false) != (value == "1"))
            {
                _preferences->putBool(preference_find_best_rssi, (value == "1"));
            }
        }
    }

    ssid.trim();
    pass.trim();

    if (ssid.length() > 0 && pass.length() > 0)
    {
        if (_preferences->getBool(preference_ip_dhcp_enabled, true) && _preferences->getString(preference_ip_address, "").length() <= 0)
        {
            const IPConfiguration *_ipConfiguration = new IPConfiguration(_preferences);

            if (!_ipConfiguration->dhcpEnabled())
            {
                WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
            }
        }

        WiFi.begin(ssid, pass);

        int loop = 0;
        while (!_network->isConnected() && loop < 150)
        {
            delay(100);
            loop++;
        }

        if (!_network->isConnected())
        {
            message = "Failed to connect to the given SSID with the given secret key, credentials not saved<br/>";
            return res;
        }
        else
        {
            message = "Connection successful. Rebooting Nuki Hub.<br/>";
            _preferences->putString(preference_wifi_ssid, ssid);
            _preferences->putString(preference_wifi_pass, pass);
            res = true;
        }
    }
    else
    {
        message = "No SSID or secret key entered, credentials not saved<br/>";
    }

    return res;
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
            buildConfirmHtml(server, "Factory resetting Nuki Hub, unpairing Nuki Lock and resetting WiFi.", 3, true);
        }
        else
        {
            buildConfirmHtml(server, "Factory resetting Nuki Hub, unpairing Nuki Lock.", 3, true);
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

    // Setzen der Cookies auf leer und Ablaufzeit auf 0, um sie zu löschen
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
