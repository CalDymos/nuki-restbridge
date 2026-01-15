#include "ImportExport.h"
#include "EspMillis.h"
#include "LittleFS.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "NukiConstants.h"

ImportExport::ImportExport(Preferences *preferences)
    : _preferences(preferences)
{
    readSettings();
}

void ImportExport::readSettings()
{


}

void ImportExport::exportNukiBridgeJson(JsonDocument &json, bool redacted, bool pairing, bool nuki)
{
    PreferencesKeyRegistry prefKeyRegistry;

    const std::vector<char*> keysPrefs = prefKeyRegistry.getPreferencesKeys();
    const std::vector<char*> boolPrefs = prefKeyRegistry.getPreferencesBoolKeys();
    const std::vector<char*> redactedPrefs = prefKeyRegistry.getPreferencesRedactedKeys();
    const std::vector<char*> bytePrefs = prefKeyRegistry.getPreferencesByteKeys();

    for(const auto& key : keysPrefs)
    {
        if(strcmp(key, preference_show_secrets) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_admin_secret) == 0)
        {
            continue;
        }
        if(!redacted) if(std::find(redactedPrefs.begin(), redactedPrefs.end(), key) != redactedPrefs.end())
            {
                continue;
            }
        if(!_preferences->isKey(key))
        {
            json[key] = "";
        }
        else if(std::find(boolPrefs.begin(), boolPrefs.end(), key) != boolPrefs.end())
        {
            json[key] = _preferences->getBool(key) ? "1" : "0";
        }
        else
        {
            switch(_preferences->getType(key))
            {
            case PT_I8:
                json[key] = String(_preferences->getChar(key));
                break;
            case PT_I16:
                json[key] = String(_preferences->getShort(key));
                break;
            case PT_I32:
                json[key] = String(_preferences->getInt(key));
                break;
            case PT_I64:
                json[key] = String(_preferences->getLong64(key));
                break;
            case PT_U8:
                json[key] = String(_preferences->getUChar(key));
                break;
            case PT_U16:
                json[key] = String(_preferences->getUShort(key));
                break;
            case PT_U32:
                json[key] = String(_preferences->getUInt(key));
                break;
            case PT_U64:
                json[key] = String(_preferences->getULong64(key));
                break;
            case PT_STR:
                json[key] = _preferences->getString(key);
                break;
            default:
                json[key] = _preferences->getString(key);
                break;
            }
        }
    }

    if(pairing)
    {
        if(nuki)
        {
            unsigned char currentBleAddress[6];
            unsigned char authorizationId[4] = {0x00};
            unsigned char secretKeyK[32] = {0x00};
            uint16_t storedPincode = 0000;
            Preferences nukiBlePref;
            nukiBlePref.begin(PREFERENCE_NAME, false);
            nukiBlePref.getBytes(Nuki::BLE_ADDRESS_STORE_NAME, currentBleAddress, 6);
            nukiBlePref.getBytes(Nuki::SECRET_KEY_STORE_NAME, secretKeyK, 32);
            nukiBlePref.getBytes(Nuki::AUTH_ID_STORE_NAME, authorizationId, 4);
            nukiBlePref.getBytes(Nuki::SECURITY_PINCODE_STORE_NAME, &storedPincode, 2);
            nukiBlePref.end();
            char text[255];
            text[0] = '\0';
            for(int i = 0 ; i < 6 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", currentBleAddress[i]);
            }
            json["bleAddressLock"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 32 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", secretKeyK[i]);
            }
            json["secretKeyKLock"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 4 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", authorizationId[i]);
            }
            json["authorizationIdLock"] = text;
            memset(text, 0, sizeof(text));
            json["securityPinCodeLock"] = storedPincode;
        }
    }

    for(const auto& key : bytePrefs)
    {
        size_t storedLength = _preferences->getBytesLength(key);
        if(storedLength == 0)
        {
            continue;
        }
        uint8_t serialized[storedLength];
        memset(serialized, 0, sizeof(serialized));
        size_t size = _preferences->getBytes(key, serialized, sizeof(serialized));
        if(size == 0)
        {
            continue;
        }
        char text[255];
        text[0] = '\0';
        for(int i = 0 ; i < size ; i++)
        {
            size_t offset = strlen(text);
            sprintf(&(text[offset]), "%02x", serialized[i]);
        }
        json[key] = text;
        memset(text, 0, sizeof(text));
    }
}

JsonDocument ImportExport::importJson(JsonDocument &doc)
{
    JsonDocument json;
    unsigned char currentBleAddress[6];
    unsigned char authorizationId[4] = {0x00};
    unsigned char secretKeyK[32] = {0x00};
    unsigned char currentBleAddressOpn[6];
    unsigned char authorizationIdOpn[4] = {0x00};
    unsigned char secretKeyKOpn[32] = {0x00};

    PreferencesKeyRegistry prefKeyRegistry;

    const std::vector<char*> keysPrefs = prefKeyRegistry.getPreferencesKeys();
    const std::vector<char*> boolPrefs = prefKeyRegistry.getPreferencesBoolKeys();
    const std::vector<char*> bytePrefs = prefKeyRegistry.getPreferencesByteKeys();
    const std::vector<char*> intPrefs = prefKeyRegistry.getPreferencesIntKeys();
    const std::vector<char*> uintPrefs = prefKeyRegistry.getPreferencesUIntKeys();
    const std::vector<char*> uint64Prefs = prefKeyRegistry.getPreferencesUInt64Keys();

    for(const auto& key : keysPrefs)
    {
        if(doc[key].isNull())
        {
            continue;
        }
        if(strcmp(key, preference_show_secrets) == 0)
        {
            continue;
        }
        if(std::find(boolPrefs.begin(), boolPrefs.end(), key) != boolPrefs.end())
        {
            if (doc[key].as<String>().length() > 0)
            {
                _preferences->putBool(key, (doc[key].as<String>() == "1" ? true : false));
                json[key] = "changed";
            }
            else
            {
                json[key] = "removed";
                _preferences->remove(key);
            }
            continue;
        }
        if(std::find(intPrefs.begin(), intPrefs.end(), key) != intPrefs.end())
        {
            if (doc[key].as<String>().length() > 0)
            {
                json[key] = "changed";
                _preferences->putInt(key, doc[key].as<int>());
            }
            else
            {
                json[key] = "removed";
                _preferences->remove(key);
            }
            continue;
        }
        if(std::find(uintPrefs.begin(), uintPrefs.end(), key) != uintPrefs.end())
        {
            if (doc[key].as<String>().length() > 0)
            {
                json[key] = "changed";
                _preferences->putUInt(key, doc[key].as<uint32_t>());
            }
            else
            {
                json[key] = "removed";
                _preferences->remove(key);
            }
            continue;
        }
        if(std::find(uint64Prefs.begin(), uint64Prefs.end(), key) != uint64Prefs.end())
        {
            if (doc[key].as<String>().length() > 0)
            {
                json[key] = "changed";
                _preferences->putULong64(key, doc[key].as<uint64_t>());
            }
            else
            {
                json[key] = "removed";
                _preferences->remove(key);
            }
            continue;
        }
        if (doc[key].as<String>().length() > 0)
        {
            json[key] = "changed";
            _preferences->putString(key, doc[key].as<String>());
        }
        else
        {
            json[key] = "removed";
            _preferences->remove(key);
        }
    }

    for(const auto& key : bytePrefs)
    {
        if(!doc[key].isNull() && doc[key].is<JsonVariant>())
        {
            String value = doc[key].as<String>();
            unsigned char tmpchar[32];
            for(int i=0; i<value.length(); i+=2)
            {
                tmpchar[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json[key] = "changed";
            _preferences->putBytes(key, (byte*)(&tmpchar), (value.length() / 2));
            memset(tmpchar, 0, sizeof(tmpchar));
        }
    }

    Preferences nukiBlePref;
    nukiBlePref.begin(PREFERENCE_NAME, false);

    if(!doc["bleAddressLock"].isNull())
    {
        if (doc["bleAddressLock"].as<String>().length() == 12)
        {
            String value = doc["bleAddressLock"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                currentBleAddress[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["bleAddressLock"] = "changed";
            nukiBlePref.putBytes(Nuki::BLE_ADDRESS_STORE_NAME, currentBleAddress, 6);
        }
    }
    if(!doc["secretKeyKLock"].isNull())
    {
        if (doc["secretKeyKLock"].as<String>().length() == 64)
        {
            String value = doc["secretKeyKLock"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                secretKeyK[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["secretKeyKLock"] = "changed";
            nukiBlePref.putBytes(Nuki::SECRET_KEY_STORE_NAME, secretKeyK, 32);
        }
    }
    if(!doc["authorizationIdLock"].isNull())
    {
        if (doc["authorizationIdLock"].as<String>().length() == 8)
        {
            String value = doc["authorizationIdLock"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                authorizationId[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["authorizationIdLock"] = "changed";
            nukiBlePref.putBytes(Nuki::AUTH_ID_STORE_NAME, authorizationId, 4);
        }
    }
    if(!doc["securityPinCodeLock"].isNull())
    {
        if(doc["securityPinCodeLock"].as<String>().length() > 0)
        {
            json["securityPinCodeLock"] = "changed";
            nukiBlePref.putBytes(Nuki::SECURITY_PINCODE_STORE_NAME, (byte*)(doc["securityPinCodeLock"].as<int>()), 2);
            //_nuki->setPin(doc["securityPinCodeLock"].as<int>());
        }
        else
        {
            json["securityPinCodeLock"] = "removed";
            unsigned char pincode[2] = {0x00};
            nukiBlePref.putBytes(Nuki::SECURITY_PINCODE_STORE_NAME, pincode, 2);
            //_nuki->setPin(0xffff);
        }
    }
    nukiBlePref.end();
    return json;
}