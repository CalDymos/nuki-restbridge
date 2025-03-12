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

void WebCfgServer::initialize()
{
    // Route für GET-Anfrage auf "/" (z.B. Einstellungsformular)
    _webServer->on("/", HTTP_GET, [this]()
                   { handleRoot(); });

    // Route für POST-Anfrage auf "/save" (z.B. zum Speichern der Settings)
    _webServer->on("/save", HTTP_POST, [this]()
                   { handleSave(); });

    // Route für css Style 
    _webServer->on("/style.css", [this]() { handleCSS();});

    _webServer->onNotFound([this]()
                           { handleNotFound(); }); // 404-Handler setzen

    // Eigentliches Starten des Webservers
    _webServer->begin();
    Log->println("WebCfgServer started on port: " + String(WEBCFGSERVER_PORT));

    if (MDNS.begin(_preferences->getString(preference_hostname, "nukibridge").c_str()))
    {
        MDNS.addService("http", "tcp", WEBCFGSERVER_PORT);
    }
}

void WebCfgServer::handleNotFound()
{
    _webServer->sendHeader("Location", "/", true); // HTTP Redirect
    _webServer->send(302, "text/plain", "Redirecting to /");
}

void WebCfgServer::handleClient()
{
    _webServer->handleClient();
}

void WebCfgServer::handleRoot()
{
    // Einfaches HTML-Formular zum Setzen eines Preferences-Schlüssels
    String html =
        "<!DOCTYPE html>"
        "<html>"
        "<head><meta charset='utf-8'/><title>WebCfgServer</title></head>"
        "<body>"
        "<h2>WebCfgServer – Einstellungen</h2>"
        "<form method='POST' action='/save'>"
        "<label for='key'>Schlüssel:</label><br>"
        "<input type='text' id='key' name='key' value='TestKey'/><br><br>"
        "<label for='val'>Wert:</label><br>"
        "<input type='text' id='val' name='val' value='HelloWorld'/><br><br>"
        "<input type='submit' value='Speichern'/>"
        "</form>"
        "</body></html>";

    _webServer->send(200, "text/html", html);
}

void WebCfgServer::handleSave()
{
    // Hier Werte aus dem POST-Request auslesen
    if (_webServer->hasArg("key") && _webServer->hasArg("val"))
    {
        String key = _webServer->arg("key");
        String val = _webServer->arg("val");

        // In Preferences schreiben (Namespace muss außerhalb aufgerufen sein, z.B. preferences.begin("myNamespace", false);)
        _preferences->putString(key.c_str(), val);

        // Einfach eine Seite ausgeben, die bestätigt, was gespeichert wurde
        String html =
            "<!DOCTYPE html>"
            "<html>"
            "<head><meta charset='utf-8'/><title>Saved</title></head>"
            "<body>"
            "<h2>Gespeichert!</h2>"
            "<p>Key: " +
            key + "</p>"
                  "<p>Value: " +
            val + "</p>"
                  "<a href='/'>Zur&uuml;ck</a>"
                  "</body></html>";

        _webServer->send(200, "text/html", html);
    }
    else
    {
        _webServer->send(400, "text/plain", "Fehler: Bitte Key und Value angeben!");
    }
}

void WebCfgServer::handleCSS()
{
    _webServer->send(200, "text/css", css);
}
