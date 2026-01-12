#pragma once

#include <Preferences.h>
#include "ArduinoJson.h"

class ImportExport
{
public:
    /**
     * @brief Constructor for ImportExport class.
     * @param preferences Pointer to the Preferences instance for NVS access.
     */
    explicit ImportExport(Preferences *preferences);

    /**
     * @brief Exports Nuki Bridge settings to a JSON document.
     * @param json Reference to the JsonDocument to populate with settings.
     * @param redacted If true, sensitive information will be omitted.
     * @param pairing If true, includes pairing-related information.
     * @param nuki If true, includes Nuki-specific pairing information.
     */
    void exportNukiBridgeJson(JsonDocument &json, bool redacted = false, bool pairing = false, bool nuki = false);
    /**
     * @brief Imports Nuki Bridge settings from a JSON document.
     * @param doc Reference to the JsonDocument containing settings to import.
     * @return JsonDocument summarizing the changes made during import.
     */
    JsonDocument importJson(JsonDocument &doc);

    /**
     * @brief Reads settings from Preferences to initialize ImportExport state.
     */
    void readSettings();

    JsonDocument _sessionsOpts; // Summary of import changes

private:
    Preferences *_preferences;

};
