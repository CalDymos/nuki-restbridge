#include "Logger.h"

#ifdef DEBUG_NUKIBRIDGE

Logger::Logger(Print *serial, Preferences *prefs)
    : _serial(serial), _preferences(prefs) {}

Logger::~Logger() {}

void Logger::clearLog()
{
  _serial->println(F("[TRACE] clearLog() called - no file operations in debug mode"));
}

void Logger::setLogLevel(msgtype level)
{
  _serial->printf(F("[TRACE] setLogLevel() called : %d\n"), logLevelToString(level));
}

Logger::msgtype Logger::getLogLevel()
{
  return MSG_TRACE;
}

String Logger::logLevelToString(msgtype level)
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

void Logger::toFile(const String &deviceType, String message)
{
  _serial->println(F("Logging message (debug mode):"));
  _serial->println(message);
}

void Logger::disableFileLog(){
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
    _maxMsgLen = 128;
    _maxLogFileSize = 256;
    _currentLogLevel = MSG_INFO;
    _backupEnabled = false;
  }
  else
  {
    _backupEnabled = _preferences->getBool(preference_log_backup_enabled, false);
    _logFile = LOGGER_FILENAME;
    _maxMsgLen = _preferences->getInt(preference_log_max_msg_len, 128);
    _maxLogFileSize = _preferences->getInt(preference_log_max_file_size, 256); // in kb
    _currentLogLevel = (msgtype)_preferences->getInt(preference_log_level, 2);
  }
  _fileWriteEnabled = true;
  _buffer.reserve(_maxMsgLen + 2); // Reserves memory for up to _maxMsgLen characters
}

Logger::~Logger() {}

size_t Logger::write(uint8_t c)
{
  if (!_fileWriteEnabled || _logBackupIsRunning.load() || _logFallBack.load())
    return _serial->write(c);

  _buffer += (char)c;
  if (c == '\n')
  {
    _buffer.trim();
    if (!_buffer.isEmpty())
      toFile(_buffer);
    _buffer = "";
  }
  return 1;
}

size_t Logger::write(const uint8_t *buffer, size_t size)
{
  if (!_fileWriteEnabled || _logBackupIsRunning.load() || _logFallBack.load())
    return _serial->write(buffer, size);

  if (size == 2 && buffer[0] == '\r' && buffer[1] == '\n')
  {
    _buffer.trim();
    if (!_buffer.isEmpty())
      toFile(_buffer);
    _buffer = "";
    return 2;
  }
  else if (size > 0 && size <= _maxMsgLen && buffer[size - 1] == '\n')
  {
    _buffer = String((const char *)buffer);
    _buffer.trim();
    if (!_buffer.isEmpty())
      toFile(_buffer);
    _buffer = "";
    return 1;
  }
  else
  {
    size_t n = 0;
    while (size--)
      n += write(*buffer++);
    return n;
  }
}

size_t Logger::printf(const __FlashStringHelper *ifsh, ...)
{
  char buf[_maxMsgLen + 1]; // max Msg len + zero termination
  va_list arg;
  va_start(arg, ifsh);
  const char *format = (reinterpret_cast<const char *>(ifsh));
  size_t len = vsnprintf(buf, sizeof(buf), format, arg);
  va_end(arg);
  if (len > 0)
  {
    write((const uint8_t *)buf, len);
  }
  return len;
}

size_t Logger::printf(const char *format, ...)
{
  char buf[_maxMsgLen + 1]; // max Msg len + zero termination
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buf, sizeof(buf), format, args);
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
  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
  {
    _logFallBack.store(true);
    println(F("[ERROR] LittleFS not initialized!"));
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
  }
}

void Logger::resetFallBack()
{
  _logFallBack.store(false);
}

String Logger::levelToString(msgtype level)
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

Logger::msgtype Logger::stringToLevel(const String &levelStr)
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
  return (getFileSize() > _maxLogFileSize);
}

size_t Logger::getFileSize()
{
  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
  {
    _logFallBack.store(true);
    println(F("[ERROR] LittleFS not initialized!"));
    return 0;
  }

  File f = LittleFS.open(_logFile, FILE_READ);
  if (!f)
  {
    return 0;
  }
  size_t size = f.size() * 1024;
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
  if (_backupEnabled)
  {

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

    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
    {
      _logFallBack.store(true);
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
  return false;
}

void Logger::disableFileLog(){
  _fileWriteEnabled = false;
}

void Logger::toFile(String message)
{
  String msgType = levelToString(_currentLogLevel); // Default value

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
  msgtype level = stringToLevel(msgType);

  // Check whether this log level is within the set level
  if (_currentLogLevel > level && (_currentLogLevel != (int)MSG_DEBUG && level == -1))
  {
    return; // Do not log message if it is not relevant
  }

  // additional output on the serial interface in debug or trace mode
  if (_currentLogLevel == MSG_TRACE || _currentLogLevel == MSG_DEBUG)
    Serial.println(message);

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

  // Create JSON log entry
  JsonDocument doc;
  char timeStr[25];
  formatUptime(timeStr, sizeof(timeStr));
  doc[F("timestamp")] = timeStr;
  doc[F("Type")] = msgType;
  doc[F("message")] = message;

  String line;
  serializeJson(doc, line);

  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
  {
    _logFallBack.store(true);
    println(F("[ERROR] LittleFS not initialized!"));
    return;
  }

  // Append to log file
  File f = LittleFS.open(String("/") + _logFile, FILE_APPEND);
  if (!f)
  {
    _logFallBack.store(true);
    println(F("[ERROR] Failed to open log file for appending"));
    return;
  }
  f.println(line);
  f.close();
}

#endif