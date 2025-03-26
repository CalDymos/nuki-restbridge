#pragma once

#include <cstdint>
#include <Preferences.h>
#include <string>

class BridgeApiToken
{
public:
    /**
     * Constructor for BridgeApiToken
     *
     * Initializes the token from preferences storage using a given key.
     *
     * @param preferences Pointer to the Preferences instance.
     * @param preferencesId Key used to store and retrieve the token.
     */
    BridgeApiToken(Preferences *preferences, const std::string &preferencesId);

    /**
     * Returns the currently stored API token.
     *
     * @return Pointer to the API token as a null-terminated C-string.
     */
    char *get();

    /**
     * Assigns a new token string and stores it persistently in preferences.
     *
     * @param token Null-terminated C-string containing the new token.
     */
    void assignToken(const char *token);

    /**
     * Generates and assigns a new random token.
     */
    void assignNewToken();

private:

    /**
     * Generates a new random token consisting of digits and lowercase letters.
     *
     * @return Pointer to a newly generated token (20 characters + null terminator).
     */
    char *getRandomToken();

    Preferences *_preferences;
    const std::string _preferencesId;
    char _apiToken[21] = {0};
};