#include "fan_bsp.h"
#include "driver/ledc.h"
#include "user_config.h"

void fan_init()
{
    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = ((uint64_t)0X01 << FAN_PWM_PIN);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    ledc_timer_config_t timer_conf = {}; 
    timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;   // 低速模式
    timer_conf.duty_resolution = LEDC_TIMER_10_BIT; // 10位分辨率
    timer_conf.timer_num = LEDC_TIMER_0;           // 定时器0
    timer_conf.freq_hz = 10 * 1000;                // 10kHz
    timer_conf.clk_cfg = LEDC_AUTO_CLK;            // 自动时钟

ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_timer_config(&timer_conf));
    ledc_channel_config_t ledc_conf = {}; 
    ledc_conf.gpio_num = FAN_PWM_PIN;
    ledc_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_conf.channel = LEDC_CHANNEL_5;
    ledc_conf.intr_type = LEDC_INTR_DISABLE;
    ledc_conf.timer_sel = LEDC_TIMER_0;
    ledc_conf.duty = 0;
    ledc_conf.hpoint = 0;
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_channel_config(&ledc_conf));
}

void fan_set_speed(fan_speed_t speed)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, speed));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5));
}
