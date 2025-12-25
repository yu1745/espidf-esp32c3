#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
/**
 * @brief 延时1tick
 */
void delay1()
{
    vTaskDelay(1);
}