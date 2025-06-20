#include "Logger.h"

#ifdef DEBUG_NUKIBRIDGE

Logger::Logger(Print *serial, Preferences *prefs)
    : _serial(serial), _preferences(prefs) {}

Logger::~Logger() {}

void Logger::clear()
{
  _serial->println(F("[TRACE] clearLog() called - no file operations in debug mode"));
}

void Logger::setLogLevel(msgtype level)
{
  _serial->printf(F("[TRACE] setLogLevel() called : %s\n"), levelToString(level));
}

Logger::msgtype Logger::getLogLevel()
{
  return MSG_TRACE;
}

const char *Logger::levelToString(msgtype level)
{
  return "TRACE";
}

size_t Logger::write(uint8_t c)
{
  return _serial->write(c);
}

size_t Logger::write(const uint8_t *buffer, size_t size)
{
  return _serial->write(buffer, size);
}

bool Logger::isLogTooBig()
{
  _serial->println(F("[TRACE] isLogTooBig() called - always returning false in debug mode"));
  return false;
}

void Logger::toQueue(const String &message)
{
  _serial->println(F("Logging message (debug mode):"));
  _serial->println(message);
}

void Logger::disableFileLog()
{
  _serial->println(F("writiing to Log file disabled!"));
}

#else // LittleFS based logging

Logger::Logger(Print *serial, Preferences *prefs)
    : _serial(serial), _preferences(prefs)
{

  if (!_preferences)
  {
    println(F("[ERROR] Preferences not initialized! Using defaults."));
    _logFile = LOGGER_FILENAME;
    _maxLogFileSize = 256;
    _currentLogLevel = MSG_INFO;
    _backupEnabled = false;
  }
  else
  {
    _backupEnabled = _preferences->getBool(preference_log_backup_enabled, false);
    _logFile = LOGGER_FILENAME;
    _currentLogLevel = (msgtype)_preferences->getInt(preference_log_level);

    _maxLogFileSize = _preferences->getInt(preference_log_max_file_size); // in kb

    if (_maxLogFileSize == 0)
    {
      _maxLogFileSize = 256;
      _preferences->putInt(preference_log_max_file_size, _maxLogFileSize);

      // _maxLogFileSize == 0 => first start of Bridge
      if (_currentLogLevel == 0)
      {
        _currentLogLevel = msgtype::MSG_INFO;
        _preferences->putInt(preference_log_level, (int)_currentLogLevel);
      }
    }
  }
  _fileWriteEnabled = true;
  _buffer.reserve(LOG_MSG_MAX_LEN + 2); // Reserves memory for up to _maxMsgLen characters

  _bufferMutex = xSemaphoreCreateMutex();

  if (!fsReady)
    _logFallBack.store(true);

  _logQueue = xQueueCreate(LOG_QUEUE_MAX_ENTRYS, sizeof(LogMessage));
  xTaskCreatePinnedToCore(Logger::queueTask, "LoggerTask", 4096, this, 1, &_queueTaskHandle, 0);
}

Logger::~Logger()
{
  if (_bufferMutex)
  {
    vSemaphoreDelete(_bufferMutex);
  }
}

size_t Logger::write(uint8_t c)
{
  if (!_fileWriteEnabled || _logBackupIsRunning.load() || _logFallBack.load() || (xPortInIsrContext() || !xPortCanYield()))
    return _serial->write(c);

  if (c == '\0')
    return 0; // IGNORE null-terminator

  if (xSemaphoreTake(_bufferMutex, portMAX_DELAY))
  {
    _buffer += (char)c;
    if (c == '\n')
    {
      _buffer.trim();
      if (!_buffer.isEmpty())
        toQueue(_buffer);
      _buffer = "";
    }
    xSemaphoreGive(_bufferMutex);
  }
  return 1;
}

size_t Logger::write(const uint8_t *buffer, size_t size)
{
  if (!_fileWriteEnabled || _logBackupIsRunning.load() || _logFallBack.load() || (xPortInIsrContext() || !xPortCanYield()))
    return _serial->write(buffer, size);

  size_t n = 0;
  if (xSemaphoreTake(_bufferMutex, portMAX_DELAY))
  {
    if (size == 2 && buffer[0] == '\r' && buffer[1] == '\n')
    {
      _buffer.trim();
      if (!_buffer.isEmpty())
        toQueue(_buffer);
      _buffer = "";
      n = 2;
    }
    else if (size > 0 && size <= LOG_MSG_MAX_LEN && buffer[size - 1] == '\n')
    {
      _buffer = "";
      _buffer.reserve(size);
      for (size_t i = 0; i < size; ++i)
      {
        if (buffer[i] == '\0')
          continue; // Nullbytes ignorieren
        _buffer += (char)buffer[i];
      }

      _buffer.trim();
      if (!_buffer.isEmpty())
        toQueue(_buffer);
      _buffer = "";
      n = size;
    }
    else
    {
      for (size_t i = 0; i < size; ++i)
      {
        if (buffer[i] != '\0')
          _buffer += (char)buffer[i];
        if (buffer[i] == '\n')
        {
          _buffer.trim();
          if (!_buffer.isEmpty())
            toQueue(_buffer);
          _buffer = "";
        }
      }
      n = size;
    }

    xSemaphoreGive(_bufferMutex);
  }

  return n;
}

size_t Logger::printf(const __FlashStringHelper *ifsh, ...)
{
  char buf[LOG_MSG_MAX_LEN + 1]; // max Msg len + zero termination
  va_list arg;
  va_start(arg, ifsh);
  const char *format = (reinterpret_cast<const char *>(ifsh));
  size_t len = vsnprintf(buf, sizeof(buf), format, arg);
  buf[sizeof(buf) - 1] = '\0'; // For safety reasons
  va_end(arg);
  if (len > 0)
  {
    write((const uint8_t *)buf, len);
  }
  return len;
}

size_t Logger::printf(const char *format, ...)
{
  char buf[LOG_MSG_MAX_LEN + 1]; // max Msg len + zero termination
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buf, sizeof(buf), format, args);
  buf[sizeof(buf) - 1] = '\0'; // For safety reasons
  va_end(args);
  if (len > 0)
  {
    write((const uint8_t *)buf, (size_t)len);
  }
  return (size_t)(len > 0 ? len : 0);
}

size_t Logger::print(const __FlashStringHelper *ifsh)
{
  return write(reinterpret_cast<const uint8_t *>(ifsh), strlen_P(reinterpret_cast<const char *>(ifsh)));
}

size_t Logger::print(const char *str)
{
  return write((const uint8_t *)str, strlen(str));
}

size_t Logger::print(const String &s)
{
  return write((const uint8_t *)s.c_str(), s.length());
}

size_t Logger::print(char c)
{
  return write((uint8_t)c);
}

size_t Logger::print(unsigned char value, int base)
{
  return print((unsigned long)value, base);
}

size_t Logger::print(int value, int base)
{
  char buf[16];
  if (base == DEC)
    snprintf(buf, sizeof(buf), "%d", value);
  else if (base == HEX)
    snprintf(buf, sizeof(buf), "%x", value);
  else if (base == BIN)
  {
    char *p = buf + sizeof(buf) - 1;
    *p-- = '\0';
    unsigned int uval = static_cast<unsigned int>(value);
    do
    {
      *p-- = (uval & 1) ? '1' : '0';
      uval >>= 1;
    } while (uval && p >= buf);
    return write((const uint8_t *)(p + 1), strlen(p + 1));
  }
  else
    snprintf(buf, sizeof(buf), "%d", value);

  return write((const uint8_t *)buf, strlen(buf));
}

size_t Logger::print(unsigned int value, int base)
{
  char buf[16];
  if (base == DEC)
    snprintf(buf, sizeof(buf), "%u", value);
  else if (base == HEX)
    snprintf(buf, sizeof(buf), "%x", value);
  else if (base == BIN)
  {
    char *p = buf + sizeof(buf) - 1;
    *p-- = '\0';
    unsigned int uval = value;
    do
    {
      *p-- = (uval & 1) ? '1' : '0';
      uval >>= 1;
    } while (uval && p >= buf);
    return write((const uint8_t *)(p + 1), strlen(p + 1));
  }
  else
    snprintf(buf, sizeof(buf), "%u", value);

  return write((const uint8_t *)buf, strlen(buf));
}

size_t Logger::print(long value, int base)
{
  return print((long long)value, base);
}

size_t Logger::print(unsigned long value, int base)
{
  return print((unsigned long long)value, base);
}

size_t Logger::print(long long value, int base)
{
  char buf[32]; // space for 64 bit signed integer + NULL
  if (base == DEC)
  {
    snprintf(buf, sizeof(buf), "%lld", value);
  }
  else if (base == HEX)
  {
    snprintf(buf, sizeof(buf), "%llx", value);
  }
  else if (base == BIN)
  {
    // Manual binary conversion
    uint64_t uval = static_cast<uint64_t>(value);
    char *p = buf + sizeof(buf) - 1;
    *p-- = '\0';
    do
    {
      *p-- = (uval & 1) ? '1' : '0';
      uval >>= 1;
    } while (uval && p >= buf);
    return write((const uint8_t *)(p + 1), strlen(p + 1));
  }
  else
  {
    // Standard fallback to decimal
    snprintf(buf, sizeof(buf), "%lld", value);
  }
  return write((const uint8_t *)buf, strlen(buf));
}

size_t Logger::print(unsigned long long value, int base)
{
  char buffer[32];
  switch (base)
  {
  case BIN:
  case OCT:
    // // No direct format for binary/octal with uint64_t in snprintf
    return print((unsigned long)value, base); // Fallback to 32 Bit
  case HEX:
    snprintf(buffer, sizeof(buffer), "%llX", value);
    break;
  case DEC:
  default:
    snprintf(buffer, sizeof(buffer), "%llu", value);
    break;
  }
  return write((const uint8_t *)buffer, strlen(buffer));
}

size_t Logger::print(double number, int digits)
{
  char buf[32];
  dtostrf(number, 0, digits, buf);
  return write((const uint8_t *)buf, strlen(buf));
}

size_t Logger::print(const Printable &obj)
{
  return obj.printTo(*this);
}

size_t Logger::print(struct tm *timeinfo, const char *format)
{
  char buf[64];
  strftime(buf, sizeof(buf), format ? format : "%F %T", timeinfo);
  return write((const uint8_t *)buf, strlen(buf));
}

size_t Logger::println(const __FlashStringHelper *ifsh)
{
  size_t n = write(reinterpret_cast<const uint8_t *>(ifsh), strlen_P(reinterpret_cast<const char *>(ifsh)));
  ;
  n += write((const uint8_t *)"\r\n", 2);
  return n;
}

size_t Logger::println(const char *str)
{
  size_t n = write((const uint8_t *)str, strlen(str));
  n += write((const uint8_t *)"\r\n", 2); // schreibt nur \r\n
  return n;
}

size_t Logger::println(const String &s)
{
  size_t n = print(s);
  n += write((const uint8_t *)"\r\n", 2); // schreibt \r\n
  return n;
}

size_t Logger::println(char c)
{
  size_t n = print(c);
  return n + println();
}

size_t Logger::println(unsigned char value, int base)
{
  size_t n = print(value, base);
  return n + println();
}

template <typename T>
size_t Logger::printlnBase(T value, int base)
{
  size_t n = print(value, base);
  return n + write((const uint8_t *)"\r\n", 2);
}

size_t Logger::println(int value, int base) { return printlnBase(value, base); }
size_t Logger::println(unsigned int value, int base) { return printlnBase(value, base); }
size_t Logger::println(long value, int base) { return printlnBase(value, base); }
size_t Logger::println(unsigned long value, int base) { return printlnBase(value, base); }
size_t Logger::println(long long value, int base) { return printlnBase(value, base); }
size_t Logger::println(unsigned long long value, int base) { return printlnBase(value, base); }

size_t Logger::println(double number, int digits)
{
  size_t n = print(number, digits);
  return n + println();
}

size_t Logger::println(const Printable &obj)
{
  size_t n = print(obj);
  return n + println();
}

size_t Logger::println(struct tm *timeinfo, const char *format)
{
  size_t n = print(timeinfo, format);
  return n + println();
}

size_t Logger::println(void)
{
  return write((const uint8_t *)"\r\n", 2);
}

void Logger::clear()
{

  if (!lock())
  {
    _serial->println(F("[WARNING] Could not acquire lock for clearing log"));
    return;
  }

  if (!fsReady)
  {
    _serial->println(F("[ERROR] LittleFS not initialized!"));
    unlock();
    return;
  }
  if (LittleFS.exists(_logFile))
  {
    LittleFS.remove(_logFile);
  }

  File f = LittleFS.open(_logFile, FILE_WRITE);
  if (f)
  {
    f.close();
    unlock();
  }
}

void Logger::resetFallBack()
{
  _logFallBack.store(false);
}

const char *Logger::levelToString(msgtype level)
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

Logger::msgtype Logger::stringToLevel(const char *levelStr)
{
  if (strcmp(levelStr, "TRACE") == 0)
    return MSG_TRACE;
  if (strcmp(levelStr, "DEBUG") == 0)
    return MSG_DEBUG;
  if (strcmp(levelStr, "INFO") == 0)
    return MSG_INFO;
  if (strcmp(levelStr, "WARNING") == 0)
    return MSG_WARNING;
  if (strcmp(levelStr, "ERROR") == 0)
    return MSG_ERROR;
  if (strcmp(levelStr, "CRITICAL") == 0)
    return MSG_CRITICAL;
  return (msgtype)-1; // if unknown type
}

void Logger::setLevel(msgtype level)
{
  _currentLogLevel = level;
}

Logger::msgtype Logger::getLevel()
{
  return _currentLogLevel;
}

void Logger::disableBackup()
{
  _backupEnabled = false;
}

void Logger::formatUptime(char *buffer, size_t size)
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

bool Logger::isFileTooBig()
{
  return (getFileSize() > (_maxLogFileSize * 1024));
}

size_t Logger::getFileSize()
{
  if (!fsReady)
  {
    println(F("[ERROR] LittleFS not initialized!"));
    return 0;
  }

  File f = LittleFS.open(String("/") + _logFile, FILE_READ);
  if (!f)
  {
    return 0;
  }
  size_t size = f.size();
  f.close();
  return size;
}

bool Logger::backupFileToFTPServer()
{
  bool expected = false;
  if (!_logBackupIsRunning.compare_exchange_strong(expected, true))
  {
    println(F("[INFO] FTP Backup is running"));
    return true;
  }

  if (!_backupEnabled)
  {
    _logBackupIsRunning.store(false);
    return false;
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
  int backupIndex = _preferences->getInt(preference_log_backup_file_index, 0) + 1;
  if (backupIndex > 100)
    backupIndex = 1;
  _preferences->putInt(preference_log_backup_file_index, backupIndex);

  if (ftpServer.isEmpty() || ftpUser.isEmpty() || ftpPass.isEmpty())
  {
    _serial->println(F("[WARNING] Backup disabled or no FTP Server set."));
    clear();
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

  if (!fsReady)
  {
    println(F("[ERROR] LittleFS not initialized!"));
    _logBackupIsRunning.store(false);
    return false;
  }

  File f = LittleFS.open(String("/") + _logFile, FILE_READ);
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

void Logger::disableFileLog()
{
  _fileWriteEnabled = false;
}

void Logger::toQueue(const String &message)
{
  LogMessage entry;
  strlcpy(entry.msg, message.c_str(), sizeof(entry.msg));
  xQueueSend(_logQueue, &entry, 0); // nicht blockierend
}

void Logger::queueTask(void *param)
{
  Logger *self = static_cast<Logger *>(param);
  self->processQueue();
}

TaskHandle_t Logger::getQueueTaskHandle()
{
  return _queueTaskHandle;
}

bool Logger::lock()
{
  return (_bufferMutex && xSemaphoreTake(_bufferMutex, pdMS_TO_TICKS(500)));
}

void Logger::unlock()
{
  if (_bufferMutex)
    xSemaphoreGive(_bufferMutex);
}

void Logger::processQueue()
{
  char msgType[11] = "INFO"; // Default

  // 25 (timeStr) + 3 (sep1 " | ") + 16 (max msgType) + 3 (sep2 " | ") + _maxMsgLen + 4 (\r\n + \0)
  int logLineSize = 25 + 3 + LOG_MSG_MAX_LEN + 3 + 16 + 4;
  char logLine[logLineSize + 1];

  LogMessage entry;

  // Never exit this task, always continue to process next log message
  while (true)
  {
    if (xQueueReceive(_logQueue, &entry, portMAX_DELAY))
    {
      // Check if entry.msg starts with [LEVEL]
      if (entry.msg[0] == '[')
      {
        char *end = strchr(entry.msg, ']');
        if (end && end > entry.msg + 1)
        {
          size_t typeLen = end - entry.msg - 1;
          if (typeLen > 0 && typeLen < 10)
          {
            memcpy(msgType, entry.msg + 1, typeLen);
            msgType[typeLen] = '\0';
          }

          // shift message content left (in-place) after ]
          size_t remainingLen = strlen(end + 1);
          memmove(entry.msg, end + 1, remainingLen + 1); // include '\0'

          // trim leading spaces
          char *start = entry.msg;
          while (*start == ' ')
            ++start;
          if (start != entry.msg)
          {
            memmove(entry.msg, start, strlen(start) + 1);
          }
        }
      }

      // Convert msgtype from string
      msgtype level = stringToLevel(msgType);

      // Skip unknown level unless current log level is DEBUG or TRACE
      if (level == (msgtype)-1 && _currentLogLevel > MSG_DEBUG)
      {
        continue;
      }

      // Skip messages below current log level
      if (level != (msgtype)-1 && level < _currentLogLevel)
      {
        continue;
      }

      // additional output on the serial interface in debug or trace mode
      if (_currentLogLevel == MSG_TRACE || _currentLogLevel == MSG_DEBUG)
        _serial->println(entry.msg);

      // Check file size, clear if too big
      if (isFileTooBig())
      {
        println(F("[INFO] Log file too large, attempt to backup to ftp Server..."));
        if (backupFileToFTPServer())
        {
          _serial->println(F("[INFO] Backup successful, clearing log file..."));
          clear();
        }
        else
        {
          println(F("[WARNING] Backup failed!"));
        }
      }

      char timeStr[25] = {0};
      formatUptime(timeStr, sizeof(timeStr));

      if (!fsReady)
      {
        _serial->println(F("[ERROR] LittleFS not initialized!"));
        continue;
      }

      // Append to log file
      File f = LittleFS.open(String("/") + _logFile, FILE_APPEND);
      if (!f)
      {
        _logFallBack.store(true);
        _serial->println(F("[ERROR] Failed to open log file for appending"));
        continue;
      }

      snprintf(logLine, logLineSize, "%s | %s | %s\r\n", timeStr, msgType, entry.msg);
      f.write((const uint8_t *)logLine, strlen(logLine));
      f.close();
    }
  }
}

#endif