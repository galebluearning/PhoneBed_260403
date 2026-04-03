#include "page.h"
#include "i2c_equipment.h"
#include "adc_bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdlib.h>
#include "bedstead.h"
#include "lvgl.h"
#include "img/img.h"

extern const lv_font_t symbol_font;

page_index_t current_page = PAGE_INDEX_TIME_CLOCK; // 当前页面索引
/* **************************** 全局 **************************** */
// #define PAGE_BG_COLOR_HEX 0x003a57 // 深蓝色，和控制板的外壳一个颜色
#define PAGE_BG_COLOR_HEX 0x5ab0eb // 浅蓝色，和时间页面的背景颜色一个颜色

/* ************************** 时间页面 ************************** */
static lv_obj_t *now_page = NULL;     // 当前时间页面对象
static lv_timer_t *time_timer = NULL; // 更新时间的定时器
static lv_obj_t *label_time = NULL;   // 显示当前时间
static lv_font_t time_mono_font;      // 时间页面字体
// 勤奋的鸽子动画 (lively_dove)
static const lv_image_dsc_t *lively_dove_imgs[LIVELY_DOVE_FRAME_COUNT] = {
    &lively_dove_0000_0001, &lively_dove_0001_0002, &lively_dove_0002_0003, &lively_dove_0003_0004, &lively_dove_0004_0005, &lively_dove_0005_0006,
    &lively_dove_0006_0007, &lively_dove_0007_0008, &lively_dove_0008_0009, &lively_dove_0009_0010, &lively_dove_0010_0011, &lively_dove_0011_0012,
    &lively_dove_0012_0013, &lively_dove_0013_0014, &lively_dove_0014_0015, &lively_dove_0015_0016, &lively_dove_0016_0017, &lively_dove_0017_0018,
    &lively_dove_0018_0019, &lively_dove_0019_0020, &lively_dove_0020_0021, &lively_dove_0021_0022, &lively_dove_0022_0023, &lively_dove_0023_0024,
    &lively_dove_0024_0025, &lively_dove_0025_0026, &lively_dove_0026_0027, &lively_dove_0027_0028, &lively_dove_0028_0029, &lively_dove_0029_0030,
    &lively_dove_0030_0031, &lively_dove_0031_0032, &lively_dove_0032_0033, &lively_dove_0033_0034, &lively_dove_0034_0035, &lively_dove_0035_0036,
    &lively_dove_0036_0037, &lively_dove_0037_0038, &lively_dove_0038_0039, &lively_dove_0039_0040, &lively_dove_0040_0041, &lively_dove_0041_0042,
    &lively_dove_0042_0043};
// 睡觉鸽子动画（sleep_dove）
static const lv_image_dsc_t *sleep_dove_imgs[SLEEP_DOVE_FRAME_COUNT] = {
    &sleep_dove_0000_0001, &sleep_dove_0001_0002, &sleep_dove_0002_0003, &sleep_dove_0003_0004, &sleep_dove_0004_0005, &sleep_dove_0005_0006,
    &sleep_dove_0006_0007, &sleep_dove_0007_0008, &sleep_dove_0008_0009, &sleep_dove_0009_0010, &sleep_dove_0010_0011, &sleep_dove_0011_0012,
    &sleep_dove_0012_0013, &sleep_dove_0013_0014, &sleep_dove_0014_0015, &sleep_dove_0015_0016, &sleep_dove_0016_0017, &sleep_dove_0017_0018,
    &sleep_dove_0018_0019, &sleep_dove_0019_0020, &sleep_dove_0020_0021, &sleep_dove_0021_0022, &sleep_dove_0022_0023, &sleep_dove_0023_0024,
    &sleep_dove_0024_0025, &sleep_dove_0025_0026, &sleep_dove_0026_0027, &sleep_dove_0027_0028, &sleep_dove_0028_0029, &sleep_dove_0029_0030,
    &sleep_dove_0030_0031, &sleep_dove_0031_0032, &sleep_dove_0032_0033, &sleep_dove_0033_0034, &sleep_dove_0034_0035, &sleep_dove_0035_0036,
    &sleep_dove_0036_0037, &sleep_dove_0037_0038, &sleep_dove_0038_0039, &sleep_dove_0039_0040, &sleep_dove_0040_0041, &sleep_dove_0041_0042,
    &sleep_dove_0042_0043};
static lv_obj_t *dove_nest_img = NULL;       // 鸟巢图片对象
static lv_obj_t *lively_dove_animimg = NULL; // lively_dove 动画图片对象
static lv_obj_t *sleep_dove_animimg = NULL;  // sleep_dove 动画图片对象

/* ************************** 闹钟页面 ************************** */
static bool alarm_clock_flag = false;   // 闹钟是否开启标志
static int alarm_clock_index = 0;       // 闹钟当前修改索引 0表示未修改 1表示修改小时 2表示修改分钟
static lv_obj_t *alarm_page = NULL;     // 闹钟页面对象
static uint8_t alarm_clock[2] = {0, 0}; // 闹钟时间结构体
static lv_obj_t *alarm_switch_default;  // 闹钟默认开关对象
static lv_obj_t *alarm_switch;          // 闹钟开关对象
static lv_obj_t *alarm_roller_hour;     // 闹钟设置小时滚轮
static lv_obj_t *alarm_roller_minute;   // 闹钟设置分钟滚轮
static lv_obj_t *img_arrow_left;        // 闹钟设置页面左侧箭头标签
static lv_obj_t *img_arrow_right;       // 闹钟设置页面右侧箭头标签

/* ************************** 响铃页面 ************************** */
static lv_obj_t *warning_page = NULL;            // 响铃页面对象
static lv_obj_t *alarm_warning_cloud = NULL;     // 响铃云朵对象
static lv_obj_t *alarm_warning_lightning = NULL; // 响铃闪电对象
static lv_timer_t *warning_timer = NULL;         // 响铃页面定时器
static bool warning_toggle = false;              // 闪电切换标志
#define WARING_TOGGLE_INTERVAL_MS 500            // 闪电切换时间间隔毫秒数

// ==========================================================================

// 设置统一风格样式
void page_style(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(PAGE_BG_COLOR_HEX), LV_PART_MAIN);
    lv_obj_set_style_text_color(parent, lv_color_hex(0xffffff), LV_PART_MAIN);
}

// 创建页面头部，联网状态等
void page_head(lv_obj_t *parent)
{
    static lv_style_t style_bg;
    static lv_style_t style_indic;
    // 背景样式
    lv_style_init(&style_bg);
    lv_style_set_border_width(&style_bg, 2);
    lv_style_set_border_color(&style_bg, lv_color_hex(0x000000));
    lv_style_set_pad_all(&style_bg, 2); /*To make the indicator smaller*/
    lv_style_set_radius(&style_bg, 6);
    lv_style_set_anim_duration(&style_bg, 1000);
    // 指示器样式
    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_radius(&style_indic, 3);
}

// 消息提示框
void page_alert_show(const char *msg)
{
    /* 在当前屏幕创建临时顶部提示框，3秒后自动删除 */
    lv_obj_t *notif = lv_obj_create(lv_scr_act());
    lv_obj_set_size(notif, 300, 40);
    lv_obj_align(notif, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_bg_color(notif, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(notif, LV_OPA_20, LV_PART_MAIN);
    lv_obj_remove_style(notif, NULL, LV_PART_SCROLLBAR); // 移除滚动条样式
    lv_obj_t *label = lv_label_create(notif);
    lv_label_set_text(label, msg);
    lv_obj_center(label);

    lv_timer_create(
        [](lv_timer_t *t)
        {
            lv_obj_del((lv_obj_t *)lv_timer_get_user_data(t));
            lv_timer_del(t);
        },
        3000,
        notif);
}

// 初始化时创建页面
void page_create()
{
    // 设置屏幕背景颜色和默认文本颜色
    page_time();
    page_alarm_clock();
    page_warning_clock();

    lv_scr_load(warning_page);

    current_page = PAGE_INDEX_TIME_CLOCK;
}

/* ************************** 时间页面 ************************** */

// 固定宽度字体获取字形描述符回调
static bool fix_w_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t letter,
                                uint32_t letter_next)
{
    bool ret = lv_font_get_glyph_dsc_fmt_txt(font, dsc, letter, letter_next);
    if (!ret)
        return false;

    // 设置固定宽度
    dsc->adv_w = 60;

    dsc->ofs_x = (dsc->adv_w - dsc->box_w) / 2;
    return true;
}

// 刷新时间显示回调
void page_time_refresh(lv_timer_t *timer)
{
    RtcDateTime_t datetime;
    datetime = i2c_rtc_get();                                                             // 获取当前时间
    char time_str[32];                                                                    // 时间字符串缓冲区
    sprintf(time_str, "%02d:%02d:%02d", datetime.hour, datetime.minute, datetime.second); // 格式化时间字符串

    if (label_time != NULL)
    {
        lv_label_set_text(label_time, time_str); // 更新标签文本
    }
    if (alarm_clock_flag && (datetime.hour == alarm_clock[0]) && (datetime.minute == alarm_clock[1]) && (datetime.second == 0)) // 如果闹钟开启且时间到了
    {
        // 触发闹钟事件
        ESP_LOGI("ALARM", "Alarm Triggered at %02d:%02d:%02d", datetime.hour, datetime.minute, datetime.second);
        alarm_start(); // 启动闹钟，开始吵，吵，吵
    }
}

// 创建时间显示页面
void page_time()
{
    now_page = lv_obj_create(NULL);
    lv_obj_set_size(now_page, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    page_head(now_page);  // 创建页面头部
    page_style(now_page); // 设置页面风格

    static lv_style_t page_time_style;
    lv_style_init(&page_time_style);
    lv_style_set_bg_image_src(&page_time_style, &time_bg); // 设置背景图片
    lv_style_set_bg_image_opa(&page_time_style, LV_OPA_COVER);
    lv_style_set_bg_image_tiled(&page_time_style, false);
    lv_obj_add_style(now_page, &page_time_style, 0);

    time_mono_font = lv_font_montserrat_48;                     // 使用内置的等宽字体
    time_mono_font.get_glyph_dsc = fix_w_get_glyph_dsc;         // 设置获取字形描述符的回调函数
    label_time = lv_label_create(now_page);                     // 创建标签对象
    lv_obj_set_style_text_font(label_time, &time_mono_font, 0); // 设置标签字体
    lv_obj_center(label_time);                                  // 居中标签

    // 中间下方的白色短线
    static lv_obj_t *line = lv_obj_create(now_page);
    lv_obj_set_size(line, 300, 2);
    lv_obj_align(line, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(line, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_radius(line, 2, LV_PART_MAIN);

    // 创建 lively_dove 动画图片对象
    lively_dove_animimg = lv_animimg_create(now_page);
    lv_obj_align(lively_dove_animimg, LV_ALIGN_BOTTOM_MID, -80, -2);
    lv_animimg_set_src(lively_dove_animimg, (const void **)lively_dove_imgs, LIVELY_DOVE_FRAME_COUNT);
    lv_animimg_set_duration(lively_dove_animimg, 7000);
    lv_animimg_set_repeat_count(lively_dove_animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(lively_dove_animimg);
    lv_obj_set_style_opa(lively_dove_animimg, LV_OPA_80, 0); // 通过设置透明度来开启或关闭显示

    // 创建 dove_nest 图片对象，和lively_dove同时消失或显示
    dove_nest_img = lv_img_create(now_page);
    lv_img_set_src(dove_nest_img, &dove_nest);
    lv_obj_align(dove_nest_img, LV_ALIGN_BOTTOM_MID, 80, -2);
    lv_obj_set_style_opa(dove_nest_img, LV_OPA_80, 0); // 通过设置透明度来开启或关闭显示

    // 创建 sleep_dove 动画图片对象
    sleep_dove_animimg = lv_animimg_create(now_page);
    lv_obj_align(sleep_dove_animimg, LV_ALIGN_BOTTOM_MID, 80, -2);
    lv_animimg_set_src(sleep_dove_animimg, (const void **)sleep_dove_imgs, SLEEP_DOVE_FRAME_COUNT);
    lv_animimg_set_duration(sleep_dove_animimg, 7000);
    lv_animimg_set_repeat_count(sleep_dove_animimg, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(sleep_dove_animimg);
    lv_obj_set_style_opa(sleep_dove_animimg, LV_OPA_0, 0); // 通过设置透明度来开启或关闭显示

    time_timer = lv_timer_create(page_time_refresh, 100, NULL);
}

void set_dove_animing(bool animing) // 设置鸟儿动画状态, true表示活泼状态，false表示睡觉状态
{
    if (animing)
    {
        lv_obj_set_style_opa(lively_dove_animimg, LV_OPA_80, 0);
        lv_obj_set_style_opa(dove_nest_img, LV_OPA_80, 0);
        lv_obj_set_style_opa(sleep_dove_animimg, LV_OPA_0, 0);
    }
    else
    {
        lv_obj_set_style_opa(lively_dove_animimg, LV_OPA_0, 0);
        lv_obj_set_style_opa(dove_nest_img, LV_OPA_0, 0);
        lv_obj_set_style_opa(sleep_dove_animimg, LV_OPA_80, 0);
    }
}

/* ************************** 闹钟页面 ************************** */

// 刷新设置闹钟页面
void page_alarm_clock_refresh(uint8_t hour, uint8_t minute)
{
    uint8_t _hour = hour >= 24 ? 0 : hour;
    uint8_t _minute = minute >= 60 ? 0 : minute;

    alarm_clock[0] = _hour;
    alarm_clock[1] = _minute;
    lv_roller_set_selected(alarm_roller_hour, alarm_clock[0], LV_ANIM_OFF);
    lv_roller_set_selected(alarm_roller_minute, alarm_clock[1], LV_ANIM_OFF);

    lv_obj_set_style_image_opa(img_arrow_left, alarm_clock_index == 1 ? LV_OPA_COVER : LV_OPA_0, 0);
    lv_obj_set_style_image_opa(img_arrow_right, alarm_clock_index == 2 ? LV_OPA_COVER : LV_OPA_0, 0);

    // 刷新闹钟开关状态
    if (alarm_clock_flag)
    {
        lv_obj_set_style_image_opa(alarm_switch_default, LV_OPA_0, 0);
        lv_obj_set_style_image_opa(alarm_switch, LV_OPA_COVER, 0);
        lv_obj_move_foreground(alarm_switch);
    }
    else
    {
        lv_obj_set_style_image_opa(alarm_switch_default, LV_OPA_COVER, 0);
        lv_obj_set_style_image_opa(alarm_switch, LV_OPA_0, 0);
        lv_obj_move_foreground(alarm_switch_default);
    }
}

// 闹钟开关事件处理
static void page_alarm_clock_switch_off_event_handler(lv_event_t *e)
{
    page_alarm_clock_set_flag(false);
}
static void page_alarm_clock_switch_on_event_handler(lv_event_t *e)
{
    page_alert_show("The alarm has been set successfully.");
    page_alarm_clock_set_flag(true);
}

// 闹钟页面滚轮事件处理 - 小时
static void roller_hour_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        alarm_clock[0] = lv_roller_get_selected(obj);
        page_alarm_clock_set_flag(false); // 只要重新设置时间，就关闭闹钟
        ESP_LOGI("ROLLER", "Selected value: %d", lv_roller_get_selected(obj));
    }
}
// 闹钟页面滚轮事件处理 - 分钟
static void roller_minute_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        alarm_clock[1] = lv_roller_get_selected(obj);
        page_alarm_clock_set_flag(false); // 只要重新设置时间，就关闭闹钟
    }
}

// 闹钟设置页面
void page_alarm_clock()
{
    alarm_page = lv_obj_create(NULL);
    lv_obj_set_size(alarm_page, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    page_head(alarm_page);  // 创建页面头部
    page_style(alarm_page); // 设置页面风格

    static lv_style_t page_alarm_style;
    lv_style_init(&page_alarm_style);
    lv_style_set_bg_image_src(&page_alarm_style, &alarm_bg); // 设置背景图片
    lv_style_set_bg_image_opa(&page_alarm_style, LV_OPA_COVER);
    lv_style_set_bg_image_tiled(&page_alarm_style, false);
    lv_obj_add_style(alarm_page, &page_alarm_style, 0);

    // 字体样式
    static lv_font_t mono_font;
    mono_font = lv_font_montserrat_48;             // 使用内置的等宽字体
    mono_font.get_glyph_dsc = fix_w_get_glyph_dsc; // 设置获取字形描述符的回调函数

    lv_obj_t *label_wall = NULL;
    label_wall = lv_label_create(alarm_page);              // 创建标签对象
    lv_label_set_text(label_wall, ":");                    // 设置标签文本
    lv_obj_set_style_text_font(label_wall, &mono_font, 0); // 设置标签字体
    lv_obj_center(label_wall);                             // 居中标签

    // 滚轮样式
    static lv_style_t style;
    lv_style_init(&style);
    // lv_style_set_bg_color(&style, lv_color_hex(PAGE_BG_COLOR_HEX));

    lv_style_set_bg_opa(&style, LV_OPA_TRANSP);
    lv_style_set_text_color(&style, lv_color_black());
    lv_style_set_border_width(&style, 0);
    lv_style_set_radius(&style, 0);

    // 滚筒选中项样式
    static lv_style_t style_selected;
    lv_style_init(&style_selected);
    lv_style_set_bg_opa(&style_selected, LV_OPA_TRANSP); // 背景透明样式
    lv_style_set_text_color(&style_selected, lv_color_white());
    // 小时选择滚筒
    static const char *hour_options = "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";
    alarm_roller_hour = lv_roller_create(alarm_page);
    lv_obj_add_style(alarm_roller_hour, &style, LV_PART_MAIN);
    lv_obj_add_style(alarm_roller_hour, &style_selected, LV_PART_SELECTED);
    lv_roller_set_options(alarm_roller_hour, hour_options, LV_ROLLER_MODE_NORMAL);
    lv_obj_align_to(alarm_roller_hour, label_wall, LV_ALIGN_OUT_LEFT_MID, -100, -18);
    lv_obj_set_size(alarm_roller_hour, 145, 166);
    lv_obj_set_style_text_font(alarm_roller_hour, &mono_font, 0);
    lv_roller_set_selected(alarm_roller_hour, alarm_clock[0], LV_ANIM_OFF);
    lv_obj_add_event_cb(alarm_roller_hour, roller_hour_event_handler, LV_EVENT_ALL, NULL);
    // 分钟选择滚筒
    static const char *minute_options = "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";
    alarm_roller_minute = lv_roller_create(alarm_page);
    lv_obj_add_style(alarm_roller_minute, &style, LV_PART_MAIN);
    lv_obj_add_style(alarm_roller_minute, &style_selected, LV_PART_SELECTED);
    lv_roller_set_options(alarm_roller_minute, minute_options, LV_ROLLER_MODE_NORMAL);
    lv_obj_align_to(alarm_roller_minute, label_wall, LV_ALIGN_OUT_RIGHT_MID, 0, -18);
    lv_obj_set_size(alarm_roller_minute, 145, 166);
    lv_obj_set_style_text_font(alarm_roller_minute, &mono_font, 0);
    lv_roller_set_selected(alarm_roller_minute, alarm_clock[1], LV_ANIM_OFF);
    lv_obj_add_event_cb(alarm_roller_minute, roller_minute_event_handler, LV_EVENT_ALL, NULL);

    // 左侧小时选择指示箭头
    static lv_obj_t *img_arrow_left_default = lv_image_create(alarm_page);
    lv_image_set_src(img_arrow_left_default, &left_off);
    lv_obj_align_to(img_arrow_left_default, label_wall, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    img_arrow_left = lv_image_create(alarm_page);
    lv_image_set_src(img_arrow_left, &left_on);
    lv_obj_align_to(img_arrow_left, label_wall, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_obj_set_style_image_opa(img_arrow_left, LV_OPA_0, 0);

    // 右侧分钟选择指示箭头
    static lv_obj_t *img_arrow_right_default = lv_image_create(alarm_page);
    lv_image_set_src(img_arrow_right_default, &right_off);
    lv_obj_align_to(img_arrow_right_default, label_wall, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    img_arrow_right = lv_image_create(alarm_page);
    lv_image_set_src(img_arrow_right, &right_on);
    lv_obj_align_to(img_arrow_right, label_wall, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_image_opa(img_arrow_right, LV_OPA_0, 0);

    // 闹钟开关指示 - 闹钟关
    alarm_switch_default = lv_image_create(alarm_page);
    lv_image_set_src(alarm_switch_default, &alarm_off);
    lv_obj_align(alarm_switch_default, LV_ALIGN_LEFT_MID, 70, 0);
    lv_obj_set_size(alarm_switch_default, 80, 150); // 增大可点击区域
    lv_obj_add_flag(alarm_switch_default, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(alarm_switch_default, page_alarm_clock_switch_on_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_move_foreground(alarm_switch_default); // 移动到前景层
    // 闹钟开关指示 - 闹钟开
    alarm_switch = lv_image_create(alarm_page); // 闹钟开关
    lv_image_set_src(alarm_switch, &alarm_on);
    lv_obj_align(alarm_switch, LV_ALIGN_LEFT_MID, 70, 0);
    lv_obj_set_size(alarm_switch, 80, 150); // 增大可点击区域
    lv_obj_move_background(alarm_switch);   // 移动到背景层
    lv_obj_set_style_image_opa(alarm_switch, LV_OPA_0, 0);
    lv_obj_add_flag(alarm_switch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(alarm_switch, page_alarm_clock_switch_off_event_handler, LV_EVENT_CLICKED, NULL);

    // 刷新闹钟时间和开关状态
    page_alarm_clock_refresh(alarm_clock[0], alarm_clock[1]);
}

// 异步刷新显示（由 LVGL 任务执行）
static void page_alarm_clock_refresh_async_cb(void *user)
{
    page_alarm_clock_refresh(alarm_clock[0], alarm_clock[1]);
    set_dove_animing(!alarm_clock_flag);
}

void page_alarm_clock_refresh_async()
{
    lv_async_call(page_alarm_clock_refresh_async_cb, NULL);
}

// 设置闹钟修改索引
void page_alarm_clock_set_index(int index)
{
    alarm_clock_index = index;
    page_alarm_clock_refresh_async();
}

// 设置闹钟时间
void page_alarm_clock_set_clock(uint8_t hour, uint8_t minute)
{
    alarm_clock[0] = hour;
    alarm_clock[1] = minute;
    page_alarm_clock_refresh_async();
}
// 当前选中的闹钟时间加一
void page_alarm_clock_set_index_clock_add(uint8_t num)
{
    if (alarm_clock_index == 1)
    {
        // 修改小时
        alarm_clock[0] = (alarm_clock[0] + num) % 24;
    }
    else if (alarm_clock_index == 2)
    {
        // 修改分钟
        alarm_clock[1] = (alarm_clock[1] + num) % 60;
    }
    page_alarm_clock_refresh_async();
}
// 当前选中的闹钟时间减一
void page_alarm_clock_set_index_clock_sub()
{
    if (alarm_clock_index == 1)
    {
        // 修改小时
        alarm_clock[0] = (alarm_clock[0] + 23) % 24;
    }
    else if (alarm_clock_index == 2)
    {
        // 修改分钟
        alarm_clock[1] = (alarm_clock[1] + 59) % 60;
    }
    page_alarm_clock_refresh_async();
}
// 获取闹钟开启或关闭状态
bool page_alarm_clock_get_flag()
{
    return alarm_clock_flag;
}
// 设置闹钟开启或关闭
void page_alarm_clock_set_flag(bool flag)
{
    alarm_clock_flag = flag;
    if (flag)
    {
        page_alert_show("The alarm has been set successfully.");
    }
    page_alarm_clock_refresh_async();
}

/* ************************** 闹钟响铃页面 ************************** */

// 响铃闹钟页面
void page_warning_clock()
{
    warning_page = lv_obj_create(NULL);
    lv_obj_set_size(warning_page, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    page_head(warning_page);  // 创建页面头部
    page_style(warning_page); // 设置页面风格

    static lv_style_t page_warning_style;
    lv_style_init(&page_warning_style);
    lv_style_set_bg_image_src(&page_warning_style, &warning_bg); // 设置背景图片
    lv_style_set_bg_image_opa(&page_warning_style, LV_OPA_COVER);
    lv_style_set_bg_image_tiled(&page_warning_style, false);
    lv_obj_add_style(warning_page, &page_warning_style, 0);

    alarm_warning_cloud = lv_image_create(warning_page);
    lv_image_set_src(alarm_warning_cloud, &warning_off);
    lv_obj_align(alarm_warning_cloud, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_image_opa(alarm_warning_cloud, LV_OPA_0, 0);

    alarm_warning_lightning = lv_image_create(warning_page);
    lv_image_set_src(alarm_warning_lightning, &warning_on);
    lv_obj_align(alarm_warning_lightning, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_image_opa(alarm_warning_lightning, LV_OPA_COVER, 0);

    warning_timer = lv_timer_create(
        [](lv_timer_t *timer)
        {
            if (warning_toggle)
            {
                lv_obj_set_style_image_opa(alarm_warning_cloud, LV_OPA_COVER, 0);
                lv_obj_set_style_image_opa(alarm_warning_lightning, LV_OPA_0, 0);
            }
            else
            {
                lv_obj_set_style_image_opa(alarm_warning_cloud, LV_OPA_0, 0);
                lv_obj_set_style_image_opa(alarm_warning_lightning, LV_OPA_COVER, 0);
            }
            warning_toggle = !warning_toggle;
        },
        WARING_TOGGLE_INTERVAL_MS,
        NULL);
    lv_timer_pause(warning_timer);
}

/* ************************** 页面间的切换 ************************** */
void page_switch(page_index_t page)
{
    switch (page)
    {
    case PAGE_INDEX_TIME_CLOCK:
        lv_scr_load(now_page);
        break;
    case PAGE_INDEX_ALARM_CLOCK:
        lv_scr_load(alarm_page);
        break;
    case PAGE_INDEX_WARNING_CLOCK:
        lv_scr_load(warning_page);
        break;
    default:
        lv_scr_load(now_page);
        break;
    }
}

// 在线程安全的上下文中调度页面切换（由 LVGL 任务执行）
static void page_switch_async_cb(void *user)
{
    page_index_t page = (page_index_t)(intptr_t)user;
    page_switch(page);
    // 切换到响铃页面时启动定时器，使闪电和云朵交替显示
    if (page == PAGE_INDEX_WARNING_CLOCK)
    {
        if (warning_timer)
        {
            lv_timer_resume(warning_timer);
        }
    }
    else
    {
        if (warning_timer)
        {
            lv_timer_pause(warning_timer);
        }
    }
}

void page_switch_async(page_index_t page)
{
    lv_async_call(page_switch_async_cb, (void *)(intptr_t)page);
    current_page = page; // 记录当前在哪个页面
}
