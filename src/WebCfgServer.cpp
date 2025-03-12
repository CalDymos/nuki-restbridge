#include <Arduino.h>
#include <ESPmDNS.h>
#include "WebCfgServer.h"
#include "Logger.hpp"
#include "PreferencesKeys.h"
#include "RestartReason.h"

const char css[] PROGMEM = ":root{--nc-font-sans:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,'Open Sans','Helvetica Neue',sans-serif,'Apple Color Emoji','Segoe UI Emoji','Segoe UI Symbol';--nc-font-mono:Consolas,monaco,'Ubuntu Mono','Liberation Mono','Courier New',Courier,monospace;--nc-tx-1:#000;--nc-tx-2:#1a1a1a;--nc-bg-1:#fff;--nc-bg-2:#f6f8fa;--nc-bg-3:#e5e7eb;--nc-lk-1:#0070f3;--nc-lk-2:#0366d6;--nc-lk-tx:#fff;--nc-ac-1:#79ffe1;--nc-ac-tx:#0c4047}@media(prefers-color-scheme:dark){:root{--nc-tx-1:#fff;--nc-tx-2:#eee;--nc-bg-1:#000;--nc-bg-2:#111;--nc-bg-3:#222;--nc-lk-1:#3291ff;--nc-lk-2:#0070f3;--nc-lk-tx:#fff;--nc-ac-1:#7928ca;--nc-ac-tx:#fff}}*{margin:0;padding:0}img,input,option,p,table,textarea,ul{margin-bottom:1rem}button,html,input,select{font-family:var(--nc-font-sans)}body{margin:0 auto;max-width:750px;padding:2rem;border-radius:6px;overflow-x:hidden;word-break:normal;overflow-wrap:anywhere;background:var(--nc-bg-1);color:var(--nc-tx-2);font-size:1.03rem;line-height:1.5}::selection{background:var(--nc-ac-1);color:var(--nc-ac-tx)}h1,h2,h3,h4,h5,h6{line-height:1;color:var(--nc-tx-1);padding-top:.875rem}h1,h2,h3{color:var(--nc-tx-1);padding-bottom:2px;margin-bottom:8px;border-bottom:1px solid var(--nc-bg-2)}h4,h5,h6{margin-bottom:.3rem}h1{font-size:2.25rem}h2{font-size:1.85rem}h3{font-size:1.55rem}h4{font-size:1.25rem}h5{font-size:1rem}h6{font-size:.875rem}a{color:var(--nc-lk-1)}a:hover{color:var(--nc-lk-2) !important;}abbr{cursor:help}abbr:hover{cursor:help}a button,button,input[type=button],input[type=reset],input[type=submit]{font-size:1rem;display:inline-block;padding:6px 12px;text-align:center;text-decoration:none;white-space:nowrap;background:var(--nc-lk-1);color:var(--nc-lk-tx);border:0;border-radius:4px;box-sizing:border-box;cursor:pointer;color:var(--nc-lk-tx)}a button[disabled],button[disabled],input[type=button][disabled],input[type=reset][disabled],input[type=submit][disabled]{cursor:default;opacity:.5;cursor:not-allowed}.button:focus,.button:hover,button:focus,button:hover,input[type=button]:focus,input[type=button]:hover,input[type=reset]:focus,input[type=reset]:hover,input[type=submit]:focus,input[type=submit]:hover{background:var(--nc-lk-2)}table{border-collapse:collapse;width:100%}td,th{border:1px solid var(--nc-bg-3);text-align:left;padding:.5rem}th{background:var(--nc-bg-2)}tr:nth-child(even){background:var(--nc-bg-2)}textarea{max-width:100%}input,select,textarea{padding:6px 12px;margin-bottom:.5rem;background:var(--nc-bg-2);color:var(--nc-tx-2);border:1px solid var(--nc-bg-3);border-radius:4px;box-shadow:none;box-sizing:border-box}img{max-width:100%}td>input{margin-top:0;margin-bottom:0}td>textarea{margin-top:0;margin-bottom:0}td>select{margin-top:0;margin-bottom:0}.warning{color:red}@media only screen and (max-width:600px){.adapt td{display:block}.adapt input[type=text],.adapt input[type=password],.adapt input[type=submit],.adapt textarea,.adapt select{width:100%}.adapt td:has(input[type=checkbox]){text-align:center}.adapt input[type=checkbox]{width:1.5em;height:1.5em}.adapt table td:first-child{border-bottom:0}.adapt table td:last-child{border-top:0}#tblnav a li>span{max-width:140px}}#tblnav a{border:0;border-bottom:1px solid;display:block;font-size:1rem;font-weight:bold;padding:.6rem 0;line-height:1;color:var(--nc-tx-1);text-decoration:none;background:linear-gradient(to left,transparent 50%,rgba(255,255,255,0.4) 50%) right;background-size:200% 100%;transition:all .2s ease}#tblnav a{background:linear-gradient(to left,var(--nc-bg-2) 50%,rgba(255,255,255,0.4) 50%) right;background-size:200% 100%}#tblnav a:hover{background-position:left;transition:all .45s ease}#tblnav a:active{background:var(--nc-lk-1);transition:all .15s ease}#tblnav a li{list-style:none;padding:.5rem;display:inline-block;width:100%}#tblnav a li>span{float:right;text-align:right;margin-right:10px;color:#f70;font-weight:100;font-style:italic;display:block}.tdbtn{text-align:center;vertical-align:middle}.naventry{float:left;max-width:375px;width:100%}";

// Konstruktor: Port + Preferences übernehmen
WebCfgServer::WebCfgServer(NukiWrapper *nuki, NukiNetwork *network, Preferences *preferences)
    : _nuki(nuki),
      _network(network),
      _preferences(preferences)

{
    _webServer = new WebServer(WEBCFGSERVER_PORT);
    _hostname = _preferences->getString(preference_hostname, "");
    String str = _preferences->getString(preference_webcfgserver_cred_user, "");
    str = _preferences->getString(preference_webcfgserver_cred_user, "");

    if (str.length() > 0)
    {
        memset(&_credUser, 0, sizeof(_credUser));
        memset(&_credPassword, 0, sizeof(_credPassword));

        const char *user = str.c_str();
        memcpy(&_credUser, user, str.length());

        str = _preferences->getString(preference_webcfgserver_cred_password, "");
        const char *pass = str.c_str();
        memcpy(&_credPassword, pass, str.length());
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
    while(esp_timer_get_time() < timeout)
    {
        if(blocking)
        {
            delay(10);
        }
        else
        {
            vTaskDelay( 50 / portTICK_PERIOD_MS);
        }
    }
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

bool WebCfgServer::processWiFi(WebServer *server, String& message)
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
            const IPConfiguration* _ipConfiguration = new IPConfiguration(_preferences);

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
