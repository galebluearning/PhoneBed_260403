#include <stdio.h>
#include "esp_wifi_bsp.h"
#include "esp_event.h" // event
#include "nvs_flash.h" // Nvs storage
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_err.h"
#include "esp_log.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_attr.h"
#include "lwip/ip_addr.h"
#include "user_config.h"

static const char *TAG = "esp_wifi_bsp";
#define MAXIMUM_RETRY_NUM  5
static int8_t wifi_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
// TaskHandle_t pxWIFIreadTask;
//
// QueueHandle_t WIFI_QueueHandle;

void espwifi_Init(void)
{
    nvs_flash_init();                                    // 初始化默认 NVS 存储
    esp_netif_init();                                    // 初始化 TCP/IP 堆栈
    esp_event_loop_create_default();                     // 创建默认事件循环
    esp_netif_create_default_wifi_sta();                 // 将 TCP/IP 堆栈附加到默认事件循环
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 默认配置
    esp_wifi_init(&cfg);                                 // 初始化Wi-Fi
    esp_event_handler_instance_t Instance_WIFI_IP;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &Instance_WIFI_IP);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &Instance_WIFI_IP);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);               //将模式设置为 STA
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config); //配置Wi-Fi
    esp_wifi_start();                               //启动无线网络
    //WIFI_QueueHandle = xQueueCreate(30, sizeof(wifi_scan_config_t)); //创建一个包含30个项目的队列，每个项目20字节
    //printf("wifi_scan_config_t: %d\n", sizeof(wifi_scan_config_t));
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect(); // 连接到 Wi-Fi
        // esp_wifi_scan_u();
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip[25];
        uint32_t pxip = event->ip_info.ip.addr;
        sprintf(ip, "%d.%d.%d.%d", (uint8_t)(pxip), (uint8_t)(pxip >> 8), (uint8_t)(pxip >> 16), (uint8_t)(pxip >> 24));
        printf("IP: %s\n", ip);
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_retry_num++;
        if (wifi_retry_num > MAXIMUM_RETRY_NUM)
        {
            printf("Failed to connect to WiFi after %d attempts.\n", MAXIMUM_RETRY_NUM);
            //此处可以添加额外的处理逻辑，例如切换到备用连接方法
        } else {
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            printf("WiFi disconnected, reason: %d\n", event->reason);
            printf("Retrying to connect... (%d)\n", wifi_retry_num);
            esp_wifi_connect(); //自动重新连接
        }
    }
}

void espwifi_close(void)
{
    esp_wifi_disconnect(); // 断开 Wi-Fi 连接
    esp_wifi_stop();       // 停止 Wi-Fi
    esp_wifi_deinit();     // 反初始化 Wi-Fi
    ESP_LOGI(TAG, "WiFi disconnected and deinitialized.");
}

// ====================== SNTP获取网络时间 ======================

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

static void obtain_time(void);

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event, sec=%lld", (long long)tv->tv_sec);
}

sntp_time_t sntp_get_now_time(void)
{
    sntp_time_t result = {0};
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // 时间定了吗？如果不是，tm_year 将为 (1970 -1900)。
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Getting time over NTP.");
        obtain_time();
        // 用当前时间更新“now”变量
        time(&now);
        localtime_r(&now, &timeinfo);
        // 如果同步后时间仍然无效，返回空结构
        if (timeinfo.tm_year < (2016 - 1900)) {
            ESP_LOGW(TAG, "SNTP sync failed or timeout");
            return result;
        }
    }
    
    // 将时区设置为中国标准时间
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    
    //填充结构体
    result.year = timeinfo.tm_year + 1900;
    result.month = timeinfo.tm_mon + 1;
    result.day = timeinfo.tm_mday;
    result.hour = timeinfo.tm_hour;
    result.minute = timeinfo.tm_min;
    result.second = timeinfo.tm_sec;
    result.weekday = timeinfo.tm_wday;
    ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d, Weekday: %d",
             result.year, result.month, result.day,
             result.hour, result.minute, result.second,
             result.weekday);
    return result;
}

static void print_servers(void)
{
    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
        if (esp_sntp_getservername(i)){
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
        } else {
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff);
        }
    }
}

static void obtain_time(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_notification_cb;
    esp_netif_sntp_init(&config);

    print_servers();

    // 等待时间设定
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

    esp_netif_sntp_deinit();
}
