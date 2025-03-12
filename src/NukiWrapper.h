// Handles the general communication between the Nuki Bridge and the Nuki Lock
#pragma once

#include "NukiNetworkLock.h"
#include "NukiConstants.h"
#include "NukiDataTypes.h"
#include "BleScanner.h"
#include "NukiLock.h"
#include "LockActionResult.h"
#include "NukiDeviceId.h"
#include "EspMillis.h"

class NukiWrapper : public Nuki::SmartlockEventHandler {
public:
  NukiWrapper(const std::string &deviceName, NukiDeviceId *deviceId, BleScanner::Scanner *scanner, NukiNetworkLock *network, Preferences *preferences, char *buffer, size_t bufferSize);
  virtual ~NukiWrapper();

  void initialize();
  void readSettings();
  void update(bool reboot = false);

  void lock();
  void unlock();
  void unlatch();
  void lockngo();
  void lockngounlatch();

  bool isPinValid();
  void setPin(const uint16_t pin);
  uint16_t getPin();
  void unpair();

  void disableWatchdog();

  const NukiLock::KeyTurnerState &keyTurnerState();
  bool isPaired() const;
  bool hasKeypad() const;
  bool hasDoorSensor() const;
  const BLEAddress getBleAddress() const;

  const NukiLock::Config &Config() const;

  std::string firmwareVersion() const;
  std::string hardwareVersion() const;

  void notify(Nuki::EventType eventType) override;

private:

  static LockActionResult onLockActionReceivedCallback(const char *value);
  static void onConfigUpdateReceivedCallback(const char *value);
  static void onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int &enabled);
  static void onTimeControlCommandReceivedCallback(const char *value);
  static void onAuthCommandReceivedCallback(const char* value);
  LockActionResult onLockActionReceived(const char *value);
  void onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int &enabled);
  void onConfigUpdateReceived(const char *value);
  void onTimeControlCommandReceived(const char *value);
  void onAuthCommandReceived(const char* value);

  bool updateKeyTurnerState();
  void updateBatteryState();
  void updateConfig();
  void updateAuthData(bool retrieved);
  void updateKeypad(bool retrieved);
  void updateTimeControl(bool retrieved);
  void updateAuth(bool retrieved);
  void postponeBleWatchdog();
  void updateTime();

  void readConfig();
  void readAdvancedConfig();

  std::string printCmdResult(Nuki::CmdResult result, const char *logDescription = nullptr);  //printCommandResult

  NukiLock::LockAction lockActionToEnum(const char *str);  // char array at least 14 characters
  Nuki::AdvertisingMode advertisingModeToEnum(const char *str);
  Nuki::TimeZoneId timeZoneToEnum(const char *str);
  uint8_t fobActionToInt(const char *str);
  NukiLock::ButtonPressAction buttonPressActionToEnum(const char *str);
  Nuki::BatteryType batteryTypeToEnum(const char *str);

  std::string _deviceName;
  NukiDeviceId *_deviceId = nullptr;
  BleScanner::Scanner *_bleScanner = nullptr;
  Preferences *_preferences;
  NukiLock::NukiLock _nukiLock;
  NukiNetworkLock *_network = nullptr;
  int _intervalLockstate = 0;     // seconds
  int _intervalBattery = 0;       // seconds
  int _intervalConfig = 60 * 60;  // seconds
  int _intervalKeypad = 0;        // seconds
  int _restartBeaconTimeout = 0;  // seconds
  bool _publishAuthData = false;
  bool _clearAuthData = false;
  bool _checkKeypadCodes = false;
  int _invalidCount = 0;
  int64_t _lastCodeCheck = 0;
  std::vector<uint16_t> _keypadCodeIds;
  std::vector<uint32_t> _keypadCodes;
  std::vector<uint8_t> _timeControlIds;
  std::vector<uint32_t> _authIds;

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
  bool _forceKeypad = false;
  bool _forceId = false;
  bool _keypadEnabled = false;
  bool _homeAutomationEnabled = false;
  uint _maxKeypadCodeCount = 0;
  uint _maxTimeControlEntryCount = 0;
  uint _maxAuthEntryCount = 0;

  int _nrOfRetries = 0;
  int _retryDelay = 0;
  int _retryCount = 0;
  int _retryConfigCount = 0;
  int _retryLockstateCount = 0;
  long _rssiPublishInterval = 0;
  int64_t _statusUpdatedTs = 0;
  int64_t _nextRetryTs = 0;
  int64_t _nextLockStateUpdateTs = 0;
  int64_t _nextBatteryReportTs = 0;
  int64_t _nextConfigUpdateTs = 0;
  int64_t _waitAuthLogUpdateTs = 0;
  int64_t _waitKeypadUpdateTs = 0;
  int64_t _waitTimeControlUpdateTs = 0;
  int64_t _waitAuthUpdateTs = 0;
  int64_t _nextKeypadUpdateTs = 0;
  int64_t _nextRssiTs = 0;
  int64_t _lastRssi = 0;
  int64_t _disableBleWatchdogTs = 0;
  uint32_t _basicLockConfigaclPrefs[16];
  uint32_t _advancedLockConfigaclPrefs[25];
  std::string _firmwareVersion = "";
  std::string _hardwareVersion = "";
  volatile NukiLock::LockAction _nextLockAction = (NukiLock::LockAction)0xff;

  char *_buffer;
  const size_t _bufferSize;
};