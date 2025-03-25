#pragma once

#include "NukiNetwork.h"
#include "NukiConstants.h"
#include "BleScanner.h"
#include <NukiLock.h>
#include "LockActionResult.h"
#include "NukiDeviceId.hpp"
#include "EspMillis.h"

class NukiWrapper : public Nuki::SmartlockEventHandler
{
public:
    /**
     * @brief Erzeugt eine Instanz zum Kommunizieren mit dem Nuki-Smartlock.
     * @param deviceName   Referenz auf den Name des Geräts.
     * @param deviceId     Zeiger auf eine NukiDeviceId Instanz für das Gerät.
     * @param scanner      Zeiger auf die BleScanner Instanz
     * @param preferences  Zeiger auf die Preferences Instanz für Konfigurationseinstellungen.
     * @param buffer       Zeiger auf einen Puffer zur Datenverarbeitung.
     * @param bufferSize   Größe des Puffers in Bytes.
     */
    NukiWrapper(const std::string &deviceName, NukiDeviceId *deviceId, BleScanner::Scanner *scanner, NukiNetwork *network, Preferences *preferences, char *buffer, size_t bufferSize);

    /**
     * @brief Standard-Destruktor.
     */
    virtual ~NukiWrapper();

    /**
     * @brief Initialisiert die Kommunikation mit dem Nuki-Schloss (z.B. BLE starten, Pairing prüfen usw.).
     */
    void initialize();

    void readSettings();

    /**
     * @brief Aktualisiert den internen Zustand, z.B. Aufrechterhaltung der BLE-Verbindung.
     *        Sollte regelmäßig (z.B. in loop()) aufgerufen werden.
     */
    void update(bool reboot);

    /**
     * @brief Führt einen Lock-Befehl aus (Verriegeln).
     */
    void lock();

    /**
     * @brief Führt einen Unlock-Befehl aus (Aufschließen).
     */
    void unlock();

    /**
     * @brief Löst das Entriegeln mit Fallenöffnung (Türschnapper) aus.
     */
    void unlatch();

    /**
     * @brief Führt Lock ’n’ Go aus (Abschließen nach kurzer Verzögerung).
     */
    void lockngo();

    /**
     * @brief Führt Lock ’n’ Go mit Fallenöffnung aus.
     */
    void lockngounlatch();

    /**
     * @brief Setzt den Security Pin für das Schloss.
     */
    void setPin(uint16_t pin);

    /**
     * @brief Prüft, ob ein gültiger PIN gesetzt ist (vereinfachte Prüfung).
     */
    bool isPinValid() const;

    /**
     * @brief Liest den aktuell gesetzten Security Pin aus.
     */
    uint16_t getPin();

    void unpair();
    
    /**
     * @brief Liefert den zuletzt bekannten LockState zurück.
     *        (z.B. kann man hier NukiLock::LockState::Locked usw. auswerten.)
     */
    NukiLock::LockState getLockState() const;
    const NukiLock::KeyTurnerState &keyTurnerState();

    void disableWatchdog();

    const bool isPaired() const;
    const bool hasKeypad() const;
    bool hasDoorSensor() const;

    const BLEAddress getBleAddress() const;

    String firmwareVersion() const;
    String hardwareVersion() const;
    void notify(Nuki::EventType eventType) override;

private:
    static LockActionResult onLockActionReceivedCallback(const char *value);
    LockActionResult onLockActionReceived(const char* value);
    void postponeBleWatchdog();

    /**
     * @brief Fragt den Status (KeyTurnerState) des Schlosses aktiv ab (z.B. ob verriegelt).
     *        Return-Wert zeigt an, ob das Abfragen geklappt hat.
     */
    bool updateKeyTurnerState();

    /**
     * @brief Fragt den Batteriestatus des Schlosses ab.
     *        Return-Wert zeigt an, ob das Abfragen geklappt hat.
     */
    bool updateBatteryState();

    bool updateConfig();
    void updateAuthData(bool retrieved);
    void updateTimeControl(bool retrieved);
    void updateKeypad(bool retrieved);
    void updateAuth(bool retrieved);
    void updateTime();

    void readConfig();
    void readAdvancedConfig();


    void printCommandResult(Nuki::CmdResult result);

    NukiLock::LockAction lockActionToEnum(const char* str); // char array at least 14 characters

    std::string _deviceName;
    NukiDeviceId *_deviceId = nullptr;
    // Internal NukiLock instance that takes care of the actual BLE communication
    NukiLock::NukiLock _nukiLock;
    BleScanner::Scanner *_bleScanner = nullptr;
    NukiNetwork *_network = nullptr;
    Preferences *_preferences;


    std::vector<uint16_t> _keypadCodeIds;
    std::vector<uint32_t> _keypadCodes;
    std::vector<uint8_t> _timeControlIds;
    std::vector<uint32_t> _authIds;
    NukiLock::KeyTurnerState _lastKeyTurnerState;
    NukiLock::KeyTurnerState _keyTurnerState;

    NukiLock::BatteryReport _batteryReport;
    NukiLock::BatteryReport _lastBatteryReport;

    int _intervalLockstate = 0;    // seconds
    int _intervalBattery = 0;      // seconds
    int _intervalConfig = 60 * 60; // seconds
    int _intervalKeypad = 0;       // seconds

    NukiLock::Config _nukiConfig = {0};
    NukiLock::AdvancedConfig _nukiAdvancedConfig = {0};
    bool _nukiConfigValid = false;
    bool _nukiAdvancedConfigValid = false;
    bool _paired = false;
    bool _statusUpdated = false;
    bool _checkKeypadCodes = false;
    int _invalidCount = 0;
    int64_t _lastCodeCheck = 0;

    int _nrOfRetries = 0;
    int _retryDelay = 0;
    bool _hasKeypad = false;
    bool _forceDoorsensor = false;
    bool _forceKeypad = false;
    bool _keypadEnabled = false;
    bool _forceId = false;
    uint _maxKeypadCodeCount = 0;
    uint _maxTimeControlEntryCount = 0;
    uint _maxAuthEntryCount = 0;

    int _restartBeaconTimeout = 0; // seconds
    int _retryConfigCount = 0;
    int _retryLockstateCount = 0;
    int _rssiPublishInterval = 0;
    int64_t _statusUpdatedTs = 0;
    int64_t _disableBleWatchdogTs = 0;
    int64_t _nextLockStateUpdateTs = 0;
    int64_t _waitAuthLogUpdateTs = 0;
    int64_t _waitKeypadUpdateTs = 0;
    int64_t _nextBatteryReportTs = 0;
    int64_t _nextConfigUpdateTs = 0;
    int64_t _waitTimeControlUpdateTs = 0;
    int64_t _waitAuthUpdateTs = 0;
    int64_t _nextTimeUpdateTs = 0;
    int64_t _nextRssiTs = 0;
    int64_t _lastRssi = 0;
    int64_t _nextKeypadUpdateTs = 0;
    uint32_t _basicLockConfigaclPrefs[16];
    uint32_t _advancedLockConfigaclPrefs[25];
    String _firmwareVersion = "";
    String _hardwareVersion = "";
    volatile NukiLock::LockAction _nextLockAction = (NukiLock::LockAction)0xff;

    char *_buffer;
    const size_t _bufferSize;
};
