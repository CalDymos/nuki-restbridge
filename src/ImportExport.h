#pragma once

#include <Preferences.h>
#include "ArduinoJson.h"

class ImportExport
{
public:
    explicit ImportExport(Preferences* preferences);
    void exportNukiBridgeJson(JsonDocument &json, bool redacted = false, bool pairing = false, bool nuki = false);
    JsonDocument importJson(JsonDocument &doc);
    void readSettings();
    JsonDocument _sessionsOpts;
    int64_t _lastCodeCheck = 0;
    int64_t _lastCodeCheck2 = 0;
    int _invalidCount = 0;
    int _invalidCount2 = 0;
private:
    Preferences* _preferences;
    struct tm timeinfo;
    bool _updateTime = false;
};

