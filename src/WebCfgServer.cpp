#include <Arduino.h>
#include <ESPmDNS.h>
#include "WebCfgServer.h"
#include "Logger.hpp"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "NetworkDeviceType.h"

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
                                           this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                                            this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                                    this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                    this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                    this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                //return buildAccLvlHtml(this->_webServer);
            }
            else if (value == "advanced")
            {
                //return buildAdvancedConfigHtml(this->_webServer);
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
                //return buildApiConfigHtml(this->_webServer);
            }
            else if (value == "harconfig")
            {
                //return buildHARConfigHtml(this->_webServer);
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
                    this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                        this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                        this->_webServer->sendHeader("Cache-Control",  "no-cache");
                        return this->redirect(this->_webServer, "/", 302);
                    }
                    else
                    {
                        this->_webServer->sendHeader("Cache-Control",  "no-cache");
                        return this->redirect(this->_webServer, "/get?page=login", 302);
                    }
                }
                else if (value == "bypass")
                {
                    bool loggedIn = processBypass(this->_webServer);
                    if (loggedIn)
                    {
                        this->_webServer->sendHeader("Cache-Control",  "no-cache");
                        _newBypass = true;
                        return this->redirect(this->_webServer, "/get?page=newbypass", 302);
                    }
                    else
                    {
                        this->_webServer->sendHeader("Cache-Control",  "no-cache");
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
                           this->_webServer->sendHeader("Cache-Control", "no-cache");
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
        Log->println("[ERROR] WebServer instance is NULL! Aborting handleClient.");
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
    response.reserve(8192); // Speicher reservieren, um Fragmentierung zu reduzieren

    buildHtmlHeader(response);

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    response += F("<form method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<input type=\"hidden\" name=\"ACLLVLCHANGED\" value=\"1\">");
    response += F("<h3>Nuki General Access Control</h3><table><tr><th>Setting</th><th>Enabled</th></tr>");

    appendCheckBoxRow(response, "CONFNHPUB", "Report Nuki Hub configuration information to Home Automation", _preferences->getBool(preference_publish_config, false), "");
    appendCheckBoxRow(response, "CONFNHCTRL", "Modify Nuki Hub configuration over REST API", _preferences->getBool(preference_config_from_api, false), "");
    appendCheckBoxRow(response, "CONFPUB", "Report Nuki configuration information to Home Automation", _preferences->getBool(preference_conf_info_enabled, true), "");

    if ((_nuki && _nuki->hasKeypad()))
    {
        appendCheckBoxRow(response, "KPPUB", "Report keypad entries information to Home Automation", _preferences->getBool(preference_keypad_info_enabled), "");
        appendCheckBoxRow(response, "KPENA", "Add, modify and delete keypad codes", _preferences->getBool(preference_keypad_control_enabled), "");
    }

    appendCheckBoxRow(response, "TCPUB", "Report time control entries information to Home Automation", _preferences->getBool(preference_timecontrol_info_enabled), "");
    appendCheckBoxRow(response, "AUTHPUB", "Report authorization entries information to Home Automation", _preferences->getBool(preference_auth_info_enabled), "");

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
    response.reserve(4096); // Speicher reservieren, um Fragmentierung zu reduzieren

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
    response.reserve(8192);

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
    appendCheckBoxRow(response, "DBGHEAP", "Send free heap to Home Automation", _preferences->getBool(preference_publish_debug_info, false), "");

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

    server->send(200, "text/html", response);
}

void WebCfgServer::buildLoginHtml(WebServer *server)
{
    String response;
    response.reserve(2048);

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
    response.reserve(1024);

    String header;
    if (!redirect)
    {
        header = "<meta http-equiv=\"Refresh\" content=\"" + String(redirectDelay) + "; url=/\" />";
    }
    else
    {
        header = "<script type=\"text/JavaScript\">function Redirect() { window.location.href = \"" + redirectTo + "\"; } setTimeout(function() { Redirect(); }, " + String(redirectDelay * 1000) + "); </script>";
    }

    buildHtmlHeader(response, header);
    response += message;
    response += "</body></html>";

    server->send(200, "text/html", response);
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
    response.reserve(4096);

    buildHtmlHeader(response);
    response += F("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response += F("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response += F("<h3>Network Configuration</h3>");
    response += F("<table>");

    appendInputFieldRow(response, "HOSTNAME", "Host name", _preferences->getString(preference_hostname).c_str(), 100, "");
    appendDropDownRow(response, "NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions(), "");

#ifndef CONFIG_IDF_TARGET_ESP32H2
    appendInputFieldRow(response, "RSSI", "RSSI Publish interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_publish_interval), 6, "");
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
    response.reserve(1024);

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
    response.reserve(8192);

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

void WebCfgServer::buildHtml(WebServer *server)
{
    String header = F("<script>let intervalId; window.onload = function() { updateInfo(); intervalId = setInterval(updateInfo, 3000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/get?page=status', true); request.onload = () => { const obj = JSON.parse(request.responseText); if (obj.stop == 1) { clearInterval(intervalId); } for (var key of Object.keys(obj)) { if(key=='ota' && document.getElementById(key) !== null) { document.getElementById(key).innerText = \"<a href='/ota'>\" + obj[key] + \"</a>\"; } else if(document.getElementById(key) !== null) { document.getElementById(key).innerText = obj[key]; } } }; request.send(); }</script>");

    String response;
    response.reserve(8192);

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

    buildNavigationMenuEntry(response, "Network Configuration", "/get?page=ntwconfig");
    buildNavigationMenuEntry(response, "REST API Configuration", "/get?page=apiconfig", _APIConfigured ? "" : "Please configure REST API");
    buildNavigationMenuEntry(response, "Home Automation Configuration", "/get?page=harconfig", _HAConfigured ? "" : "Please configure Home Automation");
    buildNavigationMenuEntry(response, "Nuki Configuration", "/get?page=nukicfg");
    buildNavigationMenuEntry(response, "Access Level Configuration", "/get?page=acclvl");
    buildNavigationMenuEntry(response, "Credentials", "/get?page=cred");

    if (_preferences->getInt(preference_network_hardware, 0) == 11)
    {
        buildNavigationMenuEntry(response, "Custom Ethernet Configuration", "/get?page=custntw");
    }

    if (_preferences->getBool(preference_enable_debug_mode, false))
    {
        buildNavigationMenuEntry(response, "Advanced Configuration", "/get?page=advanced");
    }

#ifndef CONFIG_IDF_TARGET_ESP32H2
    if (_allowRestartToPortal)
    {
        buildNavigationMenuEntry(response, "Configure Wi-Fi", "/get?page=wifi");
    }
#endif

    buildNavigationMenuEntry(response, "Info page", "/get?page=info");
    String rebooturl = "/get?page=reboot&CONFIRMTOKEN=" + _confirmCode;
    buildNavigationMenuEntry(response, "Reboot Nuki Hub", rebooturl.c_str());

    if (_preferences->getInt(preference_http_auth_type, 0) == 2)
    {
        buildNavigationMenuEntry(response, "Logout", "/get?page=logout");
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
    response.reserve(2048);

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

    server->send(200, "text/html", response);
}

#ifndef CONFIG_IDF_TARGET_ESP32H2
void WebCfgServer::buildSSIDListHtml(WebServer *server)
{
    _network->scan(true, false);
    createSsidList();

    String response;
    response.reserve(2048);

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
    response.reserve(8192); 

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

    String response = "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body>";
    response += "<h3>System Information</h3><pre>";
    response += "------------ NUKI HUB ------------\n";
    response += "Device: " + String(NUKI_REST_BRIDGE_HW) + "\n";
    response += "Version: " + String(NUKI_REST_BRIDGE_VERSION) + "\n";
    response += "Build: " + String(NUKI_REST_BRIDGE_BUILD) + "\n";
#ifndef DEBUG
    response += "Build type: Release\n";
#else
    response += "Build type: Debug\n";
#endif
    response += "Build date: " + String(NUKI_REST_BRIDGE_DATE) + "\n";
    response += "Uptime (min): " + String(espMillis() / 1000 / 60) + "\n";
    response += "Last restart reason FW: " + getRestartReason() + "\n";
    response += "Last restart reason ESP: " + getEspRestartReason() + "\n";
    response += "Free internal heap: " + String(ESP.getFreeHeap()) + "\n";
    response += "Total internal heap: " + String(ESP.getHeapSize()) + "\n";

    response += "\nNetwork task stack high watermark: " + String(uxTaskGetStackHighWaterMark(networkTaskHandle)) + "\n";
    response += "Nuki task stack high watermark: " + String(uxTaskGetStackHighWaterMark(nukiTaskHandle)) + "\n";
    response += "Web configurator task stack high watermark: " + String(uxTaskGetStackHighWaterMark(webCfgTaskHandle)) + "\n";

    // SPIFFS Info
    SPIFFS.begin(true);
    response += "\n------------ SPIFFS ------------\n";
    response += "SPIFFS Total Bytes: " + String(SPIFFS.totalBytes()) + "\n";
    response += "SPIFFS Used Bytes: " + String(SPIFFS.usedBytes()) + "\n";
    response += "SPIFFS Free Bytes: " + String(SPIFFS.totalBytes() - SPIFFS.usedBytes()) + "\n";

    response += "\n------------ GENERAL SETTINGS ------------\n";
    response += "Web configurator enabled: " + String(_preferences->getBool(preference_webcfgserver_enabled, true) ? "Yes" : "No") + "\n";
    response += "Web configurator username: " + String(_preferences->getString(preference_cred_user, "").length() > 0 ? "***" : "Not set") + "\n";
    response += "Web configurator password: " + String(_preferences->getString(preference_cred_password, "").length() > 0 ? "***" : "Not set") + "\n";
    response += "Web configurator bypass for proxy IP: " + String(_preferences->getString(preference_bypass_proxy, "").length() > 0 ? "***" : "Not set") + "\n";
    response += "Web configurator authentication: " + String(_preferences->getInt(preference_http_auth_type, 0) == 0 ? "Basic" : _preferences->getInt(preference_http_auth_type, 0) == 1 ? "Digest"
                                                                                                                                                                                         : "Form") +
                "\n";
    response += "Update Nuki Hub and Nuki devices time using NTP: " + String(_preferences->getBool(preference_update_time, false) ? "Yes" : "No") + "\n";
    response += "Session validity (in seconds): " + String(_preferences->getInt(preference_cred_session_lifetime, 3600)) + "\n";
    response += "Session validity remember (in hours): " + String(_preferences->getInt(preference_cred_session_lifetime_remember, 720)) + "\n";

    // Netzwerk-Infos
    response += "\n------------ NETWORK ------------\n";
    if (_network->networkDeviceType() == NetworkDeviceType::ETH)
        devType = "LAN";
    else if (_network->networkDeviceType() == NetworkDeviceType::WiFi)
        devType = "WLAN";
    else
        devType = "Unbekannt";
    response += "Network device: " + devType + "\n";
    response += "Network connected: " + String(_network->isConnected() ? "Yes" : "No") + "\n";
    if (_network->isConnected())
    {
        response += "IP Address: " + _network->localIP() + "\n";
        if (devType == "WLAN")
        {
            response += "SSID: " + WiFi.SSID() + "\n";
            response += "ESP32 MAC address: " + WiFi.macAddress() + "\n";
        }
    }

    response += "\n------------ NETWORK SETTINGS ------------\n";
    response += "Nuki Hub hostname: " + _preferences->getString(preference_hostname, "") + "\n";
    if (_preferences->getBool(preference_ip_dhcp_enabled, true))
    {
        response += "DHCP enabled: Yes\n";
    }
    else
    {
        response += "DHCP enabled: No\n";
        response += "Static IP address: " + _preferences->getString(preference_ip_address, "") + "\n";
        response += "Static IP subnet: " + _preferences->getString(preference_ip_subnet, "") + "\n";
        response += "Static IP gateway: " + _preferences->getString(preference_ip_gateway, "") + "\n";
        response += "Static IP DNS server: " + _preferences->getString(preference_ip_dns_server, "") + "\n";
    }

    if (devType == "WLAN")
    {
        response += "RSSI Publish interval (s): ";

        if (_preferences->getInt(preference_rssi_publish_interval, 60) < 0)
        {
            response += "Disabled\n";
        }
        else
        {
            response += String(_preferences->getInt(preference_rssi_publish_interval, 60)) + "\n";
        }

        response += "Find WiFi AP with strongest signal: " + String(_preferences->getBool(preference_find_best_rssi, false) ? "Yes" : "No") + "\n";
    }
    response += "Restart ESP32 on network disconnect enabled: " + String(_preferences->getBool(preference_restart_on_disconnect, false) ? "Yes" : "No") + "\n";
    response += "Disable Network if not connected within 60s: " + String(_preferences->getBool(preference_disable_network_not_connected, false) ? "Yes" : "No") + "\n";

    // REST Api Infos
    response += "\n------------ REST Api ------------\n";
    response += "API enabled: " + String(_preferences->getBool(preference_api_enabled, false) != false ? "Yes" : "No") + "\n";
    response += "API connected: " + String((_network->networkServicesState() == NetworkServiceStates::OK || _network->networkServicesState() == NetworkServiceStates::HTTPCLIENT_NOT_REACHABLE) ? "Yes" : "No") + "\n";
    response += "API Port: " + String(_preferences->getInt(preference_api_port, 0)) + "\n";
    response += "API auth token: " + _preferences->getString(preference_api_Token, "not defined") + "\n";

    // HomeAutomation
    response += "\n------------ Home Automation Reporting ------------\n";
    response += "HAR enabled: " + String(_preferences->getBool(preference_ha_enabled, false) != false ? "Yes" : "No") + "\n";
    response += "HA reachable: " + String((_network->networkServicesState() == NetworkServiceStates::OK || _network->networkServicesState() == NetworkServiceStates::WEBSERVER_NOT_REACHABLE) ? "Yes" : "No") + "\n";
    response += "HAR address: " + _preferences->getString(preference_ha_address, "not defined") + "\n";
    response += "HAR user: " + _preferences->getString(preference_ha_user, "not defined") + "\n";
    response += "HAR password: " + _preferences->getString(preference_ha_password, "not defined") + "\n";
    response += "HAR mode: " + _preferences->getString(preference_ha_mode, "not defined") + "\n";

    // Bluetooth Infos
    response += "\n------------ BLUETOOTH ------------\n";
    response += "Bluetooth connection mode: " + String(_preferences->getBool(preference_connect_mode, true) ? "New" : "Old") + "\n";
    response += "Bluetooth TX power (dB): " + String(_preferences->getInt(preference_ble_tx_power, 9)) + "\n";

    // Nuki Lock Infos
    response += "\n------------ NUKI LOCK ------------\n";
    if (_nuki == nullptr || !_preferences->getBool(preference_lock_enabled, true))
    {
        response += "Lock enabled: No\n";
    }
    else
    {
        response += "Lock enabled: Yes\n";
        response += "Paired: " + String(_nuki->isPaired() ? "Yes" : "No") + "\n";
        response += "Firmware version: " + _nuki->firmwareVersion() + "\n";
    }

    response += "</pre></body></html>";

    server->send(200, "text/html", response);
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

void WebCfgServer::buildNavigationMenuEntry(String &response, const char *title, const char *targetPath, const char *warningMessage)
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
    server->sendHeader("Cache-Control", "public, max-age=3600");

    // Setze den Content-Type auf text/css
    server->setContentLength(CONTENT_LENGTH_UNKNOWN); // Länge muss nicht im Voraus bekannt sein
    server->send(200, "text/css", css);               // Antwortstatus 200 und Content-Type "text/css"
}

String WebCfgServer::generateConfirmCode()
{
    int code = random(1000, 9999);
    return String(code);
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
