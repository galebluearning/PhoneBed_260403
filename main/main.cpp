#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "esp_lcd_axs15231b.h"
#include "user_config.h"
#include "i2c_bsp.h"
#include "lcd_bl_pwm_bsp.h"
#include "i2c_equipment.h"
#include "esp_wifi_bsp.h"
#include "page.h"
#include "button_bsp.h"
#include "esp_io_expander.h"
#include "adc_bsp.h"
#include "bedstead.h"
#include "motor_bsp.h"
#include "fan_bsp.h"
#include "pillow.h"

static const char *TAG = "PhoneBed_main";

TaskHandle_t audio_task_handle = NULL;          // 音频任务句柄
TaskHandle_t onboard_button_task_handle = NULL; // 板载按钮任务句柄

static SemaphoreHandle_t lvgl_mux = NULL;             // LVGL 互斥体
static SemaphoreHandle_t flush_done_semaphore = NULL; // 刷新完成信号量
uint8_t *lvgl_dest = NULL;                            // LVGL旋转缓冲区

static uint16_t *trans_buf_1; // DMA传输缓冲区

#define LCD_BIT_PER_PIXEL 16                                                // 每像素位数
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))  // 每像素字节数
#define BUFF_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * BYTES_PER_PIXEL) // 显示缓冲区大小

#define LVGL_TICK_PERIOD_MS 2            // LVGL时钟周期，单位毫秒
#define LVGL_TASK_MAX_DELAY_MS 500       // LVGL任务最大延迟，单位毫秒
#define LVGL_TASK_MIN_DELAY_MS 10        // LVGL任务最小延迟，单位毫秒
#define LVGL_TASK_STACK_SIZE (10 * 1024) // LVGL任务栈大小
#define LVGL_TASK_PRIORITY 2             // LVGL任务优先级

// AXS15231B 初始化命令序列
static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] =
    {
        {0x11, (uint8_t[]){0x00}, 0, 100},
        {0x29, (uint8_t[]){0x00}, 0, 100},
};

// LVGL 刷新完成通知回调
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_awoken = pdFALSE;                          // 高优先级任务唤醒标志
    xSemaphoreGiveFromISR(flush_done_semaphore, &high_task_awoken); // 从ISR中释放信号量
    return false;
}

// LVGL 刷新回调
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp); // 获取面板句柄
    lv_draw_sw_rgb565_swap(color_p, lv_area_get_width(area) * lv_area_get_height(area));          // RGB565字节交换
#if (Rotated == USER_DISP_ROT_90)
    lv_display_rotation_t rotation = lv_display_get_rotation(disp); // 获取显示旋转角度
    lv_area_t rotated_area;                                         // 声明旋转区域
    if (rotation != LV_DISPLAY_ROTATION_0)
    {
        lv_color_format_t cf = lv_display_get_color_format(disp); // 获取显示颜色格式
        /*计算旋转区域的位置*/
        rotated_area = *area;
        lv_display_rotate_area(disp, &rotated_area); // 旋转区域
        /*根据区域宽度计算源步长（一行中的字节数）*/
        uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
        /*计算目标（旋转）区域的步幅*/
        uint32_t dest_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area), cf);
        /*有一个缓冲区来存储旋转区域并执行旋转*/

        int32_t src_w = lv_area_get_width(area);                                                    // 获取源宽度
        int32_t src_h = lv_area_get_height(area);                                                   // 获取源高度
        lv_draw_sw_rotate(color_p, lvgl_dest, src_w, src_h, src_stride, dest_stride, rotation, cf); // 执行旋转
        /*从现在开始使用旋转区域和旋转缓冲区*/
        area = &rotated_area; // 使用旋转区域
    }

    const int flush_coun = (LVGL_SPIRAM_BUFF_LEN / LVGL_DMA_BUFF_LEN); // 刷新次数
    const int offgap = (EXAMPLE_LCD_V_RES / flush_coun);               // 每次刷新高度间隔
    const int dmalen = (LVGL_DMA_BUFF_LEN / 2);                        // 每次DMA传输长度（像素数）
    int offsetx1 = 0;                                                  // 起始X坐标
    int offsety1 = 0;                                                  // 起始Y坐标
    int offsetx2 = EXAMPLE_LCD_H_RES;                                  // 结束X坐标
    int offsety2 = offgap;                                             // 结束Y坐标

    uint16_t *map = (uint16_t *)lvgl_dest; // 使用旋转后的缓冲区
    xSemaphoreGive(flush_done_semaphore);  // 释放信号量，开始传输
    for (int i = 0; i < flush_coun; i++)
    {
        xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);                                          // 等待传输完成
        memcpy(trans_buf_1, map, LVGL_DMA_BUFF_LEN);                                                  // 复制数据到传输缓冲区
        esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2, offsety2, trans_buf_1); // 绘制位图
        offsety1 += offgap;                                                                           // 更新起始Y坐标
        offsety2 += offgap;                                                                           // 更新结束Y坐标
        map += dmalen;                                                                                // 移动到下一个数据块
    }

    xSemaphoreTake(flush_done_semaphore, portMAX_DELAY); // 等待最后一次传输完成
    lv_disp_flush_ready(disp);                           // 通知LVGL刷新完成
#else
    const int flush_coun = (LVGL_SPIRAM_BUFF_LEN / LVGL_DMA_BUFF_LEN);
    const int offgap = (EXAMPLE_LCD_V_RES / flush_coun);
    const int dmalen = (LVGL_DMA_BUFF_LEN / 2);
    int offsetx1 = 0;
    int offsety1 = 0;
    int offsetx2 = EXAMPLE_LCD_H_RES;
    int offsety2 = offgap;

    uint16_t *map = (uint16_t *)color_p;
    xSemaphoreGive(flush_done_semaphore);
    for (int i = 0; i < flush_coun; i++)
    {
        xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
        memcpy(trans_buf_1, map, LVGL_DMA_BUFF_LEN);
        esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2, offsety2, trans_buf_1);
        offsety1 += offgap;
        offsety2 += offgap;
        map += dmalen;
    }
    xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
    lv_disp_flush_ready(disp);
#endif
}

// 触摸屏读取回调
static void TouchInputReadCallback(lv_indev_t *indev, lv_indev_data_t *indevData)
{
    uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x0e, 0x0, 0x0, 0x0};      // 读取触摸点命令
    uint8_t buff[32] = {0};                                                                            // 接收缓冲区
    esp_err_t ret = i2c_master_write_read_dev(disp_touch_dev_handle, read_touchpad_cmd, 11, buff, 32); // 通过I2C读取触摸数据
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Touch I2C read failed: %s", esp_err_to_name(ret));
        indevData->state = LV_INDEV_STATE_RELEASED; // I2C 错误时返回未按下状态
        return;
    }
    uint16_t pointX;                                                // 触摸点X坐标
    uint16_t pointY;                                                // 触摸点Y坐标
    pointX = (((uint16_t)buff[2] & 0x0f) << 8) | (uint16_t)buff[3]; // 计算X坐标
    pointY = (((uint16_t)buff[4] & 0x0f) << 8) | (uint16_t)buff[5]; // 计算Y坐标
    // ESP_LOGI(TAG, "触摸: %d,%d", pointX, pointY);
    if (buff[1] > 0 && buff[1] < 5)
    {
        indevData->state = LV_INDEV_STATE_PRESSED; // 按下状态
        if (pointX > EXAMPLE_LCD_V_RES)
            pointX = EXAMPLE_LCD_V_RES;
        if (pointY > EXAMPLE_LCD_H_RES)
            pointY = EXAMPLE_LCD_H_RES;
        indevData->point.x = pointY;
        indevData->point.y = (EXAMPLE_LCD_V_RES - pointX);
    }
    else
    {
        indevData->state = LV_INDEV_STATE_RELEASED; // 未按下
    }
}

// 增加LVGL滴答计数
static void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS); // 增加LVGL滴答计数，心跳
}

// 锁定LVGL互斥体，timeout_ms：-1表示永远等待
static bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms); // 计算超时滴答数
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;                                        // 尝试获取互斥体，返回是否成功
}

// 释放LVGL互斥体
static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux); // 释放互斥体
}

// LVGL 任务
static void example_lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS; // 初始任务延迟时间

    for (;;)
    {
        // 由于 LVGL API 不是线程安全的，因此锁定互斥体
        if (example_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler(); // 处理 LVGL 定时器并获取下一个延迟时间
            // 释放互斥体
            example_lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) // 限制最大延迟时间
        {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) // 限制最小延迟时间
        {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

// 手机放上去充上电后，会进这个中断服务程序
static void IRAM_ATTR charge_isr_handler(void *arg)
{
    quilt_close();                   // 先把被子盖上
    fan_set_speed(FAN_SPEED_GRADE3); // 拉高风扇转速，防止温度过高
}

extern "C" void app_main(void)
{
    cmd_init();                                      // 初始化命令行
    lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);           // 初始化背光PWM，默认最大亮度
    flush_done_semaphore = xSemaphoreCreateBinary(); // 刷新完成信号量
    assert(flush_done_semaphore);                    // 确认创建成功
    // tca9554 也是在 i2c_master_init 中初始化的
    i2c_master_init(); // 初始化I2C总线

    // =============================== 屏幕相关初始化 ===============================
    gpio_config_t gpio_conf = {};                                         // LCD复位引脚
    gpio_conf.intr_type = GPIO_INTR_DISABLE;                              // 中断禁用
    gpio_conf.mode = GPIO_MODE_OUTPUT;                                    // 输出模式
    gpio_conf.pin_bit_mask = ((uint64_t)0x01 << EXAMPLE_PIN_NUM_LCD_RST); // 配置引脚
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;                       // 下拉禁用
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;                            // 上拉启用
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));               // 配置GPIO

    spi_bus_config_t buscfg = {};                    // SPI总线配置
    buscfg.sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK;   // SPI时钟引脚
    buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0; // SPI数据0引脚
    buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1; // SPI数据1引脚
    buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2; // SPI数据2引脚
    buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3; // SPI数据3引脚
    buscfg.max_transfer_sz = LVGL_DMA_BUFF_LEN;      // 最大传输大小
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t panel_io = NULL; // LCD面板IO句柄
    esp_lcd_panel_handle_t panel = NULL;       // LCD面板句柄

    esp_lcd_panel_io_spi_config_t io_config = {};            // SPI面板IO配置
    io_config.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS;          // 片选引脚
    io_config.dc_gpio_num = -1;                              // 数据/命令引脚（未使用，使用4线SPI模式）
    io_config.spi_mode = 3;                                  // SPI模式3
    io_config.pclk_hz = 40 * 1000 * 1000;                    // SPI时钟频率40MHz
    io_config.trans_queue_depth = 10;                        // 传输队列深度
    io_config.on_color_trans_done = notify_lvgl_flush_ready; // 颜色传输完成回调
    // io_config.user_ctx = &disp_drv,
    io_config.lcd_cmd_bits = 32;                                                // 命令位数
    io_config.lcd_param_bits = 8;                                               // 参数位数
    io_config.flags.quad_mode = true;                                           // 启用4线SPI模式
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &panel_io)); // 创建新的SPI面板IO

    axs15231b_vendor_config_t vendor_config = {};                                    // AXS15231B配置
    vendor_config.flags.use_qspi_interface = 1;                                      // 使用QSPI接口
    vendor_config.init_cmds = lcd_init_cmds;                                         // 初始化命令
    vendor_config.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]); // 初始化命令大小

    esp_lcd_panel_dev_config_t panel_config = {};           // LCD面板配置
    panel_config.reset_gpio_num = -1;                       // 复位引脚（未使用，使用外部GPIO复位）
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB; // RGB元素顺序
    panel_config.bits_per_pixel = LCD_BIT_PER_PIXEL;        // 每像素位数
    panel_config.vendor_config = &vendor_config;            // 配置

    ESP_ERROR_CHECK(esp_lcd_new_panel_axs15231b(panel_io, &panel_config, &panel)); // 创建新的AXS15231B面板驱动

    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST, 1)); // 复位引脚设置高
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST, 0)); // 复位引脚设置低
    vTaskDelay(pdMS_TO_TICKS(250));
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST, 1)); // 复位引脚设置高
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel)); // 初始化面板

    // =============================== RTC相关初始化 ===============================
    i2c_rtc_setup();

    // =============================== LVGL相关初始化 ===============================
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();                                                                    // 初始化LVGL库
    lv_display_t *disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES); // 水平和垂直分辨率（像素）的基本初始化
    lv_display_set_flush_cb(disp, lvgl_flush_cb);                                 // 设置刷新回调函数以绘制到显示器

    uint8_t *buffer_1 = NULL;                                                                 // 分配显示缓冲区
    uint8_t *buffer_2 = NULL;                                                                 // 分配显示缓冲区
    buffer_1 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);                     // 分配SPIRAM内存
    assert(buffer_1);                                                                         // 确认分配成功
    buffer_2 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);                     // 分配SPIRAM内存
    assert(buffer_2);                                                                         // 确认分配成功
    trans_buf_1 = (uint16_t *)heap_caps_malloc(LVGL_DMA_BUFF_LEN, MALLOC_CAP_DMA);            // 分配DMA内存
    assert(trans_buf_1);                                                                      // 确认分配成功
    lv_display_set_buffers(disp, buffer_1, buffer_2, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL); // 设置显示缓冲区
    lv_display_set_user_data(disp, panel);                                                    // 将面板句柄设置为用户数据
#if (Rotated == USER_DISP_ROT_90)                                                             // 如果旋转90度
    lvgl_dest = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);                    // 旋转buf
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
#endif
    lv_indev_t *touch_indev = NULL;                            // 触摸输入设备
    touch_indev = lv_indev_create();                           // 创建触摸输入设备
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);     // 设置输入设备类型为指针设备
    lv_indev_set_read_cb(touch_indev, TouchInputReadCallback); // 设置读取回调函数

    esp_timer_create_args_t lvgl_tick_timer_args = {};                                      // 创建LVGL滴答计时器
    lvgl_tick_timer_args.callback = &example_increase_lvgl_tick;                            // 滴答计时器回调函数
    lvgl_tick_timer_args.name = "lvgl_tick";                                                // 滴答计时器名称
    esp_timer_handle_t lvgl_tick_timer = NULL;                                              // 滴答计时器句柄
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));             // 创建滴答计时器
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000)); // 启动滴答计时器

    lvgl_mux = xSemaphoreCreateMutex();                                                                               // 互斥信号量
    assert(lvgl_mux);                                                                                                 // 确认创建成功
    xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL, 0); // 创建LVGL任务

    if (example_lvgl_lock(-1))
    {
        page_create(); // 页面创建
        // 释放互斥体
        example_lvgl_unlock();
    }

    // =============================== AUDIO相关初始化 ===============================
    audio_system_init(); // 先初始化音频系统（codec + record + playback）

    // =============================== WIFI相关初始化 ===============================
    espwifi_Init(); // 初始化WiFi

    // 尝试多次获取网络时间，确保SNTP同步完成
    sntp_time_t stime = {};
    for (int retry = 0; retry < 3 && stime.year == 0; retry++)
    {
        stime = sntp_get_now_time(); // 获取网络时间
        if (stime.year == 0)
        {
            ESP_LOGW(TAG, "SNTP sync not ready, retry %d/3...", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(2000)); // 等待2秒后重试
        }
    }

    if (stime.year != 0)
    {
        // 成功获取网络时间
        ESP_LOGI(TAG, "Setting RTC time: %04d-%02d-%02d %02d:%02d:%02d",
                 stime.year, stime.month, stime.day, stime.hour, stime.minute, stime.second);
        i2c_rtc_setTime(stime.year, stime.month, stime.day, stime.hour, stime.minute, stime.second); // 设置RTC时间
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get network time, RTC not updated");
    }

    // =============================== 按键相关初始化 ===============================
    xTaskCreatePinnedToCore(onboard_button_task, "onboard_button_task", 8 * 1024, NULL, 2, &onboard_button_task_handle, 1); // 按钮事件

    // =============================== 风扇相关初始化 ===============================
    fan_init();                   // 初始化风扇控制
    fan_set_speed(FAN_SPEED_LOW); // 设置风扇速度为低速

    // ================================== 电机相关初始化 ==================================
    motor_bsp_init();    // 初始化电机控制
    quilt_limit_reset(); // 重置被子状态

    // ================================== 充电检测 ==================================
    gpio_config_t io_conf = {};                                         // 充电检测引脚配置
    io_conf.intr_type = GPIO_INTR_NEGEDGE;                              // 下降沿触发中断，手机放上去充电时会触发
    io_conf.pin_bit_mask = (1ULL << CHARGE_PIN);                        // 配置充电检测引脚
    io_conf.mode = GPIO_MODE_INPUT;                                     // 输入模式
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;                            // 使能上拉模式
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;                       // 禁用下拉模式
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&io_conf));               // 配置GPIO
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);                       // 注册中断服务
    gpio_isr_handler_add(CHARGE_PIN, charge_isr_handler, (void *)NULL); // 设置GPIO的中断服务函数
    gpio_intr_enable(CHARGE_PIN);                                       // 启用GPIO中断

    // =================================== 其它 ===================================
    // vTaskDelay(pdMS_TO_TICKS(1000 * 10)); // 主任务延时10秒
}
