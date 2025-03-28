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

extern bool timeSynced; // External variable for checking the status of SNTP synchronization

class DebugLog : public Print
{

public:
  enum msgtype
  {
    MSG_TRACE,    // Log everything
    MSG_DEBUG,    // Everything except TRACE
    MSG_INFO,     // Everything except DEBUG and TRACE
    MSG_WARNING,  // Everything except INFO, DEBUG and TRACE
    MSG_ERROR,    // Log ERROR and CRITICAL only
    MSG_CRITICAL, // Log CRITICAL only
  };

#ifdef DEBUG_NUKIBRIDGE
  DebugLog(Print *serial, Preferences *prefs)
      : _serial(serial),
        _preferences(prefs)
  {
  }

  virtual ~DebugLog() {}

  void clearLog()
  {
    _serial->println(F("[TRACE] clearLog() called - no file operations in debug mode"));
  }

  void setLogLevel(msgtype level)
  {
    _serial->printf(F("[TRACE] setLogLevel() called : %d\n"), logLevelToString(level));
  }

  msgtype getLogLevel()
  {
    return MSG_TRACE;
  }

  String logLevelToString(msgtype level)
  {
    return "TRACE";
  }

  size_t write(uint8_t c) override
  {
    return _serial->write(c);
  }

  size_t write(const uint8_t *buffer, size_t size) override
  {
    return _serial->write(buffer, size);
  }

private:
  Preferences *_preferences;
  Print *_serial;

  bool isLogTooBig()
  {
    _serial->println(F("[TRACE] isLogTooBig() called - always returning false in debug mode"));
    return false;
  }

  void toFile(const String &deviceType, String message)
  {
    _serial->println(F("Logging message (debug mode):"));
    _serial->println(message);
  }
};

extern DebugLog *Log;

#else // Production mode with SPIFFS logging

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
      _currentLogLevel = MSG_INFO;
    }
    else
    {

      _logFile = _preferences->getString(preference_log_filename, "nukiBridge.log");
      _maxMsgLen = _preferences->getInt(preference_log_max_msg_len, 80);
      _maxLogFileSize = _preferences->getInt(preference_log_max_file_size, 256); // in kb
      _currentLogLevel = (msgtype)_preferences->getInt(preference_log_level, 2);
    }

    _buffer.reserve(_maxMsgLen + 2); // Reserves memory for up to _maxMsgLen characters
  }

  virtual ~DebugLog() {}

  size_t write(const uint8_t *buffer, size_t size) override
  {
    if (_logBackupIsRunning.load() || _logFallBack.load())
      return _serial->write(buffer, size);

    if (size > 0 && size <= _maxMsgLen && buffer[size - 1] == '\n')
    {
      _buffer = String((const char *)buffer);
      if (!_buffer.isEmpty()) // Prevents empty logs
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
      if (!_buffer.isEmpty()) // Prevents empty logs
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

  String logLevelToString(msgtype level)
  {
    switch (level)
    {
    case MSG_TRACE:
      return "TRACE";
    case MSG_DEBUG:
      return "DEBUG";
    case MSG_INFO:
      return "INFO";
    case MSG_WARNING:
      return "WARNING";
    case MSG_ERROR:
      return "ERROR";
    case MSG_CRITICAL:
      return "CRITICAL";
    default:
      return "UNKNOWN";
    }
  }

  msgtype stringToLogLevel(const String &levelStr)
  {
    if (levelStr == "TRACE")
      return MSG_TRACE;
    if (levelStr == "DEBUG")
      return MSG_DEBUG;
    if (levelStr == "INFO")
      return MSG_INFO;
    if (levelStr == "WARNING")
      return MSG_WARNING;
    if (levelStr == "ERROR")
      return MSG_ERROR;
    if (levelStr == "CRITICAL")
      return MSG_CRITICAL;
    return (msgtype)-1; // if unknown type
  }

  void setLogLevel(msgtype level)
  {
    _currentLogLevel = level;
  }

  msgtype getLogLevel()
  {
    return _currentLogLevel;
  }

private:
  Print *_serial;
  Preferences *_preferences;
  String _logFile;
  String _buffer;
  int _maxMsgLen;
  int _maxLogFileSize;
  std::atomic<bool> _logFallBack{false};        // Flag to prevent LogFile overflow
  std::atomic<bool> _logBackupIsRunning{false}; // Flag to prevent write to LogFile while Backup is running
  msgtype _currentLogLevel;                     // Default value (debug level)

  // -----------------------------------------------------------
  // Converts milliseconds into days, hours, minutes, seconds (ISO 8601)
  // -----------------------------------------------------------

  void formatUptime(char *buffer, size_t size)
  {
    if (timeSynced)
    {
      // If the time has been synchronized, get the current time
      struct tm timeinfo;
      time_t now = time(NULL);
      localtime_r(&now, &timeinfo);

      snprintf(buffer, size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
               timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
               timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else
    {
      // If there is no time synchronization, use the old code (Uptime)
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

    println("[INFO] FTP Backup successful!");
    _logBackupIsRunning.store(false);
    return true;
  }

  // -----------------------------------------------------------
  // Writes a JSON log entry to the log file.
  // If the file exceeds the max size, it will be cleared.
  // -----------------------------------------------------------
  void toFile(String message)
  {
    String msgType = logLevelToString(_currentLogLevel); // Default value

    // Trim message if it exceeds max length
    if (message.length() > _maxMsgLen)
    {
      message = message.substring(0, _maxMsgLen);
    }

    // Check whether the message begins with [XYZ] and extract the type
    if (message.startsWith("["))
    {
      int endBracket = message.indexOf("]");
      if (endBracket > 1)
      { // At least 1 character between [ and ]
        msgType = message.substring(1, endBracket);
        message = message.substring(endBracket + 1); // Extract the rest of the message
        message.trim();
      }
    }

    // Convert msgtype from string
    msgtype level = stringToLogLevel(msgType);

    // Check whether this log level is within the set level
    if (_currentLogLevel > level && (_currentLogLevel != (int)MSG_DEBUG && level == -1))
    {
      return; // Do not log message if it is not relevant
    }

    // additional output on the serial interface in debug or trace mode
    if (_currentLogLevel == MSG_TRACE || _currentLogLevel == MSG_DEBUG)
      Serial.println(message);

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
};

extern DebugLog *Log;

#endif // DEBUG_NUKIBRIDGE
