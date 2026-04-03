/**
 * @file i2c_bsp.c
 * @brief I2C 总线驱动板级支持包 (BSP) 源文件
 * @note 本文件代码来源于微雪电子 (Waveshare) 官方示例代码，
 *       适用于 ESP32-S3-Touch-LCD-3.49 开发板。
 * @see https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49
 */

#include <stdio.h>
#include "i2c_bsp.h"
#include "user_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_io_expander_tca9554.h"

i2c_master_bus_handle_t user_i2c_port0_handle = NULL;
i2c_master_bus_handle_t user_i2c_port1_handle = NULL;
i2c_master_dev_handle_t rtc_dev_handle = NULL;
i2c_master_dev_handle_t imu_dev_handle = NULL;
i2c_master_dev_handle_t disp_touch_dev_handle = NULL;

static uint32_t i2c_data_pdMS_TICKS = 0;
static uint32_t i2c_done_pdMS_TICKS = 0;

esp_io_expander_handle_t io_expander = NULL;

static void tca9554_init(void)
{
  i2c_master_bus_handle_t tca9554_i2c_bus_ = NULL;
  ESP_ERROR_CHECK(i2c_master_get_bus_handle(0, &tca9554_i2c_bus_));
  esp_io_expander_new_i2c_tca9554(tca9554_i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander);
  esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_6, IO_EXPANDER_OUTPUT);
  esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_6, 1);
  esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_7, IO_EXPANDER_OUTPUT);  // 设置引脚7为输出，使能audio
  esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_7, 1);
}

void i2c_master_init(void)
{
  i2c_data_pdMS_TICKS = pdMS_TO_TICKS(5000);
  i2c_done_pdMS_TICKS = pdMS_TO_TICKS(1000);
  /*i2c_port 0 init*/
  i2c_master_bus_config_t i2c_bus_config =
      {
          .clk_source = I2C_CLK_SRC_DEFAULT,
          .i2c_port = I2C_NUM_0,
          .scl_io_num = ESP32_SCL_NUM,
          .sda_io_num = ESP32_SDA_NUM,
          .glitch_ignore_cnt = 7,
          .flags = {
              .enable_internal_pullup = true,
          },
      };
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &user_i2c_port0_handle));
  // 稍作延迟，确保外设上电稳定
  vTaskDelay(pdMS_TO_TICKS(20));
  i2c_bus_config.scl_io_num = Touch_SCL_NUM;
  i2c_bus_config.sda_io_num = Touch_SDA_NUM;
  i2c_bus_config.i2c_port = I2C_NUM_1;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &user_i2c_port1_handle));
  vTaskDelay(pdMS_TO_TICKS(20));

  i2c_device_config_t dev_cfg =
      {
          .dev_addr_length = I2C_ADDR_BIT_LEN_7,
          .scl_speed_hz = 300000,
      };
  dev_cfg.device_address = RTC_PCF85063_ADDR;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(user_i2c_port0_handle, &dev_cfg, &rtc_dev_handle));

  dev_cfg.device_address = IMU_QMI8658_ADDR;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(user_i2c_port0_handle, &dev_cfg, &imu_dev_handle));

  // 触摸设备使用更低速率以提高兼容性
  dev_cfg.scl_speed_hz = 100000;
  dev_cfg.device_address = DISP_TOUCH_ADDR;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(user_i2c_port1_handle, &dev_cfg, &disp_touch_dev_handle));
  // 主动探测触摸设备是否应答，便于早期定位连接问题
  esp_err_t probe_ret = i2c_master_probe(user_i2c_port1_handle, DISP_TOUCH_ADDR, pdMS_TO_TICKS(100));
  if (probe_ret != ESP_OK) {
    ESP_LOGE("i2c.touch", "Probe failed: addr=0x%02X, scl=%d, sda=%d, err=%s", (unsigned)DISP_TOUCH_ADDR, Touch_SCL_NUM, Touch_SDA_NUM, esp_err_to_name(probe_ret));
  } else {
    ESP_LOGI("i2c.touch", "Probe OK: addr=0x%02X on port1", (unsigned)DISP_TOUCH_ADDR);
  }

  tca9554_init();
}

uint8_t i2c_writr_buff(i2c_master_dev_handle_t dev_handle, int reg, uint8_t *buf, uint8_t len)
{
  uint8_t ret;
  uint8_t *pbuf = NULL;
  ret = i2c_master_bus_wait_all_done(user_i2c_port1_handle, i2c_done_pdMS_TICKS);
  if (ret != ESP_OK)
    return ret;
  if (reg == -1)
  {
    ret = i2c_master_transmit(dev_handle, buf, len, i2c_data_pdMS_TICKS);
  }
  else
  {
    pbuf = (uint8_t *)malloc(len + 1);
    pbuf[0] = reg;
    for (uint8_t i = 0; i < len; i++)
    {
      pbuf[i + 1] = buf[i];
    }
    ret = i2c_master_transmit(dev_handle, pbuf, len + 1, i2c_data_pdMS_TICKS);
    free(pbuf);
    pbuf = NULL;
  }
  return ret;
}
uint8_t i2c_master_write_read_dev(i2c_master_dev_handle_t dev_handle, uint8_t *writeBuf, uint8_t writeLen, uint8_t *readBuf, uint8_t readLen)
{
  uint8_t ret;
  ret = i2c_master_bus_wait_all_done(user_i2c_port1_handle, i2c_done_pdMS_TICKS);
  if (ret != ESP_OK)
  {
    ESP_LOGE("i2c.master", "wait_all_done failed: %s", esp_err_to_name(ret));
    return ret;
  }
  ret = i2c_master_transmit_receive(dev_handle, writeBuf, writeLen, readBuf, readLen, i2c_data_pdMS_TICKS);
  if (ret != ESP_OK) {
    ESP_LOGE("i2c.master", "transmit_receive failed: %s", esp_err_to_name(ret));
    (void)i2c_master_bus_reset(user_i2c_port1_handle);
  }
  return ret;
}
uint8_t i2c_read_buff(i2c_master_dev_handle_t dev_handle, int reg, uint8_t *buf, uint8_t len)
{
  uint8_t ret;
  uint8_t addr = 0;
  ret = i2c_master_bus_wait_all_done(user_i2c_port1_handle, i2c_done_pdMS_TICKS);
  if (ret != ESP_OK)
    return ret;
  if (reg == -1)
  {
    ret = i2c_master_receive(dev_handle, buf, len, i2c_data_pdMS_TICKS);
  }
  else
  {
    addr = (uint8_t)reg;
    ret = i2c_master_transmit_receive(dev_handle, &addr, 1, buf, len, i2c_data_pdMS_TICKS);
  }
  if (ret != ESP_OK) {
    ESP_LOGE("i2c.master", "read failed: %s", esp_err_to_name(ret));
    (void)i2c_master_bus_reset(user_i2c_port1_handle);
  }
  return ret;
}
