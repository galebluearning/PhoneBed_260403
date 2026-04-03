#ifndef ESP_WIFI_BSP_H
#define ESP_WIFI_BSP_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h" //WIFI
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // 时间结构体
    typedef struct {
        uint16_t year;   // 年
        uint8_t month;   // 月 (1-12)
        uint8_t day;     // 日 (1-31)
        uint8_t hour;    // 时 (0-23)
        uint8_t minute;  // 分 (0-59)
        uint8_t second;  // 秒 (0-59)
        uint8_t weekday; // 星期 (0-6, 0=周日)
    } sntp_time_t;

    void espwifi_Init(void);
    void espwifi_close(void);
    sntp_time_t sntp_get_now_time(void);

#ifdef __cplusplus
}
#endif

#endif