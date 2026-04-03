/**
 * @file adc_bsp.h
 * @brief ADC 驱动板级支持包 (BSP) 头文件
 * @note 本文件代码来源于微雪电子 (Waveshare) 官方示例代码，
 *       适用于 ESP32-S3-Touch-LCD-3.49 开发板。
 * @see https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49
 */

#ifndef ADC_BSP_H
#define ADC_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

void adc_bsp_init(void);
void adc_example(void* parmeter);
void adc_get_value(float *value,int *data);

#ifdef __cplusplus
}
#endif

#endif