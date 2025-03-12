// Handles all Rest-API requests relating to the lock.
#pragma once

#include <Preferences.h>
#include <vector>
#include <list>
#include "NukiConstants.h"
#include "NukiLockConstants.h"
#include "NukiNetwork.h"
#include "QueryCommand.h"
#include "LockActionResult.h"
#include "EspMillis.h"

class NukiNetworkLock : public RestDataReceiver {
public:

  explicit NukiNetworkLock(NukiNetwork* network, Preferences* preferences, char* buffer, size_t bufferSize);
  virtual ~NukiNetworkLock();

  void initialize();
  bool update();

  void sendToHAKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState, const NukiLock::KeyTurnerState& lastKeyTurnerState);
  //void publishState(NukiLock::LockState lockState);
  void sendToHAAuthorizationInfo(const std::list<NukiLock::LogEntry>& logEntries, bool latest);
  void sendToHACommandResult(const char* resultStr); // publishCommandResult
  void sendToHALockstateCommandResult(const char* resultStr);
  void sendToHABatteryReport(const NukiLock::BatteryReport& batteryReport);
  void sendToHAConfig(const NukiLock::Config& config);
  void sendToHAAdvancedConfig(const NukiLock::AdvancedConfig& config);
  void sendToHARssi(const int& rssi); //publishRssi
  void sendToHARetry(const std::string& message); // publishRetry
  void sendToHABleAddress(const std::string& address); //publishBleAddress
  //void publishKeypad(const std::list<NukiLock::KeypadEntry>& entries, uint maxKeypadCodeCount);
  //void publishTimeControl(const std::list<NukiLock::TimeControlEntry>& timeControlEntries, uint maxTimeControlEntryCount);
  void sendToHAStatusUpdated(const bool statusUpdated); // publishStatusUpdated
  //void publishConfigCommandResult(const char* result);
  //void publishKeypadCommandResult(const char* result);
  //void publishTimeControlCommandResult(const char* result);
  //void publishAuthCommandResult(const char* result);
  //void publishOffAction(const int value);

  void setLockActionReceivedCallback(LockActionResult (*lockActionReceivedCallback)(const char* value));
  void setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char* value));
  void setKeypadCommandReceivedCallback(void (*keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled));
  void setTimeControlCommandReceivedCallback(void (*timeControlCommandReceivedReceivedCallback)(const char* value));
  void setAuthCommandReceivedCallback(void (*authCommandReceivedReceivedCallback)(const char* value));
  void onRestDataReceived(const char* path, WebServer& server) override;

  //const uint32_t getAuthId() const;
  //const char* getAuthName();
  int NetworkServicesState();
  uint8_t queryCommands();
private:
  bool comparePrefixedPath(const char* fullPath, const char* subPath);

  void buildApiPath(const char* path, char* outPath);
  char* getArgs(WebServer& server);

  NukiNetwork* _network = nullptr;
  Preferences* _preferences = nullptr;
  bool _apiEnabled = false;
  char* _buffer;
  size_t _bufferSize;
  char _apiLockPath[129] = { 0 };

  String _keypadCommandName = "";
  String _keypadCommandCode = "";
  uint _keypadCommandId = 0;
  int _keypadCommandEnabled = 1;
  uint8_t _queryCommands = 0;

  LockActionResult (*_lockActionReceivedCallback)(const char* value) = nullptr;
  void (*_configUpdateReceivedCallback)(const char* value) = nullptr;
  void (*_keypadCommandReceivedReceivedCallback)(const char* command, const uint& id, const String& name, const String& code, const int& enabled) = nullptr;
  void (*_timeControlCommandReceivedReceivedCallback)(const char* value) = nullptr;
  void (*_authCommandReceivedReceivedCallback)(const char* value) = nullptr;
};