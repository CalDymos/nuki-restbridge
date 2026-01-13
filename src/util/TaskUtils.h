#pragma once
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Delays the current task for the given time in milliseconds.
 * If the task is registered with the task watchdog, it is reset before sleeping.
 */
static inline void TaskWdtResetAndDelay(uint32_t delayMs)
{
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
        esp_task_wdt_reset();
    }

    vTaskDelay(pdMS_TO_TICKS(delayMs));
}

/**
 * @brief Resets the task watchdog for the current task, if registered.
 */
static inline void TaskWdtReset()
{
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
        esp_task_wdt_reset();
    }
}

/**
 * @brief Delays the current task for the given time in milliseconds without resetting the watchdog.
 */
static inline void TaskWdtDelay(uint32_t delayMs)
{
        vTaskDelay(pdMS_TO_TICKS(delayMs));
}