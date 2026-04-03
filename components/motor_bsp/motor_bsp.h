#ifndef __MOTOR_BSP_H
#define __MOTOR_BSP_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

typedef enum
{
    MOTOR_MODE_STOP = 0,
    MOTOR_MODE_FORWARD,
    MOTOR_MODE_REVERSE,
    MOTOR_MODE_BRAKE
} motor_mode_t;

typedef enum
{
    MOTOR_SPEED_STOP = (1024 / 10) * 0,
    MOTOR_SPEED_10 = (1024 / 10) * 1,
    MOTOR_SPEED_20 = (1024 / 10) * 2,
    MOTOR_SPEED_30 = (1024 / 10) * 3,
    MOTOR_SPEED_40 = (1024 / 10) * 4,
    MOTOR_SPEED_50 = (1024 / 10) * 5,
    MOTOR_SPEED_60 = (1024 / 10) * 6,
    MOTOR_SPEED_70 = (1024 / 10) * 7,
    MOTOR_SPEED_80 = (1024 / 10) * 8,
    MOTOR_SPEED_90 = (1024 / 10) * 9,
    MOTOR_SPEED_FULL = (1024 / 10) * 10,
} motor_speed_t;

typedef struct
{
    ledc_channel_t pwm_channel1;
    ledc_channel_t pwm_channel2;
    motor_speed_t speed;
} motor_t;

void motor_bsp_init();
void motor_set_mode_speed(uint8_t motor_index, motor_mode_t mode, motor_speed_t speed = MOTOR_SPEED_STOP);

void quilt_open();     // 打开被子
void quilt_close();    // 关闭被子
void quilt_limit_reset(); // 重置被子限位状态

#endif // __MOTOR_BSP_H
