#include <Arduino.h>
#include "BridgeApiToken.h"
#include "util/CryptoUtils.h"
#include "PreferencesKeys.h"

BridgeApiToken::BridgeApiToken(Preferences *preferences, const std::string &preferencesId)
  : _preferences(preferences),
    _preferencesId(preferencesId) {
  _preferences->getString(_preferencesId.c_str(), _apiToken, API_TOKEN_LENGTH + 1);
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
    char newToken[API_TOKEN_LENGTH + 1] = {0};
    esp_fill_random(newToken, API_TOKEN_LENGTH);
    mapBytesToApiToken(newToken);
    assignToken(newToken);
}