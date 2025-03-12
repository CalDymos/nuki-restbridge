#pragma once

#include "NukiNetwork.h"
#include "NukiConstants.h"
#include "BleScanner.h"
#include <NukiLock.h> // Die Nuki-Library Nuki
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

    /**
     * @brief Liefert den zuletzt bekannten LockState zurück.
     *        (z.B. kann man hier NukiLock::LockState::Locked usw. auswerten.)
     */
    NukiLock::LockState getLockState() const;

    void notify(Nuki::EventType eventType) override;

    const bool isPaired() const;

    const BLEAddress getBleAddress() const;

private:
    // Interne NukiLock-Instanz, die für die eigentliche BLE-Kommunikation sorgt
    NukiLock::NukiLock _nukiLock;

    std::string _deviceName;
    NukiDeviceId *_deviceId = nullptr;
    BleScanner::Scanner *_bleScanner = nullptr;
    Preferences *_preferences;
    NukiNetwork *_network = nullptr;

    NukiLock::KeyTurnerState _lastKeyTurnerState;
    NukiLock::KeyTurnerState _keyTurnerState;
  
    NukiLock::BatteryReport _batteryReport;
    NukiLock::BatteryReport _lastBatteryReport;

    // Merker, ob bereits einmal initialize() aufgerufen wurde
    bool _initialized = false;

    bool _nukiConfigValid = false;
    bool _nukiAdvancedConfigValid = false;
    bool _paired = false;
    bool _statusUpdated = false;

    int _restartBeaconTimeout = 0; // seconds

    int64_t _disableBleWatchdogTs = 0;

    char *_buffer;
    const size_t _bufferSize;

};
