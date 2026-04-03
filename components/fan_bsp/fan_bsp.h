#ifndef __FAN_BSP_H
#define __FAN_BSP_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"

typedef enum {
    FAN_SPEED_LOW = 1024,
    FAN_SPEED_GRADE1 = (1024/4)*3,
    FAN_SPEED_GRADE2 = (1024/4)*2,
    FAN_SPEED_GRADE3 = (1024/4)*1,
    FAN_SPEED_HIGH = 0,
} fan_speed_t;

void fan_init();
void fan_set_speed(fan_speed_t speed);


#endif
