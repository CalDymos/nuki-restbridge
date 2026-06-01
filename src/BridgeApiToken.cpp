#include <Arduino.h>
#include "BridgeApiToken.h"
#include "PreferencesKeys.h"

BridgeApiToken::BridgeApiToken(Preferences *preferences, const std::string &preferencesId)
  : _preferences(preferences),
    _preferencesId(preferencesId) {
  _preferences->getString(_preferencesId.c_str(), _apiToken, 21);
}

char* BridgeApiToken::get() {
  return _apiToken;
}

void BridgeApiToken::assignToken(const char *token) {
  strncpy(_apiToken,token, sizeof(_apiToken));
  _apiToken[strnlen(token, sizeof(_apiToken) - 1)] = '\0';
  _preferences->putString(_preferencesId.c_str(), token);
}

void BridgeApiToken::assignNewToken() {
    char newToken[21] = {0};

    // use ESP32 Hardware-TRNG
    esp_fill_random(newToken, 20);

    // Map to printable ASCII characters (a–z, 0–9 = 36 characters)
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < 20; i++) {
        newToken[i] = charset[(uint8_t)newToken[i] % sizeof(charset)];
    }
    newToken[20] = '\0';

    assignToken(newToken);
}