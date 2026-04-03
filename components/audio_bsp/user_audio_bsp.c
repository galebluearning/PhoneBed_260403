/**
 * @file user_audio_bsp.c
 * @brief 音频驱动板级支持包 (BSP) 源文件
 * @note 本文件代码来源于微雪电子 (Waveshare) 官方示例代码，
 *       适用于 ESP32-S3-Touch-LCD-3.49 开发板。
 * @see https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "user_audio_bsp.h"
#include "esp_log.h"
#include "esp_err.h"

#include "esp_gmf_init.h"

#include "codec_board.h"
#include "codec_init.h"

#define SAMPLE_RATE     24000           // 采样率：24000Hz
#define BIT_DEPTH       16              // 位深：16位

static const char *TAG = "user_audio_bsp";

esp_codec_dev_handle_t playback = NULL;
esp_codec_dev_handle_t record = NULL;

extern const uint8_t canon_pcm_start[] asm("_binary_canon_pcm_start");
extern const uint8_t canon_pcm_end[]   asm("_binary_canon_pcm_end");
extern const uint8_t alarm1_pcm_start[] asm("_binary_alarm1_pcm_start");
extern const uint8_t alarm1_pcm_end[]   asm("_binary_alarm1_pcm_end");

static TaskHandle_t s_audio_task_handle = NULL;
static volatile bool s_audio_running = false;
static volatile audio_select_t s_current_audio = AUDIO_SELECT_CANON;

static bool audio_get_pcm(audio_select_t audio, const uint8_t **data_ptr, uint32_t *len)
{
    if (data_ptr == NULL || len == NULL) {
        return false;
    }

    switch (audio) {
        case AUDIO_SELECT_CANON:
            *data_ptr = canon_pcm_start;
            *len = (uint32_t)(canon_pcm_end - canon_pcm_start);
            return true;
        case AUDIO_SELECT_ALARM1:
            *data_ptr = alarm1_pcm_start;
            *len = (uint32_t)(alarm1_pcm_end - alarm1_pcm_start);
            return true;
        default:
            return false;
    }
}

static void audio_play_task(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (s_audio_running) {
            const uint8_t *data_ptr = NULL;
            uint32_t bytes_size = 0;
            audio_select_t playing_audio = s_current_audio;

            if (!audio_get_pcm(playing_audio, &data_ptr, &bytes_size)) {
                ESP_LOGE(TAG, "invalid audio enum: %d", (int)playing_audio);
                s_audio_running = false;
                break;
            }

            uint32_t bytes_write = 0;
            while (s_audio_running && playing_audio == s_current_audio && bytes_write < bytes_size) {
                uint32_t chunk = (bytes_size - bytes_write) >= 256 ? 256 : (bytes_size - bytes_write);
                int err = esp_codec_dev_write(playback, (void *)(data_ptr + bytes_write), chunk);
                if (err != ESP_CODEC_DEV_OK) {
                    ESP_LOGW(TAG, "audio write failed: %d", err);
                    s_audio_running = false;
                    break;
                }
                bytes_write += chunk;
            }

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void user_audio_bsp_init(void)
{
    set_codec_board_type("S3_LCD_3_49");
    codec_init_cfg_t codec_cfg = 
    {
        .in_mode = CODEC_I2S_MODE_STD,
        .out_mode = CODEC_I2S_MODE_STD,
        .in_use_tdm = false,
        .reuse_dev = false,
    };
    ESP_ERROR_CHECK(init_codec(&codec_cfg));
    playback = get_playback_handle();
    record = get_record_handle();
}

uint8_t *i2s_get_handle(uint32_t *len)
{
    *len = (uint32_t)(canon_pcm_end - canon_pcm_start);
    uint8_t *data_ptr = (uint8_t *)canon_pcm_start;
    return data_ptr;
}

void i2s_echo(void *arg)
{
    esp_codec_dev_set_out_vol(playback, 70.0); //设置70声音大小
    esp_codec_dev_set_in_gain(record, 20.0);   //设置录音时的增益
    uint8_t *data_ptr = (uint8_t *)heap_caps_malloc(1024 * sizeof(uint8_t), MALLOC_CAP_DEFAULT);
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE,
        .channel = 2,
        .bits_per_sample = BIT_DEPTH,
    };
    esp_codec_dev_open(playback, &fs); //打开播放
    esp_codec_dev_open(record, &fs);   //打开录音
    for(;;)
    {
        if(ESP_CODEC_DEV_OK == esp_codec_dev_read(record, data_ptr, 1024))
        {
            esp_codec_dev_write(playback, data_ptr, 1024);
        }
    }
}

void audio_play_init(void)
{
	esp_codec_dev_set_out_vol(playback, 100.0); //设置100声音大小
    esp_codec_dev_set_in_gain(record, 35.0);   //设置录音时的增益
    esp_codec_dev_sample_info_t fs = {};
        fs.sample_rate = 24000;
        fs.channel = 2;
        fs.bits_per_sample = 16;
    esp_codec_dev_open(playback, &fs); //打开播放
    esp_codec_dev_open(record, &fs);   //打开录音

    if (s_audio_task_handle == NULL) {
        xTaskCreatePinnedToCore(audio_play_task, "audio_play_task", 4 * 1024, NULL, 2, &s_audio_task_handle, 1);
    }
}

int audio_playback_read(void *data_ptr,uint32_t len)
{
    int err = esp_codec_dev_read(record, data_ptr, len);
	return err;
}

int audio_playback_write(void *data_ptr,uint32_t len)
{
	int err = esp_codec_dev_write(playback, data_ptr, len);
    return err;
}


void audio_playback_set_vol(uint8_t vol)
{
    esp_codec_dev_set_out_vol(playback, vol);   //设置声音大小
}

void audio_start(audio_select_t audio)
{
    if (s_audio_task_handle == NULL) {
        ESP_LOGW(TAG, "audio task not ready, call audio_play_init first");
        return;
    }
    s_current_audio = audio;
    s_audio_running = true;
    xTaskNotifyGive(s_audio_task_handle);
}

void audio_stop(void)
{
    s_audio_running = false;
}
