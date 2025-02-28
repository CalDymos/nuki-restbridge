#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <functional>

#define LANEvent_t arduino_event_id_t

class NetworkManager {
public:
    using DataReceivedCallback = std::function<void(const String&, WebServer&)>;

    NetworkManager();
    void begin(const char* ssid = nullptr, const char* password = nullptr);
    bool isConnected() const;
    void reconnect();
    String getIPAddress() const;
    
    void startServer();
    void stopServer();
    void handleClient();
    void setDataReceivedCallback(DataReceivedCallback callback);

    int sendHttpRequest(const String& url, const String& payload = "", const String& method = "GET");

private:
    static void LANEventHandler(LANEvent_t event);
    void setupWiFi(const char* ssid, const char* password);
    void setupEthernet();
    void handleRequest();  // Neue Methode für HTTP-Requests

    bool connected;
    WebServer server;
    DataReceivedCallback dataReceivedCallback;
};

#endif
