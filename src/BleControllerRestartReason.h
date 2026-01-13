#pragma once

#include <cstdint>

enum class BleControllerRestartReason : uint8_t
{
    None = 0,
    DisconnectError = 1,
    BeaconWatchdog = 2
};