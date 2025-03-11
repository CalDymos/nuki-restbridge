#pragma once

#include <cstdint>
#include <Preferences.h>
#include <string>


class BridgeApiToken
{
public:
    BridgeApiToken(Preferences* preferences, const std::string& preferencesId);

    char* get();

    void assignToken(const char* token);
    void assignNewToken();

private:
    char* getRandomToken();

    Preferences* _preferences;
    const std::string _preferencesId;
    char _apiToken[21] = {0};
};