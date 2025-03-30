#pragma once
#include <cstdint>
#include "esp_timer.h"

inline int64_t espMillis()
{
    return esp_timer_get_time() / 1000;
}