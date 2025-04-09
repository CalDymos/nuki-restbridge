#pragma once

/**
 * @brief Represents the reason why the ESP32 was restarted or shut down.
 */
enum class RestartReason
{
    RequestedViaApi,
    RequestedViaWebCfgServer,
    BLEError,
    BLEBeaconWatchdog,
    RestartOnDisconnectWatchdog,
    RestartIntervalWatchdog,
    NetworkTimeoutWatchdog,
    WifiInitFailed,
    ReconfigureWifi,
    ReconfigureETH,
    NetworkDeviceCriticalFailure,
    NetworkDeviceCriticalFailureNoWifiFallback,
    ConfigurationUpdated,
    RestartTimer,
    DeviceUnpaired,
    NukiBridgeReset,
    FactoryReset,
    ReconfigureWebCfgServer,
    DisableNetworkIfNotConnected,
    SafeShutdownRequestViaApi,
    SafeShutdownRequestViaWebCfgServer,
    NotApplicable
};

/// @brief Magic value to indicate a valid restart reason
#define RESTART_REASON_VALID_DETECT 0xa00ab00bc00bd00d

/// @brief Stores the current restart reason as int
extern int restartReason;

/// @brief Used to verify whether the stored restart reason is valid
extern uint64_t restartReasonValidDetect;

/// @brief Stores the parsed restart reason as enum valu
extern RestartReason currentRestartReason;

/// @brief Indicates whether the restart reason is valid
extern bool restartReason_isValid;

/**
 * @brief Safely shuts down the ESP32 and enters deep sleep.
 *
 * @param reason The reason for shutting down.
 */
inline static void safeShutdownESP(RestartReason reason)
{
    restartReason = (int)reason;
    restartReasonValidDetect = RESTART_REASON_VALID_DETECT;
    SPIFFS.end();
    delay(10);                        // to ensure that all pending write operations are completed
    esp_deep_sleep_start();           // ESP goes to sleep
}

/**
 * @brief Restarts the ESP32 with a specified reason.
 *
 * @param reason The reason for restarting.
 */
inline static void restartEsp(RestartReason reason)
{
    restartReason = (int)reason;
    restartReasonValidDetect = RESTART_REASON_VALID_DETECT;
    SPIFFS.end();
    delay(10); // to ensure that all pending write operations are completed
    ESP.restart();
}

/**
 * @brief Initializes the restart reason after boot by checking the magic number.
 */
inline static void initializeRestartReason()
{
    uint64_t cmp = RESTART_REASON_VALID_DETECT;
    restartReason_isValid = (restartReasonValidDetect == cmp);
    if (restartReason_isValid)
    {
        currentRestartReason = (RestartReason)restartReason;
        memset(&restartReasonValidDetect, 0, sizeof(restartReasonValidDetect));
    }
    else
    {
        //
    }
}

/**
 * @brief Returns the restart reason as a human-readable string.
 *
 * @return String representation of the restart reason.
 */
inline static String getRestartReason()
{
    switch (currentRestartReason)
    {
    case RestartReason::RequestedViaApi:
        return "RequestedViaApi";
    case RestartReason::RequestedViaWebCfgServer:
        return "RequestedViaWebServer";
    case RestartReason::ReconfigureWebCfgServer:
        return "ReconfigureWebServer";
    case RestartReason::BLEError:
        return "BLEError";
    case RestartReason::BLEBeaconWatchdog:
        return "BLEBeaconWatchdog";
    case RestartReason::RestartOnDisconnectWatchdog:
        return "RestartOnDisconnectWatchdog";
    case RestartReason::RestartIntervalWatchdog:
        return "RestartIntervalWatchdog";
    case RestartReason::NetworkTimeoutWatchdog:
        return "NetworkTimeoutWatchdog";
    case RestartReason::WifiInitFailed:
        return "WifiInitFailed";
    case RestartReason::ReconfigureWifi:
        return "ReconfigureWifi";
    case RestartReason::ReconfigureETH:
        return "ReconfigureETH";
    case RestartReason::NetworkDeviceCriticalFailure:
        return "NetworkDeviceCriticalFailure";
    case RestartReason::NetworkDeviceCriticalFailureNoWifiFallback:
        return "NetworkDeviceCriticalFailureNoWifiFallback";
    case RestartReason::ConfigurationUpdated:
        return "ConfigurationUpdated";
    case RestartReason::RestartTimer:
        return "RestartTimer";
    case RestartReason::DeviceUnpaired:
        return "DeviceUnpaired";
    case RestartReason::NukiBridgeReset:
        return "NukiBridgeFactoryReset";
    case RestartReason::FactoryReset:
        return "NukiBridgeFactoryReset";
    case RestartReason::DisableNetworkIfNotConnected:
        return "NetworkDisabledOnNotConnected";
    case RestartReason::SafeShutdownRequestViaApi:
        return "SafeShutdownRequestViaApi";
    case RestartReason::SafeShutdownRequestViaWebCfgServer:
        return "SafeShutdownRequestViaWebCfgServer";
    case RestartReason::NotApplicable:
        return "NotApplicable";
    default:
        return "Unknown: " + restartReason;
    }
}

/**
 * @brief Returns the ESP32 reset reason from hardware as a human-readable string.
 *
 * @return String description of the hardware reset reason.
 */
inline static String getEspRestartReason()
{
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason)
    {
    case esp_reset_reason_t::ESP_RST_UNKNOWN:
        return "ESP_RST_UNKNOWN: Reset reason can not be determined.";
    case esp_reset_reason_t::ESP_RST_POWERON:
        return "ESP_RST_POWERON: Reset due to power-on event.";
    case esp_reset_reason_t::ESP_RST_EXT:
        return "ESP_RST_EXT: Reset by external pin";
    case esp_reset_reason_t::ESP_RST_SW:
        return "ESP_RST_SW: Software reset via esp_restart.";
    case esp_reset_reason_t::ESP_RST_PANIC:
        return "ESP_RST_PANIC: Software reset due to exception/panic.";
    case esp_reset_reason_t::ESP_RST_INT_WDT:
        return "ESP_RST_INT_WDT: Reset (software or hardware) due to interrupt watchdog";
    case esp_reset_reason_t::ESP_RST_TASK_WDT:
        return "ESP_RST_TASK_WDT: Reset due to task watchdog.";
    case esp_reset_reason_t::ESP_RST_WDT:
        return "ESP_RST_WDT: Reset due to other watchdogs.";
    case esp_reset_reason_t::ESP_RST_DEEPSLEEP:
        return "ESP_RST_DEEPSLEEP: Reset after exiting deep sleep mode.";
    case esp_reset_reason_t::ESP_RST_BROWNOUT:
        return "ESP_RST_BROWNOUT: Brownout reset (software or hardware)";
    case esp_reset_reason_t::ESP_RST_SDIO:
        return "ESP_RST_SDIO: Reset over SDIO.";
    default:
        return "Unknown: " + (int)reason;
    }
}