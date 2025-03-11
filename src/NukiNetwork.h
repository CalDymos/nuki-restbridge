// Handles the general network communication of the Nuki Bridge
#pragma once

#ifndef NUKI_NETWORK_H
#define NUKI_NETWORK_H

// Standard libraries
#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "ESP32Ping.h"
#include <functional>
#include "ArduinoJson.h"

// Project-specific and ESP32 headers
#include "RestDataReceiver.h"
#include "RestApiPaths.h"
#include "IPConfiguration.h"
#include "NetworkDeviceType.h"
#include "BridgeApiToken.h"

class NukiNetwork {
public:
  // Constructor
  explicit NukiNetwork(Preferences* preferences, char* buffer, size_t bufferSize);

  // Initialization & configuration
  void initialize();
  void reconfigure();
  bool update();

  // Network status & properties
  bool isApOpen() const;
  bool isConnected() const;
  bool isWifiConnected();
  const String localIP() const;
  const String networkBSSID() const;
  NetworkDeviceType networkDeviceType();
  int8_t signalStrength();
  bool isWifiConfigured() const;
  void clearWifiFallback();
  void disableAutoRestarts();
  int NetworkServicesState();
  bool NetworkServicesRecentlyConnected();

  void registerRestDataReceiver(RestDataReceiver* receiver);

  // Network scanning
  void scan(bool passive = false, bool async = true);

  // Data send methods to home automation
  void sendToHAFloat(const char* path, const char* query, const float value, const uint8_t precision = 2);
  void sendToHAInt(const char* path, const char* query, const int value);
  void sendToHAUInt(const char* path, const char* query, const unsigned int value);
  void sendToHAULong(const char* path, const char* query, const unsigned long value);
  void sendToHALongLong(const char* path, const char* query, int64_t value);
  void sendToHABool(const char* path, const char* query, const bool value);
  void sendToHAString(const char* path, const char* query, const char* value);
  void sendRequestToHA(const char* path, const char* query, const char* value);

  // respond method
  void sendResponse(JsonDocument& jsonResult, bool success = true, int httpCode = 200);
  void sendResponse(const char* jsonResultStr);

private:
  // Device setup and initialization
  void setupDevice();
  void initializeWiFi();
  void initializeEthernet();
  void startNetworkServices();  // Starts HTTPClient & WebServer
  void restartNetworkServices(int status = 1);
  int testNetworkServices();
  void readSettings();

  // Event handlers & callbacks
  void onDisconnected();
  static void onRestDataReceivedCallback(const char* path, WebServer& server);
  void onRestDataReceived(const char* path, WebServer& server);
  void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);
  bool comparePrefixedPath(const char* fullPath, const char* subPath);
  void buildApiPath(const char* path, char* outPath);

  // Singleton instance
  static NukiNetwork* _inst;

  // Configuration & network parameters
  Preferences* _preferences;
  BridgeApiToken* _apitoken = nullptr;
  IPConfiguration* _ipConfiguration = nullptr;
  String _hostname;
  String _WiFissid;
  String _WiFipass;

  // Network & system states
  bool _firstBootAfterDeviceChange = false;
  bool _webEnabled = true;
  bool _apiEnabled = false;
  bool _openAP = false;
  bool _startAP = true;
  bool _connected = false;
  bool _hardwareInitialized = false;
  bool _publishDebugInfo = false;
  bool _restartOnDisconnect = false;
  bool _disableNetworkIfNotConnected = false;
  int _networkServicesState = -3;
  int _NetworkServicesConnectCounter = 0;

  // Timestamps for network monitoring
  int64_t _checkIpTs = -1;
  int64_t _lastConnectedTs = 0;
  int64_t _lastMaintenanceTs = 0;
  int64_t _lastNetworkServiceTs = 0;
  int64_t _publishedUpTime = 0;
  int64_t _lastRssiTs = 0;

  // Network services & connection parameters
  WebServer* _server = nullptr;  // for REST-API Requests
  HTTPClient* _httpClient = nullptr;
  std::vector<RestDataReceiver*> _restDataReceivers;
  int _foundNetworks = 0;
  int _networkTimeout = 0;
  int _rssiPublishInterval = 0;
  int _MaintenancePublishIntervall = 0;
  int _networkServicesConnectCounter = 0;

  // Signal strength & network type
  NetworkDeviceType _networkDeviceType = (NetworkDeviceType)-1;
  int8_t _lastRssi = 127;

  // Home automation parameters
  bool _homeAutomationEnabled = false;
  String _homeAutomationAdress;
  String _homeAutomationUser;
  String _homeAutomationPassword;
  int _homeAutomationPort;

  char* _buffer;
  const size_t _bufferSize;
  char _apiBridgePath[129] = { 0 };
};

#endif  // NUKI_NETWORK_H
