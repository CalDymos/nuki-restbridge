#pragma once

#include "NukiNetwork.h"
#include "NukiConstants.h"
#include "BleScanner.h"
#include <NukiLock.h>
#include "LockActionResult.h"
#include "NukiDeviceId.hpp"
#include "EspMillis.h"
#include "util/NukiRetryHandler.h"
#include "RestartReason.h"
#include "BleControllerRestartReason.h"

class NukiWrapper : public Nuki::SmartlockEventHandler
{
public:
    using BleControllerRestartReason = ::BleControllerRestartReason;
    /**
     * @brief Creates an instance to communicate with the Nuki Smart Lock.
     * @param deviceName   Reference to the name of the device.
     * @param deviceId     Pointer to the NukiDeviceId instance used for identification.
     * @param scanner      Pointer to the BLE scanner instance.
     * @param network      Pointer to the NukiNetwork instance for communication.
     * @param preferences  Pointer to the Preferences instance for persistent settings.
     * @param buffer       Pointer to a data buffer used for communication or state serialization.
     * @param bufferSize   Size of the provided buffer in bytes.
     */
    NukiWrapper(const std::string &deviceName, NukiDeviceId *deviceId, BleScanner::Scanner *scanner, NukiNetwork *network, Preferences *preferences, char *buffer, size_t bufferSize);

    /**
     * @brief Standard destructor.
     */
    virtual ~NukiWrapper();

    /**
     * @brief Initializes communication with the Nuki Smart Lock (e.g. start BLE, check pairing, etc.).
     */
    void initialize();

    /**
     * @brief Reads settings from preferences (e.g. intervals, flags, config).
     */
    void readSettings();

    /**
     * @brief Updates the internal state (e.g. maintain BLE connection).
     *        Should be called regularly, e.g. from the main loop().
     * @param reboot Whether to reboot the lock logic after a failure.
     */
    void update(bool reboot);

    /**
     * @brief Executes a lock command (lock the door).
     */
    void lock();

    /**
     * @brief Executes an unlock command (open the door).
     */
    void unlock();

    /**
     * @brief Executes an unlatch command (releases the door latch).
     */
    void unlatch();

    /**
     * @brief Executes the Lock 'n' Go command (lock after delay).
     */
    void lockngo();

    /**
     * @brief Executes Lock 'n' Go with unlatch (door latch release).
     */
    void lockngounlatch();

    /**
     * @brief Sets the security PIN for the lock.
     * @param pin The 4-digit PIN code.
     */
    void setPin(uint16_t pin);

    /**
     * @brief Checks whether a valid PIN is currently set (basic check).
     * @return True if a PIN is stored.
     */
    bool isPinValid() const;

    /**
     * @brief Returns the currently set security PIN.
     * @return The PIN as a 16-bit integer.
     */
    uint16_t getPin();

    /**
     * @brief Unpairs the lock from the current device.
     */
    void unpair();

    /**
     * @brief Returns the last known lock state (e.g. Locked, Unlocked, etc.).
     */
    NukiLock::LockState getLockState() const;

    /**
     * @brief Returns the most recent full KeyTurnerState received from the device.
     */
    const NukiLock::KeyTurnerState &keyTurnerState();

    /**
     * @brief Disables the BLE watchdog timer.
     */
    void disableWatchdog();

    /**
     * @brief Returns true if the lock is currently paired with this device.
     */
    const bool isPaired() const;

    /**
     * @brief Returns true if the lock has a keypad accessory paired.
     */
    const bool hasKeypad() const;

    /**
     * @brief Returns true if the lock has a door sensor.
     */
    bool hasDoorSensor() const;

    /**
     * @brief Returns the BLE address of the lock.
     */
    const BLEAddress getBleAddress() const;

    /**
     * @brief Returns whether the lock has successfully connected at least once.
     */
    const bool hasConnected() const;

    /**
     * @brief Returns the reason for the last BLE controller restart.
     * @return BleControllerRestartReason Enum value indicating the restart reason.
     */
    BleControllerRestartReason getBleControllerRestartReason() const;

    /**
     * @brief Returns the firmware version of the connected Smart Lock.
     * @return Firmware version as a string.
     */
    String firmwareVersion() const;

    /**
     * @brief Returns the hardware version of the connected Smart Lock.
     * @return Hardware version as a string.
     */
    String hardwareVersion() const;

    /**
     * @brief Handles Smart Lock events (e.g. battery low, lock state change, etc.).
     *        This method is called by the NukiLock instance and overrides the base event handler.
     * @param eventType The type of event received from the lock.
     */
    void notify(Nuki::EventType eventType) override;

    /**
     * @brief Encrypts a numeric keypad code using user-defined Preferences values.
     *        Applies: Encrypted = ((Code * Multiplier + Offset) % Modulus) + Modulus
     * @param code The original numeric code (max. 6 digits).
     * @return Encrypted code as 32-bit unsigned integer.
     */
    uint32_t encryptKeypadCode(uint32_t code);

    /**
     * @brief Decrypts a previously encrypted keypad code using inverse logic.
     *        Applies: Decrypted = ((Encrypted - Offset) * InverseMultiplier) % Modulus
     * @param encryptedCode The encrypted code to decode.
     * @return Original 6-digit code as 32-bit unsigned integer.
     */
    uint32_t decryptKeypadCode(uint32_t encryptedCode);

    void setTimeCtrlInfoEnabled(bool enable);
    void setAuthInfoEnabled(bool enable);
    void setkeypadInfoEnabled(bool enable);

private:
    /**
     * @brief Handles an incoming lock action request from API.
     * @param value Lock action string value.
     * @return Result of the lock action execution.
     */
    static LockActionResult onLockActionReceivedCallback(const char *value);

    static void onConfigUpdateReceivedCallback(const char *value);

    static void onKeypadCommandReceivedCallback(const char *command, const uint &id, const String &name, const String &code, const int &enabled);

    static void onTimeControlCommandReceivedCallback(const char *value);

    static void onAuthCommandReceivedCallback(const char *value);

    /**
     * @brief Static callback function for external lock action requests.
     * @param value Lock action string value.
     * @return Result of the lock action execution.
     */
    LockActionResult onLockActionReceived(const char *value);

    void onConfigUpdateReceived(const char *value);

    void onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int &enabled);

    void onTimeControlCommandReceived(const char *value);

    void onAuthCommandReceived(const char *value);
    /**
     * @brief Resets or delays the BLE watchdog timer.
     */
    void postponeBleWatchdog();

    /**
     * @brief Actively queries the current KeyTurnerState of the lock (e.g. whether it is locked).
     * @return True if the request succeeded.
     */
    bool updateKeyTurnerState();

    /**
     * @brief Queries the current battery status of the lock.
     * @return True if the request succeeded.
     */
    bool updateBatteryState();

    /**
     * @brief Queries the lock's basic configuration and updates local cache.
     * @return True if the configuration was successfully read.
     */
    bool updateConfig();

    /**
     * @brief Updates internal auth data state (from lock to memory).
     * @param retrieved Whether new data was retrieved from the device.
     */
    void updateAuthData(bool retrieved);

    /**
     * @brief Updates internal time control state (from lock to memory).
     * @param retrieved Whether new data was retrieved from the device.
     */
    void updateTimeControl(bool retrieved);

    /**
     * @brief Updates internal keypad state (from lock to memory).
     * @param retrieved Whether new data was retrieved from the device.
     */
    void updateKeypad(bool retrieved);

    /**
     * @brief Updates internal auth configuration (from lock to memory).
     * @param retrieved Whether new data was retrieved from the device.
     */
    void updateAuth(bool retrieved);

    /**
     * @brief Updates the time on the lock (synchronizes RTC).
     */
    void updateTime();

    uint32_t calcKeypadCodeInverse();

    /**
     * @brief Reads basic lock configuration from the device.
     */
    bool readConfig();

    /**
     * @brief Reads advanced lock configuration from the device.
     */
    bool readAdvancedConfig();

    /**
     * @brief Prints the result of a Nuki command to log.
     * @param result Command result code from NukiLock::CmdResult.
     */
    void printCommandResult(Nuki::CmdResult result);

    /**
     * @brief Converts a lock action string to LockAction enum.
     * @param str A character array containing the lock action (min 14 bytes).
     * @return Corresponding NukiLock::LockAction enum value.
     */
    NukiLock::LockAction lockActionToEnum(const char *str); // char array at least 14 characters

    Nuki::AdvertisingMode advertisingModeToEnum(const char *str);
    Nuki::TimeZoneId timeZoneToEnum(const char *str);
    uint8_t fobActionToInt(const char *str);
    NukiLock::ButtonPressAction buttonPressActionToEnum(const char *str);
    Nuki::BatteryType batteryTypeToEnum(const char *str);

    std::string _deviceName;                       // Name of the smart lock device (user-defined identifier).
    NukiDeviceId *_deviceId = nullptr;             // Unique device ID stored in preferences.
                                                   //
    BleScanner::Scanner *_bleScanner = nullptr;    // BLE scanner instance to find/connect the lock.
    NukiLock::NukiLock _nukiLock;                  // Instance handling BLE communication with the lock.
    NukiNetwork *_network = nullptr;               // Reference to the network service (API, Home Automation).
    Preferences *_preferences;                     // Pointer to the ESP32 preferences for persistent storage.
    NukiRetryHandler *_nukiRetryHandler = nullptr; // Retry handler for Nuki communication.

    char *_buffer;                                                                             // Shared data buffer for building requests or storing responses.
    const size_t _bufferSize;                                                                  // Size of the shared buffer in bytes.
                                                                                               //
    NukiLock::KeyTurnerState _lastKeyTurnerState;                                              // Previously known KeyTurnerState.
    NukiLock::KeyTurnerState _keyTurnerState;                                                  // Most recent KeyTurnerState from the device.
                                                                                               //
    std::vector<uint16_t> _keypadCodeIds;                                                      // IDs of configured keypad codes.
    std::vector<uint32_t> _keypadCodes;                                                        // Keypad code hashes (or representations).
    std::vector<uint8_t> _timeControlIds;                                                      // Time control entry IDs.
    std::vector<uint32_t> _authIds;                                                            // Authorization IDs stored on the device.
                                                                                               //
    bool _checkKeypadCodes = false;                                                            // Indicates if keypad codes need to be validated.
    bool _hasKeypad = false;                                                                   // Whether the lock has a keypad accessory.
    bool _forceDoorsensor = false;                                                             // Enforce door sensor presence.
    bool _forceKeypad = false;                                                                 // Enforce keypad detection.
    bool _forceId = false;                                                                     // Force assignment of a specific device ID.
    bool _keypadInfoEnabled = false;                                                           // Indicates if the keypad info is currently active.
                                                                                               //
    bool _authInfoEnabled = false;                                                             // Indicates if the keypad info is currently active.
    bool _timeCtrlInfoEnabled = false;                                                         // Indicates if the keypad info is currently active.
                                                                                               //
    NukiLock::Config _nukiConfig = {0};                                                        // Basic configuration of the lock.
    NukiLock::AdvancedConfig _nukiAdvancedConfig = {0};                                        // Advanced configuration.
    bool _nukiConfigValid = false;                                                             // Whether the basic configuration is valid.
    bool _nukiAdvancedConfigValid = false;                                                     // Whether the advanced configuration is valid.
    uint32_t _basicLockConfigaclPrefs[16];                                                     // Stored preference bitfields for access control of basic lock configuration (persisted per ACL entry).
    uint32_t _advancedLockConfigaclPrefs[25];                                                  // Stored preference bitfields for access control of advanced lock configuration (persisted per ACL entry).
                                                                                               //
    NukiLock::BatteryReport _batteryReport;                                                    // Latest battery status reported by the lock.
    NukiLock::BatteryReport _lastBatteryReport;                                                // Previously stored battery report.
                                                                                               //
    int _intervalLockstate = 0;                                                                // Update interval for lock state polling (seconds).
    int _intervalBattery = 0;                                                                  // Update interval for battery checks (seconds).
    int _intervalConfig = 60 * 60;                                                             // Interval for configuration polling (seconds).
    int _intervalKeypad = 0;                                                                   // Interval for keypad update polling (seconds).
                                                                                               //
    int64_t _statusUpdatedTs = 0;                                                              // Timestamp of last successful status update.
    int _newSignal = 0;                                                                        // Signal strength of the last connection.
    int64_t _disableBleWatchdogTs = 0;                                                         // Timestamp when BLE watchdog was disabled.
    int64_t _nextLockStateUpdateTs = 0;                                                        // Next planned lock state update timestamp.
    int64_t _nextBatteryReportTs = 0;                                                          // Next planned battery report timestamp.
    int64_t _nextConfigUpdateTs = 0;                                                           // Next configuration update timestamp.
    int64_t _nextKeypadUpdateTs = 0;                                                           // Next keypad update timestamp.
    int64_t _nextTimeUpdateTs = 0;                                                             // Next time sync with lock.
    int64_t _nextRssiTs = 0;                                                                   // Next planned RSSI update.
    int64_t _waitAuthUpdateTs = 0;                                                             // Timestamp for next auth data check.
    int64_t _waitTimeControlUpdateTs = 0;                                                      // Timestamp for next time control update.
    int64_t _waitKeypadUpdateTs = 0;                                                           // Timestamp for next keypad sync.
    int64_t _waitAuthLogUpdateTs = 0;                                                          // Timestamp for delayed auth log polling.
    int64_t _lastPairingLogTs = 0;                                                             // throttling pairing Message via time
                                                                                               //
    int _invalidCount = 0;                                                                     // Number of invalid communication attempts.
    int _nrOfRetries = 0;                                                                      // Retry counter for reconnect attempts.
    int _retryDelay = 0;                                                                       // Delay between retries in milliseconds.
    int _retryConfigCount = 0;                                                                 // Retry attempts for reading configuration.
    int _retryLockstateCount = 0;                                                              // Retry attempts for polling lock state.
    int _restartBeaconTimeout = 0;                                                             // Timeout to restart beacon scanner (seconds).
    int _rssiPublishInterval = 0;                                                              // Interval in seconds for publishing RSSI values.
                                                                                               //
    String _firmwareVersion = "";                                                              // Cached firmware version string.
    String _hardwareVersion = "";                                                              // Cached hardware version string.
                                                                                               //
    bool _hasConnected = false;                                                                // Whether the lock has successfully connected at least once.
    uint _maxKeypadCodeCount = 0;                                                              // Max supported number of keypad entries.
    uint _maxTimeControlEntryCount = 0;                                                        // Max supported time control entries.
    uint _maxAuthEntryCount = 0;                                                               // Max supported authorization entries.
    BleControllerRestartReason _bleControllerRestartReason = BleControllerRestartReason::None; // Reason for the last BLE controller restart.
                                                                                               //
    bool _keypadCodeEncryptionEnabled = false;                                                 // Whether keypad code encryption is enabled.
    uint32_t _keypadCodeMultiplier;                                                            // Multiplier for keypad code encryption.
    uint32_t _keypadCodeOffset;                                                                // Offset for keypad code encryption.
    uint32_t _keypadCodeModulus;                                                               // Modulus for keypad code encryption.
    uint32_t _keypadCodeInverse;                                                               // Inverse multiplier for keypad code decryption.
                                                                                               //
    bool _paired = false;                                                                      // Whether the lock is currently paired.
    bool _pairingMsgShown = false;                                                             // nur einmalige Sofortausgabe
    bool _statusUpdated = false;                                                               // Whether the latest update was successful.
    int64_t _lastCodeCheck = 0;                                                                // Last time the PIN codes were checked.
    int64_t _lastRssi = 0;                                                                     // Last known RSSI value.
                                                                                               //
    volatile NukiLock::LockAction _nextLockAction = (NukiLock::LockAction)0xff;                // Next lock action to be performed via API.
};
