#include <Arduino.h>
#include <ESPmDNS.h>
#include "WebCfgServer.h"
#include "Logger.hpp"
#include "PreferencesKeys.h"
#include "RestartReason.h"
#include "NetworkDeviceType.h"

const char css[] PROGMEM = ":root{--nc-font-sans:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,'Open Sans','Helvetica Neue',sans-serif,'Apple Color Emoji','Segoe UI Emoji','Segoe UI Symbol';--nc-font-mono:Consolas,monaco,'Ubuntu Mono','Liberation Mono','Courier New',Courier,monospace;--nc-tx-1:#000;--nc-tx-2:#1a1a1a;--nc-bg-1:#fff;--nc-bg-2:#f6f8fa;--nc-bg-3:#e5e7eb;--nc-lk-1:#0070f3;--nc-lk-2:#0366d6;--nc-lk-tx:#fff;--nc-ac-1:#79ffe1;--nc-ac-tx:#0c4047}@media(prefers-color-scheme:dark){:root{--nc-tx-1:#fff;--nc-tx-2:#eee;--nc-bg-1:#000;--nc-bg-2:#111;--nc-bg-3:#222;--nc-lk-1:#3291ff;--nc-lk-2:#0070f3;--nc-lk-tx:#fff;--nc-ac-1:#7928ca;--nc-ac-tx:#fff}}*{margin:0;padding:0}img,input,option,p,table,textarea,ul{margin-bottom:1rem}button,html,input,select{font-family:var(--nc-font-sans)}body{margin:0 auto;max-width:750px;padding:2rem;border-radius:6px;overflow-x:hidden;word-break:normal;overflow-wrap:anywhere;background:var(--nc-bg-1);color:var(--nc-tx-2);font-size:1.03rem;line-height:1.5}::selection{background:var(--nc-ac-1);color:var(--nc-ac-tx)}h1,h2,h3,h4,h5,h6{line-height:1;color:var(--nc-tx-1);padding-top:.875rem}h1,h2,h3{color:var(--nc-tx-1);padding-bottom:2px;margin-bottom:8px;border-bottom:1px solid var(--nc-bg-2)}h4,h5,h6{margin-bottom:.3rem}h1{font-size:2.25rem}h2{font-size:1.85rem}h3{font-size:1.55rem}h4{font-size:1.25rem}h5{font-size:1rem}h6{font-size:.875rem}a{color:var(--nc-lk-1)}a:hover{color:var(--nc-lk-2) !important;}abbr{cursor:help}abbr:hover{cursor:help}a button,button,input[type=button],input[type=reset],input[type=submit]{font-size:1rem;display:inline-block;padding:6px 12px;text-align:center;text-decoration:none;white-space:nowrap;background:var(--nc-lk-1);color:var(--nc-lk-tx);border:0;border-radius:4px;box-sizing:border-box;cursor:pointer;color:var(--nc-lk-tx)}a button[disabled],button[disabled],input[type=button][disabled],input[type=reset][disabled],input[type=submit][disabled]{cursor:default;opacity:.5;cursor:not-allowed}.button:focus,.button:hover,button:focus,button:hover,input[type=button]:focus,input[type=button]:hover,input[type=reset]:focus,input[type=reset]:hover,input[type=submit]:focus,input[type=submit]:hover{background:var(--nc-lk-2)}table{border-collapse:collapse;width:100%}td,th{border:1px solid var(--nc-bg-3);text-align:left;padding:.5rem}th{background:var(--nc-bg-2)}tr:nth-child(even){background:var(--nc-bg-2)}textarea{max-width:100%}input,select,textarea{padding:6px 12px;margin-bottom:.5rem;background:var(--nc-bg-2);color:var(--nc-tx-2);border:1px solid var(--nc-bg-3);border-radius:4px;box-shadow:none;box-sizing:border-box}img{max-width:100%}td>input{margin-top:0;margin-bottom:0}td>textarea{margin-top:0;margin-bottom:0}td>select{margin-top:0;margin-bottom:0}.warning{color:red}@media only screen and (max-width:600px){.adapt td{display:block}.adapt input[type=text],.adapt input[type=password],.adapt input[type=submit],.adapt textarea,.adapt select{width:100%}.adapt td:has(input[type=checkbox]){text-align:center}.adapt input[type=checkbox]{width:1.5em;height:1.5em}.adapt table td:first-child{border-bottom:0}.adapt table td:last-child{border-top:0}#tblnav a li>span{max-width:140px}}#tblnav a{border:0;border-bottom:1px solid;display:block;font-size:1rem;font-weight:bold;padding:.6rem 0;line-height:1;color:var(--nc-tx-1);text-decoration:none;background:linear-gradient(to left,transparent 50%,rgba(255,255,255,0.4) 50%) right;background-size:200% 100%;transition:all .2s ease}#tblnav a{background:linear-gradient(to left,var(--nc-bg-2) 50%,rgba(255,255,255,0.4) 50%) right;background-size:200% 100%}#tblnav a:hover{background-position:left;transition:all .45s ease}#tblnav a:active{background:var(--nc-lk-1);transition:all .15s ease}#tblnav a li{list-style:none;padding:.5rem;display:inline-block;width:100%}#tblnav a li>span{float:right;text-align:right;margin-right:10px;color:#f70;font-weight:100;font-style:italic;display:block}.tdbtn{text-align:center;vertical-align:middle}.naventry{float:left;max-width:375px;width:100%}";
extern bool timeSynced;

// Konstruktor: Port + Preferences übernehmen
WebCfgServer::WebCfgServer(NukiWrapper *nuki, NukiNetwork *network, Preferences *preferences)
    : _nuki(nuki),
      _network(network),
      _preferences(preferences)

{
    _webServer = new WebServer(WEBCFGSERVER_PORT);
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
}

bool WebCfgServer::isAuthenticated(WebServer *server)
{
    String cookieKey = "sessionId";

    // Hole alle Cookies als String
    String cookieHeader = server->header("Cookie");

    if (cookieHeader.length() > 0)
    {
        // Suche den Cookie-Wert
        int startIndex = cookieHeader.indexOf(cookieKey + "=");
        if (startIndex != -1)
        {
            startIndex += cookieKey.length() + 1; // Position nach dem Gleichheitszeichen
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
                    Log->println("Cookie found, but not valid anymore");
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
                Log->println("Authentication Failed");
                return savedAuthType;
            }
        }
        else
        {
            if (!server->authenticate(_credUser, _credPassword))
            {
                Log->println("Authentication Failed");
                return savedAuthType;
            }
        }
    }

    return 4;
}

void WebCfgServer::initialize()
{

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
            if(value == "export" && timeSynced && this->_webServer->hasArg("adminkey"))
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

            if (!adminKeyValid && value != "status" && value != "login" && value != "duocheck" && value != "bypass")
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
                //return buildCredHtml(this->_webServer);
            }
            else if (value == "ntwconfig")
            {
                //return buildNetworkConfigHtml(this->_webServer);
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
                //return buildNukiConfigHtml(this->_webServer);
            }
            else if (value == "wifi")
            {
                //return buildConfigureWifiHtml(this->_webServer);
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
                buildConfirmHtml(this->_webServer, "Restarting. Connect to ESP access point (\"NukiHub\" with password \"NukiHubESP32\") to reconfigure Wi-Fi.", 0);
                waitAndProcess(false, 1000);
                _network->reconfigure();
                return;
            }
            else
            {
                Log->println("Page not found, loading index");
                this->_webServer->sendHeader("Cache-Control",  "no-cache");
                this->redirect(this->_webServer, "/", 302);
                return;
            } });
    }

    // Eigentliches Starten des Webservers
    _webServer->begin();
    Log->println("WebCfgServer started on port: " + String(WEBCFGSERVER_PORT));

    if (MDNS.begin(_preferences->getString(preference_hostname, "nukibridge").c_str()))
    {
        MDNS.addService("http", "tcp", WEBCFGSERVER_PORT);
    }
}

void WebCfgServer::redirect(WebServer *server, const char *url, int code)
{
    _webServer->sendHeader("Location", url, true); // HTTP Redirect
    _webServer->send(code, "text/plain", "Redirecting to /");
}

void WebCfgServer::handleClient()
{
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

void WebCfgServer::buildBypassHtml(WebServer *server)
{
    if (timeSynced)
    {
        buildConfirmHtml(server, "One-time bypass is only available if NTP time is not synced</a>", 3, true, "/");
        return;
    }

    String response = "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    response += "<style>form{border:3px solid #f1f1f1; max-width: 400px;}input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;box-sizing:border-box}button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;border:none;cursor:pointer;width:100%}button:hover{opacity:.8}.container{padding:16px}span.password{float:right;padding-top:16px}@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>";
    response += "</head><body><center><h2>Nuki Hub One-time Bypass</h2>";
    response += "<form action=\"/post?page=bypass\" method=\"post\">";
    response += "<div class=\"container\">";
    response += "<label for=\"bypass\"><b>Bypass code</b></label><input type=\"text\" placeholder=\"Enter bypass code\" name=\"bypass\">";
    response += "<button type=\"submit\">Login</button></div>";
    response += "</form></center></body></html>";

    server->send(200, "text/html", response);
}

void WebCfgServer::buildLoginHtml(WebServer *server)
{
    String response = "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    response += "<style>form{border:3px solid #f1f1f1; max-width: 400px;}input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;box-sizing:border-box}button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;border:none;cursor:pointer;width:100%}button:hover{opacity:.8}.container{padding:16px}span.password{float:right;padding-top:16px}@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>";
    response += "</head><body><center><h2>Nuki Hub login</h2><form action=\"/post?page=login\" method=\"post\">";
    response += "<div class=\"container\"><label for=\"username\"><b>Username</b></label><input type=\"text\" placeholder=\"Enter Username\" name=\"username\" required>";
    response += "<label for=\"password\"><b>Password</b></label><input type=\"password\" placeholder=\"Enter Password\" name=\"password\" required>";

    response += "<button type=\"submit\">Login</button>";

    response += "<label><input type=\"checkbox\" name=\"remember\"> Remember me</label></div>";
    response += "</form></center></body></html>";

    server->send(200, "text/html", response);
}

void WebCfgServer::buildConfirmHtml(WebServer *server, const String &message, uint32_t redirectDelay, bool redirect, String redirectTo)
{
    String response = "<html><head>";
    String header;

    if (!redirect)
    {
        header = "<meta http-equiv=\"Refresh\" content=\"" + String(redirectDelay) + "; url=/\" />";
    }
    else
    {
        header = "<script type=\"text/JavaScript\">function Redirect() { window.location.href = \"" + redirectTo + "\"; } setTimeout(function() { Redirect(); }, " + String(redirectDelay * 1000) + "); </script>";
    }

    response += header;
    response += "<title>Confirmation</title></head><body>";
    response += message;
    response += "</body></html>";

    server->send(200, "text/html", response);
}

void WebCfgServer::buildCoredumpHtml(WebServer *server)
{
    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        File file = SPIFFS.open("/coredump.hex", "r");

        if (!file || file.isDirectory())
        {
            Log->println("coredump.hex not found");
        }
        else
        {
            server->sendHeader("Content-Disposition", "attachment; filename=\"coredump.txt\"");
            server->streamFile(file, "application/octet-stream");
            file.close();
            return;
        }
    }

    server->sendHeader("Cache-Control", "no-cache");
    server->sendHeader("Location", "/");
    server->send(302, "text/plain", "");
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

void WebCfgServer::buildSSIDListHtml(WebServer *server)
{
    _network->scan(true, false);
    createSsidList();

    String response = "<html><body><table>";

    for (size_t i = 0; i < _ssidList.size(); i++)
    {
        response += "<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '" + _ssidList[i] + "';\">";
        response += "<td colspan=\"2\">" + _ssidList[i] + " (" + String(_rssiList[i]) + " %)</td></tr>";
    }

    response += "</table></body></html>";

    server->send(200, "text/html", response);
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

void WebCfgServer::createSsidList()
{
    int _foundNetworks = WiFi.scanComplete();
    std::vector<String> _tmpSsidList;
    std::vector<int> _tmpRssiList;

    for (int i = 0; i < _foundNetworks; i++)
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

void WebCfgServer::logoutSession(WebServer *server)
{
    Log->println("Logging out");

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
        Log->println("No session cookie found");
    }

    buildConfirmHtml(server, "Logging out", 3, true, "/");
}

void WebCfgServer::saveSessions()
{
    if (_preferences->getBool(preference_update_time, false))
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
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
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file;

            file = SPIFFS.open("/sessions.json", "r");

            if (!file || file.isDirectory())
            {
                Log->println("sessions.json not found");
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
        Log->println("SPIFFS Mount Failed");
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
