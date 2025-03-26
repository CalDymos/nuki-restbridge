#pragma once

#include <Arduino.h>
#include <cstdint>
#include <Preferences.h>
#include <string>
#include <cstring>

/**
 * @brief Handles persistent storage and generation of a unique Nuki device ID.
 *
 * The device ID is stored in NVS preferences. If no ID exists, a new random one is generated and stored.
 */
class NukiDeviceId
{
public:
    /**
     * @brief Constructs the NukiDeviceId manager with a preferences reference and key name.
     *
     * Loads the device ID from preferences. If it does not exist, a new one is generated.
     *
     * @param preferences Pointer to the Preferences instance.
     * @param preferencesId Key name used to store the device ID.
     */
    NukiDeviceId(Preferences *preferences, const std::string &preferencesId)
        : _preferences(preferences),
          _preferencesId(preferencesId)
    {
        _deviceId = _preferences->getUInt(_preferencesId.c_str(), 0);

        if (_deviceId == 0)
        {
            assignNewId();
        }
    }

    /**
     * @brief Returns the stored device ID.
     *
     * @return The current device ID (32-bit unsigned integer).
     */
    uint32_t get() const
    {
        return _deviceId;
    }

    /**
     * @brief Assigns a new device ID and stores it persistently.
     *
     * @param id The new device ID to assign.
     */
    void assignId(const uint32_t &id)
    {
        _deviceId = id;
        _preferences->putUInt(_preferencesId.c_str(), id);
    }

    /**
     * @brief Generates and assigns a new random device ID.
     */
    void assignNewId()
    {
        assignId(getRandomId());
    }

private:
    /**
     * @brief Generates a new random 32-bit device ID.
     *
     * @return Randomly generated 32-bit unsigned integer.
     */
    uint32_t getRandomId()
    {
        uint8_t rnd[4];
        for (int i = 0; i < 4; i++)
        {
            rnd[i] = random(255);
        }
        uint32_t deviceId;
        memcpy(&deviceId, &rnd, sizeof(deviceId));
        return deviceId;
    }

    Preferences *_preferences;        // Pointer to the preferences instance used for storage.
    const std::string _preferencesId; // Key name under which the ID is stored.
    uint32_t _deviceId = 0;           // Cached device ID.
};
