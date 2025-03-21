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
  assignToken(getRandomToken());
}

char* BridgeApiToken::getRandomToken() {

  static char Apitoken[21] = {0};
  char rndNum;
  char rndLetter;

  for (int i = 0; i < 20; i++) {
    rndNum = random(48, 57);
    rndLetter = random(97, 122);
    if (random(1, 2) == 1) {
      Apitoken[i] = rndNum;
    } else {
      Apitoken[i] = rndLetter;
    }
  }
  Apitoken[20] = '\0';
  return Apitoken;
}