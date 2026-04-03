#include "bedstead.h"
#include "user_audio_bsp.h"
#include "button_bsp.h"
#include "esp_io_expander.h"
#include "page.h"
#include "motor_bsp.h"
#include "fan_bsp.h"
#include <math.h>
#include "esp_heap_caps.h"

static const char *TAG = "phonebed_task";

extern TaskHandle_t audio_task_handle;
static uint8_t *audio_ptr = NULL;

void bed_button_l_task(void *arg);
void bed_button_r_task(void *arg);

void audio_system_init(void)
{
    audio_ptr = (uint8_t *)heap_caps_malloc(288 * 1000 * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    user_audio_bsp_init();
    audio_play_init();
    audio_playback_set_vol(50); // 设置初始音量
    ESP_LOGI(TAG, "Audio system initialized");
}

void i2s_audio_task(void *arg)
{
    for (;;)
    {
        assert(audio_ptr);
        // ESP_LOGI(TAG, "i2s_audio_task running");
        audio_playback_set_vol(80);                      // 设置音量
        uint32_t bytes_sizt;                             // 获取音频数据大小
        size_t bytes_write = 0;                          // 已写入字节数
        uint8_t *data_ptr = i2s_get_handle(&bytes_sizt); // 获取音频数据指针和大小
        while (bytes_write < bytes_sizt)                 // 循环写入音频数据
        {
            audio_playback_write(data_ptr, 256); // 写入256字节音频数据
            data_ptr += 256;                     // 移动数据指针
            bytes_write += 256;                  // 更新已写入字节数
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 延时，避免任务过于频繁执行
    }
}

extern esp_io_expander_handle_t io_expander;
static bool is_vbatpowerflag = false;
/*
板载按键任务，按键主任务
*/
void onboard_button_task(void *arg)
{
    button_Init();
    // 创建床头按键任务
    xTaskCreatePinnedToCore(bed_button_l_task, "bed_button_l_task", 4 * 1024, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(bed_button_r_task, "bed_button_r_task", 4 * 1024, NULL, 2, NULL, 1);
    for (;;)
    {
        EventBits_t even = xEventGroupWaitBits(pwr_groups, set_bit_all, pdTRUE, pdFALSE, portMAX_DELAY);
        if (get_bit_button(even, 1)) // 长按
        {
            if (is_vbatpowerflag)
            {
                is_vbatpowerflag = false;
                esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_6, 0);
            }
        }
        else if (get_bit_button(even, 2)) // 长按保持
        {
            if (!is_vbatpowerflag)
            {
                is_vbatpowerflag = true;
            }
        }
    }
}

extern page_index_t current_page;
uint8_t alarm_switch_index = 0;
/*
左键单击：
    闹钟设置页面：当前选中的时间减少 1
    闹钟响铃页面：停止闹钟
左键双击：
    闹钟设置页面：切换闹钟修改索引（小时、分钟、无修改）
左键长按：
    默认页面：页面切换（时间页面、闹钟设置页面）
*/
void bed_button_l_task(void *arg)
{
    for (;;)
    {
        EventBits_t even = xEventGroupWaitBits(l_groups, set_bit_all, pdTRUE, pdFALSE, portMAX_DELAY);
        if (get_bit_button(even, 0)) // 左按键单击
        {
            if (current_page == PAGE_INDEX_ALARM_CLOCK)
            {
                // 当前页面是闹钟设置页面，修改闹钟时间
                page_alarm_clock_set_index_clock_sub();
            }
            else if (current_page == PAGE_INDEX_WARNING_CLOCK)
            {
                // 当前页面是闹钟响铃页面，停止闹钟
                alarm_stop();
            }
            // 打印日志，显示当前是什么页面
            ESP_LOGI(TAG, "Bed Left Button Clicked, current page: %d", current_page);
        }
        else if (get_bit_button(even, 1)) // 左按键双击
        {
            ESP_LOGI(TAG, "Bed Left Button Double Clicked");
            // 只有在闹钟设置页面才切换修改索引
            if (current_page == PAGE_INDEX_ALARM_CLOCK)
            {
                alarm_switch_index++;
                if (alarm_switch_index > 2)
                {
                    alarm_switch_index = 0;
                }
                page_alarm_clock_set_index(alarm_switch_index);
            }
        }
        else if (get_bit_button(even, 2)) // 左按键长按
        {
            ESP_LOGI(TAG, "Bed Left Button Long Pressed");
            // 页面间切换
            if (current_page == PAGE_INDEX_TIME_CLOCK)
            {
                page_switch_async(PAGE_INDEX_ALARM_CLOCK);
            }
            else if (current_page == PAGE_INDEX_ALARM_CLOCK)
            {
                page_switch_async(PAGE_INDEX_TIME_CLOCK);
            }

            // 让出 CPU，避免长时间占用导致 IDLE 任务无法喂狗
            vTaskDelay(1);
        }
    }
}

/*
右键单击：
    闹钟设置页面：当前选中的时间增加 1
    闹钟响铃页面：停止闹钟
右键双击：
    闹钟设置页面：设置闹钟开启或关闭
右键长按：
    闹钟设置页面：当前选中的时间增加加加 10
*/
void bed_button_r_task(void *arg)
{
    for (;;)
    {
        EventBits_t even = xEventGroupWaitBits(r_groups, set_bit_all, pdTRUE, pdFALSE, portMAX_DELAY);
        if (get_bit_button(even, 0)) // 右按键单击
        {
            ESP_LOGI(TAG, "Bed Right Button Clicked");
            if (current_page == PAGE_INDEX_ALARM_CLOCK)
            {
                // 当前页面是闹钟设置页面，修改闹钟时间
                page_alarm_clock_set_index_clock_add(1);
            }
            else if (current_page == PAGE_INDEX_WARNING_CLOCK)
            {
                // 当前页面是闹钟响铃页面，停止闹钟
                alarm_stop();
            }
        }
        else if (get_bit_button(even, 1)) // 右按键双击
        {
            ESP_LOGI(TAG, "Bed Right Button Double Clicked");
            // 切换闹钟开启或关闭状态
            page_alarm_clock_set_flag(!page_alarm_clock_get_flag());
        }
        else if (get_bit_button(even, 2)) // 右按键长按
        {
            ESP_LOGI(TAG, "Bed Right Button Long Pressed");
            page_alarm_clock_set_index_clock_add(10);
        }
    }
}
// 闹钟开始
void alarm_start()
{
    // 启动闹钟声音播放
    audio_start(AUDIO_SELECT_ALARM1);
    // 通知LVGL闹钟开始，显示闹钟响的页面
    page_switch_async(PAGE_INDEX_WARNING_CLOCK);
}
// 闹钟停止
void alarm_stop()
{
    // 停止闹钟声音播放
    audio_stop();
    // 通知LVGL闹钟停止，恢复正常页面显示
    page_switch_async(PAGE_INDEX_TIME_CLOCK);
    // 打开被子
    quilt_open();
    // 风扇调整到最低转速
    fan_set_speed(FAN_SPEED_LOW);
}
