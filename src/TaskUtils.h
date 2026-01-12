#pragma once
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Delays the current task for the given time in milliseconds.
 * If the task is registered with the task watchdog, it is reset before sleeping.
 */
static inline void TaskDelayMsWdt(uint32_t delayMs)
{
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
        esp_task_wdt_reset();
    }

    vTaskDelay(pdMS_TO_TICKS(delayMs));
}
