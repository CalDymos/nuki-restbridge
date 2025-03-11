#pragma once

#include <WString.h>
#include "EspMillis.h"
#include <Preferences.h>
#include "PreferencesKeys.h"
#include <SPIFFS.h>
#include <FS.h>
#include "ArduinoJson.h"
#include "ESP32_FTPClient.h"

#ifdef DEBUG
#include <Print.h>

class DebugLog : public Print {
private:

  bool isLogTooBig() {
    Serial.println(F("isLogTooBig() called - always returning false in debug mode"));
    return false;
  }
public:
  DebugLog(Preferences* prefs) {}

  virtual ~DebugLog() {}

  using Print::print;
  using Print::println;

  void clearLog() {
    Serial.println(F("clearLog() called - no file operations in debug mode"));
  }

  void toFile(const String& deviceType, String message) {
    if (message.length() > _maxMsgLen) {
      message = message.substring(0, _maxMsgLen);
    }

    Serial.println(F("Logging message (debug mode):"));
    Serial.println(message);
  }
};

extern DebugLog* Log;

#else  // Production mode with SPIFFS logging

class DebugLog {
private:
  Preferences* _preferences;
  String _logFile;
  int _maxMsgLen;
  int _maxLogFileSize;

  // -----------------------------------------------------------
  // Converts milliseconds into days, hours, minutes, seconds (ISO 8601)
  // -----------------------------------------------------------
  void formatUptime(char* buffer, size_t size) {
    int64_t millis = espMillis();

    int days = millis / (1000LL * 60 * 60 * 24);
    millis %= (1000LL * 60 * 60 * 24);

    int hours = millis / (1000LL * 60 * 60);
    millis %= (1000LL * 60 * 60);

    int minutes = millis / (1000LL * 60);
    millis %= (1000LL * 60);

    int seconds = millis / 1000;

    snprintf(buffer, size, "P%d:%02d:%02d:%02d", days, hours, minutes, seconds);
  }

  // -----------------------------------------------------------
  // Checks if the log file exceeds the maximum allowed size
  // -----------------------------------------------------------
  bool isLogTooBig() {
    File f = SPIFFS.open(_logFile, FILE_READ);
    if (!f) {
      return false;
    }
    bool tooBig = (f.size() > _maxLogFileSize);
    f.close();
    return tooBig;
  }

  // -----------------------------------------------------------
  // Backup the Log File to Networkdrive
  // -----------------------------------------------------------
  bool backupLogToFTPServer() {
    if (!_preferences) {
      println(F("[ERROR] Preferences not initialized!"));
      return false;
    }

    String ftpServer = _preferences->getString(preference_log_backup_ftp_server, "");
    String ftpUser = _preferences->getString(preference_log_backup_ftp_user, "");
    String ftpPass = _preferences->getString(preference_log_backup_ftp_pwd, "");
    String ftpDir = "/" + _preferences->getString(preference_log_backup_ftp_dir, "");
    bool backupEnabled = _preferences->getBool(preference_log_backup_enabled, false);
    int backupIndex = _preferences->getInt(preference_log_backup_file_index, 0) + 1;
    _preferences->putInt(preference_log_backup_file_index, backupIndex);

    if (!backupEnabled || ftpServer.isEmpty() || ftpUser.isEmpty() || ftpPass.isEmpty()) {
      println(F("[WARNING] Backup disabled or no FTP Server set."));
      return false;
    }

    println(F("[INFO] Backing up log file to FTP Server..."));

    char ftpServerChar[ftpServer.length() + 1];
    char ftpUserChar[ftpUser.length() + 1];
    char ftpPassChar[ftpPass.length() + 1];

    strcpy(ftpServerChar, ftpServer.c_str());
    strcpy(ftpUserChar, ftpUser.c_str());
    strcpy(ftpPassChar, ftpPass.c_str());

    ESP32_FTPClient ftp(ftpServerChar, ftpUserChar, ftpPassChar);

    ftp.OpenConnection();
    ftp.InitFile("Type A");
    ftp.ChangeWorkDir(ftpDir.c_str());
    int dotPos = _logFile.indexOf('.');
    String backupFilename;
    if (dotPos != -1) {
      backupFilename = _logFile.substring(0, dotPos) + String(backupIndex) + "." + _logFile.substring(dotPos + 1);
    } else {
      backupFilename = _logFile + String(backupIndex) + ".log";
    }
    ftp.DeleteFile(backupFilename.c_str());
    ftp.NewFile(backupFilename.c_str());

    File f = SPIFFS.open(String("/") + _logFile, FILE_READ);
    if (!f) {
      println(F("[ERROR] Failed to open log file for backup!"));
      ftp.CloseConnection();
      return false;
    }

    const size_t bufferSize = 512;
    unsigned char buffer[bufferSize];

    while (f.available()) {
      int bytesRead = f.read(buffer, bufferSize);
      if (bytesRead > 0) {
        ftp.WriteData(buffer, bytesRead);
      }
    }

    f.close();
    ftp.CloseFile();
    ftp.CloseConnection();

    println("[OK] FTP Backup successful!");
    return true;
  }
public:
  DebugLog(Preferences* prefs)
    : _preferences(prefs) {

    _logFile = _preferences->getString(preference_log_filename, "nukiBridge.log");
    _maxMsgLen = _preferences->getInt(preference_log_max_msg_len, 80);
    _maxLogFileSize = _preferences->getInt(preference_log_max_file_size, 256);  // in kb
  }

  virtual ~DebugLog() {}

  void printf(const char* format, ...) {}
  void printf(const __FlashStringHelper* ifsh, ...) {}

  void print(const __FlashStringHelper* ifsh) {}
  void print(const String&) {}
  void print(const char[]) {}
  void print(char) {}
  void print(unsigned char, int = 10) {}
  void print(int, int = 10) {}
  void print(unsigned int, int = 10) {}
  void print(long, int = 10) {}
  void print(unsigned long, int = 10) {}
  void print(long long, int = 10) {}
  void print(unsigned long long, int = 10) {}
  void print(double, int = 2) {}

  void println(const __FlashStringHelper* ifsh) {}
  void println(const String&) {}
  void println(const char[]) {}
  void println(char) {}
  void println(unsigned char, int = 10) {}
  void println(int, int = 10) {}
  void println(unsigned int, int = 10) {}
  void println(long, int = 10) {}
  void println(unsigned long, int = 10) {}
  void println(long long, int = 10) {}
  void println(unsigned long long, int = 10) {}
  void println(double, int = 2) {}
  void println(void) {}

  // -----------------------------------------------------------
  // Deletes the current log file (if exists) and creates a new empty file
  // -----------------------------------------------------------
  void clearLog() {
    if (SPIFFS.exists(_logFile)) {
      SPIFFS.remove(_logFile);
    }

    File f = SPIFFS.open(_logFile, FILE_WRITE);
    if (f) {
      f.close();
    }
  }

  // -----------------------------------------------------------
  // Writes a JSON log entry to the log file.
  // If the file exceeds the max size, it will be cleared.
  // -----------------------------------------------------------

  void toFile(const String& deviceType, String message) {
    // Trim message if it exceeds max length
    if (message.length() > _maxMsgLen) {
      message = message.substring(0, _maxMsgLen);
    }

    // Check file size, clear if too big
    if (isLogTooBig()) {
      println(F("[INFO] Log file too large, attempt to backup to ftp Server..."));
      backupLogToFTPServer();
      clearLog();
    }

    // Create JSON log entry
    JsonDocument doc;
    char timeStr[25];
    formatUptime(timeStr, sizeof(timeStr));
    doc[F("timestamp")] = timeStr;
    doc[F("deviceType")] = deviceType;
    doc[F("message")] = message;

    String line;
    serializeJson(doc, line);

    // Append to log file
    File f = SPIFFS.open(String("/") + _logFile, FILE_APPEND);
    if (!f) {
      println(F("[ERROR] Failed to open log file for appending"));
      return;
    }
    f.println(line);
    f.close();

    // Also print message to the log output
    println(message);
  }
};

extern DebugLog* Log;

#endif  // DEBUG
