#pragma once

#include "lib/NukiBleEsp32/src/NukiLock.h"
#include "lib/NukiBleEsp32/src/NukiConstants.h"
#include "lib/NukiBleEsp32/src/NukiDataTypes.h"
#include <BleScanner.h>
#include "NukiDeviceId.h"
#include "AccessLevel.h"
#include "QueryCommand.h"
#include "LockActionResult.h"
#include "BridgeApiToken.h"

class NukiWrapper : public Nuki::SmartlockEventHandler {
public:
  NukiWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, Preferences* preferences);
  virtual ~NukiWrapper();

  void initialize(const bool& firstStart);
  void update(uint8_t queryCommands=0);

  void lock();
  void unlock();
  void unlatch();

  bool isPinSet();
  void setPin(const uint16_t pin);
  void unpair();

  void disableWatchdog();

  const NukiLock::KeyTurnerState& keyTurnerState();
  bool isPaired() const;
  bool hasKeypad() const;
  bool hasDoorSensor() const;
  const BLEAddress getBleAddress() const;

  const NukiLock::Config& Config() const;

  std::string firmwareVersion() const;
  std::string hardwareVersion() const;

  void notify(Nuki::EventType eventType) override;

private:

  //static LockActionResult onLockActionReceivedCallback(const char* value);
  //static void onConfigUpdateReceivedCallback(const char* topic, const char* value);
  //static void onKeypadCommandReceivedCallback(const char* command, const uint& id, const String& name, const String& code, const int& enabled);

  //void onConfigUpdateReceived(const char* topic, const char* value);
  //void onKeypadCommandReceived(const char* command, const uint& id, const String& name, const String& code, const int& enabled);

  void updateKeyTurnerState();
  void updateBatteryState();
  void updateConfig();
  void updateAuthData();
  void updateKeypad();
  void postponeBleWatchdog();

  void readConfig();
  void readAdvancedConfig();

  std::string CommandResultToString(Nuki::CmdResult result, const char* logDescription = nullptr);

  NukiLock::LockAction lockActionToEnum(const char* str); // char array at least 14 characters

  std::string _deviceName;
  NukiDeviceId* _deviceId = nullptr;
  BleScanner::Scanner* _bleScanner = nullptr;
  Preferences* _preferences;
  NukiLock::NukiLock _nukiLock;
  int _intervalLockstate = 0;     // seconds
  int _intervalBattery = 0;       // seconds
  int _intervalConfig = 60 * 60;  // seconds
  int _intervalKeypad = 0;        // seconds
  int _restartBeaconTimeout = 0;  // seconds
  bool _clearAuthData = false;
  std::vector<uint16_t> _keypadCodeIds;

  NukiLock::KeyTurnerState _lastKeyTurnerState;
  NukiLock::KeyTurnerState _keyTurnerState;

  NukiLock::BatteryReport _batteryReport;
  NukiLock::BatteryReport _lastBatteryReport;

  NukiLock::Config _nukiConfig = { 0 };
  NukiLock::AdvancedConfig _nukiAdvancedConfig = { 0 };


  bool _nukiConfigValid = false;
  bool _nukiAdvancedConfigValid = false;
  bool _paired = false;
  bool _statusUpdated = false;
  bool _configRead = false;
  bool _hasKeypad = false;
  bool _keypadEnabled = false;
  uint _maxKeypadCodeCount = 0;

  int _nrOfRetries = 0;
  int _retryDelay = 0;
  int _retryCount = 0;
  int _retryLockstateCount = 0;
  long _rssiUpdateInterval = 0;
  static AccessLevel _accessLevel;
  int64_t _nextRetryTs = 0;
  int64_t _nextLockStateUpdateTs = 0;
  int64_t _nextBatteryReportTs = 0;
  int64_t _nextConfigUpdateTs = 0;
  int64_t _nextKeypadUpdateTs = 0;
  int64_t _nextRssiTs = 0;
  int64_t _lastRssi = 0;
  int64_t _disableBleWatchdogTs = 0;
  std::string _firmwareVersion = "";
  std::string _hardwareVersion = "";
  volatile NukiLock::LockAction _nextLockAction = (NukiLock::LockAction)0xff;
};