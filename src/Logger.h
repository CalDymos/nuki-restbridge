// Logger.h
#pragma once

#include <WString.h>
#include "EspMillis.h"
#include <Preferences.h>
#include "PreferencesKeys.h"
#include <LittleFS.h>
#include <FS.h>
#include "ArduinoJson.h"
#include "ESP32_FTPClient.h"
#include <Print.h>
#include <atomic>
#include <freertos/semphr.h>

#define LOGGER_FILENAME (char *)"nukiBridge.log"

/**
 * @brief Logger class for serial and file-based logging with support for multiple log levels.
 *
 * The Logger supports output to serial and to LittleFS (when not in DEBUG_NUKIBRIDGE mode),
 * including JSON log entries, FTP backup, and various print/println overloads.
 */
class Logger : public Print
{
public:
    /**
     * @brief Log severity levels.
     */
    enum msgtype
    {
        MSG_TRACE,
        MSG_DEBUG,
        MSG_INFO,
        MSG_WARNING,
        MSG_ERROR,
        MSG_CRITICAL
    };

    /**
     * @brief Construct a new Logger object.
     *
     * @param serial Serial interface to mirror log messages.
     * @param prefs Preferences pointer for reading log settings.
     */
    Logger(Print *serial, Preferences *prefs);

    /**
     * @brief Destroy the Logger object.
     */
    virtual ~Logger();

    /**
     * @brief get current size of Log file in kb.
     */
    size_t getFileSize();

    /**
     * @brief Delete and recreate the log file.
     */
    void clear();

    /**
     * @brief Reset internal fallback state (used when LittleFS is not usable).
     */
    void resetFallBack();

    /**
     * @brief disables writing to the log file
     */
    void disableFileLog();

    /**
     * @brief Set the current log level.
     *
     * @param level Desired log level.
     */
    void setLevel(msgtype level);

    /**
     * @brief Get the current log level.
     *
     * @return msgtype Current log level.
     */
    msgtype getLevel();

    /**
     * @brief Convert log level enum to string.
     *
     * @param level Log level enum value.
     * @return String Representation as string.
     */
    String levelToString(msgtype level);

    /**
     * @brief Convert string to log level enum.
     *
     * @param levelStr Log level string.
     * @return msgtype Enum value.
     */
    msgtype stringToLevel(const String &levelStr);

    /**
     * @brief disables saving the log file to an FTP server
     */
    void disableBackup();

    // -------------------- Print/Write overrides --------------------

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
    size_t printf(const __FlashStringHelper *ifsh, ...);

    size_t print(const __FlashStringHelper *);
    size_t print(const String &);
    size_t print(const char[]);
    size_t print(char);
    size_t print(unsigned char, int = DEC);
    size_t print(int, int = DEC);
    size_t print(unsigned int, int = DEC);
    size_t print(long, int = DEC);
    size_t print(unsigned long, int = DEC);
    size_t print(long long, int = DEC);
    size_t print(unsigned long long, int = DEC);
    size_t print(double, int = 2);
    size_t print(const Printable &);
    size_t print(struct tm *timeinfo, const char *format = nullptr);

    size_t println(const __FlashStringHelper *);
    size_t println(const String &);
    size_t println(const char[]);
    size_t println(char);
    size_t println(unsigned char, int = DEC);
    size_t println(int, int = DEC);
    size_t println(unsigned int, int = DEC);
    size_t println(long, int = DEC);
    size_t println(unsigned long, int = DEC);
    size_t println(long long, int = DEC);
    size_t println(unsigned long long, int = DEC);
    size_t println(double, int = 2);
    size_t println(const Printable &);
    size_t println(struct tm *timeinfo, const char *format = nullptr);
    size_t println(void);

private:
    Print *_serial;                               // Serial interface for mirroring output
    Preferences *_preferences;                    // Preferences for config values
    String _logFile;                              // Path to log file
    String _buffer;                               // Internal buffer for streaming
    int _maxMsgLen;                               // Maximum message length
    int _maxLogFileSize;                          // Max log file size (KB)
    bool _backupEnabled;                          // Flag to enable backup of log file
    bool _fileWriteEnabled;                       // Flag to enable writing to Log file
    std::atomic<bool> _logFallBack{false};        // LittleFS failure fallback flag
    std::atomic<bool> _logBackupIsRunning{false}; // FTP backup activity flag
    SemaphoreHandle_t _bufferMutex;               // FreeRTOS semaphore for _buffer protection
    msgtype _currentLogLevel;                     // Active log level

    /**
     * @brief Write message as JSON to file and optionally to serial.
     *
     * @param message Message to be logged.
     */
    void toFile(String message);

    /**
     * @brief Check whether log file exceeds size limit.
     *
     * @return true If file is too large.
     * @return false Otherwise.
     */
    bool isFileTooBig();

    /**
     * @brief Attempt to upload current log file to FTP server.
     *
     * @return true On success or already running.
     * @return false On failure.
     */
    bool backupFileToFTPServer();

    /**
     * @brief Format uptime or current time to ISO-8601 string.
     *
     * @param buffer Output buffer.
     * @param size Buffer size.
     */
    void formatUptime(char *buffer, size_t size);

    /**
     * @brief Shared method for println with base formatting.
     *
     * @tparam T Type (int, long, etc.).
     * @param value Value to print.
     * @param base Base (DEC, HEX, BIN).
     * @return size_t Bytes written.
     */
    template <typename T>
    size_t printlnBase(T value, int base);
};

/// Global Logger instance
extern Logger *Log;

/// External flag indicating SNTP synchronization
extern bool timeSynced;
/// External flag indicating if LittleFS was successfully mounted
extern bool fsReady;