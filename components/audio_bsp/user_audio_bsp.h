/**
 * @file user_audio_bsp.h
 * @brief 音频驱动板级支持包 (BSP) 头文件
 * @note 本文件代码来源于微雪电子 (Waveshare) 官方示例代码，
 *       适用于 ESP32-S3-Touch-LCD-3.49 开发板。
 * @see https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49
 */

#ifndef USER_AUDIO_BSP_H
#define USER_AUDIO_BSP_H


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	AUDIO_SELECT_CANON = 0,
	AUDIO_SELECT_ALARM1,
} audio_select_t;

uint8_t *i2s_get_handle(uint32_t *len);
void user_audio_bsp_init(void);
void i2s_echo(void *arg);
void audio_playback_set_vol(uint8_t vol);

int audio_playback_read(void *data_ptr,uint32_t len);

int audio_playback_write(void *data_ptr,uint32_t len);
void audio_play_init(void);
void audio_start(audio_select_t audio);
void audio_stop(void);

#ifdef __cplusplus
}
#endif

#endif // !USER_AUDIO_BSP_H
