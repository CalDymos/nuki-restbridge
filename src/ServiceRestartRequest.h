#pragma once

#include <cstdint>

/**
 * @brief Enum representing requests for network service restarts.
 *
 * This enum is used to indicate whether a network service restart is requested,
 * and if so, whether it should include a reconnection attempt.
 */
enum class ServiceRestartRequest : uint8_t
{
    None = 0,
    Restart = 1,
    RestartWithReconnect = 2
};