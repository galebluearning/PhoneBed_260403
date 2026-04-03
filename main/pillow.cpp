#include "pillow.h"

#include <stdio.h>
#include <inttypes.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "motor_bsp.h"
#include "bedstead.h"
#include "user_audio_bsp.h"

static const char *TAG = "cmd";
#define PROMPT_STR CONFIG_IDF_TARGET

/** 'restart' command restarts the program */
static int restart(int argc, char **argv)
{
    ESP_LOGI(TAG, "Restarting");
    esp_restart();
    return 0;
}

static void register_restart(void)
{
    // 修复：零初始化
    esp_console_cmd_t cmd = {}; 
    cmd.command = "restart";
    cmd.help = "Software reset of the chip";
    cmd.func = &restart;
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** 'free' command prints available heap memory */
static int free_mem(int argc, char **argv)
{
    printf("%" PRIu32 "\n", esp_get_free_heap_size());
    return 0;
}

static void register_free(void)
{
    // 修复：零初始化
    esp_console_cmd_t cmd = {}; 
    cmd.command = "free";
    cmd.help = "Get the current size of free heap memory";
    cmd.func = &free_mem;
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/* 'heap' command prints minimum heap size */
static int heap_size(int argc, char **argv)
{
    uint32_t heap_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    printf("min heap size: %" PRIu32 "\n", heap_size);
    return 0;
}

static void register_heap(void)
{
    // 修复：零初始化
    esp_console_cmd_t heap_cmd = {}; 
    heap_cmd.command = "heap";
    heap_cmd.help = "Get minimum size of free heap memory that was available during program execution";
    heap_cmd.func = &heap_size;
    ESP_ERROR_CHECK(esp_console_cmd_register(&heap_cmd));
}

/* --------------------- bed motor console --------------------- */
static int cmd_quilt_open(int argc, char **argv)
{
    quilt_open();
    return 0;
}
static void register_quilt_open(void)
{
    // 修复：零初始化
    esp_console_cmd_t quilt_open_cmd = {}; 
    quilt_open_cmd.command = "open";
    quilt_open_cmd.help = "Open the quilt";
    quilt_open_cmd.func = &cmd_quilt_open;
    ESP_ERROR_CHECK(esp_console_cmd_register(&quilt_open_cmd));
}

static int cmd_quilt_close(int argc, char **argv)
{
    quilt_close();
    return 0;
}
static void register_quilt_close(void)
{
    // 修复：零初始化
    esp_console_cmd_t quilt_close_cmd = {}; 
    quilt_close_cmd.command = "close";
    quilt_close_cmd.help = "Close the quilt";
    quilt_close_cmd.func = &cmd_quilt_close;
    ESP_ERROR_CHECK(esp_console_cmd_register(&quilt_close_cmd));
}

/* --------------------- audio task console --------------------- */

static int cmd_alarm_start(int argc, char **argv)
{
    alarm_start();
    return 0;
}
static void register_alarm_start(void)
{
    // 修复：零初始化
    esp_console_cmd_t alarm_start_cmd = {}; 
    alarm_start_cmd.command = "alarmstart";
    alarm_start_cmd.help = "The scene when the alarm clock rings.";
    alarm_start_cmd.func = &cmd_alarm_start;
    ESP_ERROR_CHECK(esp_console_cmd_register(&alarm_start_cmd));
}

static int cmd_alarm_stop(int argc, char **argv)
{
    alarm_stop();
    return 0;
}
static void register_alarm_stop(void)
{
    // 修复：零初始化
    esp_console_cmd_t alarm_stop_cmd = {}; 
    alarm_stop_cmd.command = "alarmstop";
    alarm_stop_cmd.help = "Stop the alarm clock.";
    alarm_stop_cmd.func = &cmd_alarm_stop;
    ESP_ERROR_CHECK(esp_console_cmd_register(&alarm_stop_cmd));
}

void cmd_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 0;

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    /* Register commands */
    register_restart();
    register_free();
    register_heap();

    /* 开关被子的控制命令 */
    register_quilt_open();
    register_quilt_close();
    /* 闹钟 */
    register_alarm_start();
    register_alarm_stop();

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}