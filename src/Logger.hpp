#pragma once

#include <WString.h>
#include "EspMillis.h"
#include <Preferences.h>
#include "PreferencesKeys.h"
#include <SPIFFS.h>
#include <FS.h>
#include "ArduinoJson.h"
#include "ESP32_FTPClient.h"
#include <Print.h>
#include <atomic>

#ifdef DEBUG

class DebugLog : public Print
{
private:
  Preferences *_preferences;
  Print *_serial;

  bool isLogTooBig()
  {
    _serial->println(F("isLogTooBig() called - always returning false in debug mode"));
    return false;
  }

  void toFile(const String &deviceType, String message)
  {
    _serial->println(F("Logging message (debug mode):"));
    _serial->println(message);
  }

public:
  DebugLog(Print *serial, Preferences *prefs)
      : _serial(serial),
        _preferences(prefs)
  {
  }

  virtual ~DebugLog() {}

  void clearLog()
  {
    _serial->println(F("clearLog() called - no file operations in debug mode"));
  }

  size_t write(uint8_t c) override
  {
    return _serial->write(c);
  }

  size_t write(const uint8_t *buffer, size_t size) override
  {
    return _serial->write(buffer, size);
  }
};

extern DebugLog *Log;

#else // Production mode with SPIFFS logging

extern bool timeSynced; // Externe Variable zur Statusprüfung der SNTP-Synchronisation

class DebugLog : public Print
{
private:
  Print *_serial;
  Preferences *_preferences;
  String _logFile;
  String _buffer;
  int _maxMsgLen;
  int _maxLogFileSize;
  std::atomic<bool> _logFallBack{false};        // Flag to prevent LogFile overflow
  std::atomic<bool> _logBackupIsRunning{false}; // Flag to prevent write to LogFile while Backup is running

  // -----------------------------------------------------------
  // Converts milliseconds into days, hours, minutes, seconds (ISO 8601)
  // -----------------------------------------------------------

  void formatUptime(char *buffer, size_t size)
  {
    if (timeSynced)
    {
      // Wenn die Zeit synchronisiert wurde, hole die aktuelle Uhrzeit
      struct tm timeinfo;
      time_t now = time(NULL);
      localtime_r(&now, &timeinfo);

      snprintf(buffer, size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
               timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
               timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else
    {
      // Falls keine Zeit-Synchronisation vorliegt, verwende den alten Code (Uptime)
      int64_t millis = espMillis();
      int days = millis / (1000LL * 60 * 60 * 24);
      millis %= (1000LL * 60 * 60 * 24);
      int hours = millis / (1000LL * 60 * 60);
      millis %= (1000LL * 60 * 60);
      int minutes = millis / (1000LL * 60);
      millis %= (1000LL * 60);
      int seconds = millis / 1000;

      snprintf(buffer, size, "P%dDT%02dH%02dM%02dS", days, hours, minutes, seconds);
    }
  }

  // -----------------------------------------------------------
  // Checks if the log file exceeds the maximum allowed size
  // -----------------------------------------------------------
  bool isLogTooBig()
  {
    if (!SPIFFS.begin(true))
    {
      _logFallBack.store(true);
      println(F("[ERROR] SPIFFS not initialized!"));
      return false;
    }

    File f = SPIFFS.open(_logFile, FILE_READ);
    if (!f)
    {
      return false;
    }
    bool tooBig = (f.size() > _maxLogFileSize * 1024);
    f.close();
    return tooBig;
  }

  // -----------------------------------------------------------
  // Backup the Log File to Networkdrive
  // -----------------------------------------------------------
  bool backupLogToFTPServer()
  {
    bool expected = false;
    if (!_logBackupIsRunning.compare_exchange_strong(expected, true))
    {
      println(F("[INFO] FTP Backup is running"));
      return true;
    }

    if (!_preferences)
    {
      println(F("[ERROR] Preferences not initialized!"));
      _logBackupIsRunning.store(false);
      _logFallBack.store(true);
      return false;
    }

    String ftpServer = _preferences->getString(preference_log_backup_ftp_server, "");
    String ftpUser = _preferences->getString(preference_log_backup_ftp_user, "");
    String ftpPass = _preferences->getString(preference_log_backup_ftp_pwd, "");
    String ftpDir = "/" + _preferences->getString(preference_log_backup_ftp_dir, "");
    bool backupEnabled = _preferences->getBool(preference_log_backup_enabled, false);
    int backupIndex = _preferences->getInt(preference_log_backup_file_index, 0) + 1;
    _preferences->putInt(preference_log_backup_file_index, backupIndex);

    if (!backupEnabled || ftpServer.isEmpty() || ftpUser.isEmpty() || ftpPass.isEmpty())
    {
      _serial->println(F("[WARNING] Backup disabled or no FTP Server set."));
      clearLog();
      return false;
    }

    println(F("[INFO] Backing up log file to FTP Server..."));

    char ftpServerChar[ftpServer.length() + 1] = {0};
    char ftpUserChar[ftpUser.length() + 1] = {0};
    char ftpPassChar[ftpPass.length() + 1] = {0};

    ftpServer.toCharArray(ftpServerChar, sizeof(ftpServerChar));
    ftpUser.toCharArray(ftpUserChar, sizeof(ftpUserChar));
    ftpPass.toCharArray(ftpPassChar, sizeof(ftpPassChar));

    ESP32_FTPClient ftp(ftpServerChar, ftpUserChar, ftpPassChar);

    ftp.OpenConnection();
    if (!ftp.isConnected())
    {
      println(F("[ERROR] FTP connection failed!"));
      _logBackupIsRunning.store(false);
      _logFallBack.store(true);
      return false;
    }
    ftp.InitFile("Type A");
    ftp.ChangeWorkDir(ftpDir.c_str());

    int dotPos = _logFile.indexOf('.');
    String backupFilename;
    if (dotPos != -1)
    {
      backupFilename = _logFile.substring(0, dotPos) + String(backupIndex) + "." + _logFile.substring(dotPos + 1);
    }
    else
    {
      backupFilename = _logFile + String(backupIndex) + ".log";
    }
    ftp.DeleteFile(backupFilename.c_str());
    ftp.NewFile(backupFilename.c_str());

    if (!SPIFFS.begin(true))
    {
      _logFallBack.store(true);
      println(F("[ERROR] SPIFFS not initialized!"));
      _logBackupIsRunning.store(false);
      return false;
    }

    File f = SPIFFS.open(String("/") + _logFile, FILE_READ);
    if (!f)
    {
      _logFallBack.store(true);
      println(F("[ERROR] Failed to open log file for backup!"));
      ftp.CloseConnection();
      _logBackupIsRunning.store(false);
      return false;
    }

    const size_t bufferSize = 512; // Blocksize 512 Byte
    unsigned char buffer[bufferSize];

    while (f.available())
    {
      int bytesRead = f.read(buffer, bufferSize);
      if (bytesRead > 0)
      {
        ftp.WriteData(buffer, bytesRead);
      }
    }

    f.close();
    ftp.CloseFile();
    ftp.CloseConnection();

    println("[OK] FTP Backup successful!");
    _logBackupIsRunning.store(false);
    return true;
  }

  // -----------------------------------------------------------
  // Writes a JSON log entry to the log file.
  // If the file exceeds the max size, it will be cleared.
  // -----------------------------------------------------------
  void toFile(String message)
  {
    String msgType = "INFO"; // Standardwert

    // Trim message if it exceeds max length
    if (message.length() > _maxMsgLen)
    {
      message = message.substring(0, _maxMsgLen);
    }

    // Prüfe, ob die Nachricht mit [XYZ] beginnt
    if (message.startsWith("["))
    {
      int endBracket = message.indexOf("]");
      if (endBracket > 1)
      { // Mindestens 1 Zeichen zwischen [ und ]
        msgType = message.substring(1, endBracket);
        message = message.substring(endBracket + 1);
        message.trim();
      }
    }

    // Check file size, clear if too big
    if (isLogTooBig())
    {
      println(F("[INFO] Log file too large, attempt to backup to ftp Server..."));
      if (backupLogToFTPServer())
      {
        _serial->println(F("[INFO] Backup successful, clearing log file..."));
        clearLog();
      }
      else
      {
        println(F("[WARNING] Backup failed!"));
      }
    }

    // Create JSON log entry
    JsonDocument doc;
    char timeStr[25];
    formatUptime(timeStr, sizeof(timeStr));
    doc[F("timestamp")] = timeStr;
    doc[F("Type")] = msgType;
    doc[F("message")] = message;

    String line;
    serializeJson(doc, line);

    if (!SPIFFS.begin(true))
    {
      _logFallBack.store(true);
      println(F("[ERROR] SPIFFS not initialized!"));
      return;
    }

    // Append to log file
    File f = SPIFFS.open(String("/") + _logFile, FILE_APPEND);
    if (!f)
    {
      _logFallBack.store(true);
      println(F("[ERROR] Failed to open log file for appending"));
      return;
    }
    f.println(line);
    f.close();
  }

public:
  DebugLog(Print *serial, Preferences *prefs)
      : _serial(serial),
        _preferences(prefs)
  {

    if (!_preferences)
    {
      println(F("[ERROR] Preferences not initialized! Using defaults."));
      _logFile = "nukiBridge.log";
      _maxMsgLen = 80;
      _maxLogFileSize = 256;
    }
    else
    {

      _logFile = _preferences->getString(preference_log_filename, "nukiBridge.log");
      _maxMsgLen = _preferences->getInt(preference_log_max_msg_len, 80);
      _maxLogFileSize = _preferences->getInt(preference_log_max_file_size, 256); // in kb
    }

    _buffer.reserve(_maxMsgLen + 2); // Reserviert Speicher für bis _maxMsgLen Zeichen
  }

  virtual ~DebugLog() {}

  size_t write(const uint8_t *buffer, size_t size) override
  {
    if (_logBackupIsRunning.load() || _logFallBack.load())
      return _serial->write(buffer, size);

    if (size > 0 && size <= _maxMsgLen && buffer[size - 1] == '\n')
    {
      _buffer = String((const char *)buffer);
      if (!_buffer.isEmpty()) // Verhindert leere Logs
      {
        toFile(_buffer);
      }
      _buffer = "";
      return 1;
    }
    else
    {

      size_t n = 0;
      while (size--)
      {
        n += write(*buffer++);
      }
      return n;
    }
  }

  size_t write(uint8_t c) override
  {
    if (_logBackupIsRunning.load() || _logFallBack.load())
      return _serial->write(c);

    _buffer += (char)c;

    if (c == '\n')
    {
      if (!_buffer.isEmpty()) // Verhindert leere Logs
      {
        toFile(_buffer);
      }
      _buffer = "";
    }
    return 1;
  }
  // -----------------------------------------------------------
  // Deletes the current log file (if exists) and creates a new empty file
  // -----------------------------------------------------------
  void clearLog()
  {
    if (!SPIFFS.begin(true))
    {
      _logFallBack.store(true);
      println(F("[ERROR] SPIFFS not initialized!"));
      return;
    }
    if (SPIFFS.exists(_logFile))
    {
      SPIFFS.remove(_logFile);
    }

    File f = SPIFFS.open(_logFile, FILE_WRITE);
    if (f)
    {
      f.close();
    }
  }

  void resetFallBack()
  {
    _logFallBack.store(false);
  }
};

extern DebugLog *Log;

#endif // DEBUG
