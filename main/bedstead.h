#ifndef __BEDSTEAD_H
#define __BEDSTEAD_H

#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void audio_system_init(void);        // 音频系统初始化（codec + record + playback）
void i2s_audio_task(void *arg);      // I2S 音频任务
void onboard_button_task(void *arg); // 板载按键任务，按键主任务

void alarm_start(); // 闹钟开始
void alarm_stop();  // 闹钟停止

#endif // __BEDSTEAD_H