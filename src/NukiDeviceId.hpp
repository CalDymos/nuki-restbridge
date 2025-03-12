#pragma once

#include <Arduino.h>
#include <cstdint>
#include <Preferences.h>
#include <string>
#include <cstring>


class NukiDeviceId
{
public:
    NukiDeviceId(Preferences* preferences, const std::string& preferencesId)
        : _preferences(preferences),
          _preferencesId(preferencesId)
    {
        _deviceId = _preferences->getUInt(_preferencesId.c_str(), 0);

        if (_deviceId == 0)
        {
            assignNewId();
        }
    }

    uint32_t get() const
    {
        return _deviceId;
    }

    void assignId(const uint32_t& id)
    {
        _deviceId = id;
        _preferences->putUInt(_preferencesId.c_str(), id);
    }

    void assignNewId()
    {
        assignId(getRandomId());
    }

private:
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

    Preferences* _preferences;
    const std::string _preferencesId;
    uint32_t _deviceId = 0;
};
