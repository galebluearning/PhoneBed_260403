/**
 * @file i2c_bsp.h
 * @brief I2C 总线驱动板级支持包 (BSP) 头文件
 * @note 本文件代码来源于微雪电子 (Waveshare) 官方示例代码，
 *       适用于 ESP32-S3-Touch-LCD-3.49 开发板。
 * @see https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49
 */

#ifndef I2C_BSP_H
#define I2C_BSP_H
#include "driver/i2c_master.h"

extern i2c_master_dev_handle_t rtc_dev_handle;
extern i2c_master_dev_handle_t imu_dev_handle;
extern i2c_master_dev_handle_t disp_touch_dev_handle;

extern i2c_master_bus_handle_t user_i2c_port0_handle;
extern i2c_master_bus_handle_t user_i2c_port1_handle;
#ifdef __cplusplus
extern "C" {
#endif

void i2c_master_init(void);
void touch_i2c_master_Init(void);
uint8_t i2c_writr_buff(i2c_master_dev_handle_t dev_handle,int reg,uint8_t *buf,uint8_t len);
uint8_t i2c_master_write_read_dev(i2c_master_dev_handle_t dev_handle,uint8_t *writeBuf,uint8_t writeLen,uint8_t *readBuf,uint8_t readLen);
uint8_t i2c_read_buff(i2c_master_dev_handle_t dev_handle,int reg,uint8_t *buf,uint8_t len);

#ifdef __cplusplus
}
#endif

#endif