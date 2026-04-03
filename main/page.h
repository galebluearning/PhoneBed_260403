#ifndef __PAGE_H
#define __PAGE_H

#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "user_config.h"

typedef enum
{
    PAGE_INDEX_TIME_CLOCK,   // 显示当前时间的页面
    PAGE_INDEX_ALARM_CLOCK,  // 闹钟设置页面
    PAGE_INDEX_WARNING_CLOCK // 响铃闹钟页面
} page_index_t;

void page_create(); // 初始化页面
void page_time();   // 显示当前时间的页面

void page_alarm_clock();               // 闹钟设置页面
void page_alarm_clock_refresh_async(); // 线程安全的异步刷新设置闹钟页面

void page_slider_clock(); // 滑块关闭闹钟页面

void page_warning_clock(); // 响铃闹钟页面

// *************************** 给外部使用的函数 ***************************
void page_switch_async(page_index_t page); // 线程安全的异步页面切换

void page_alarm_clock_set_index(int index);                    // 设置闹钟修改索引
void page_alarm_clock_set_clock(uint8_t hour, uint8_t minute); // 设置闹钟时间
void page_alarm_clock_set_index_clock_add(uint8_t num);        // 当前选中的闹钟时间加 num
void page_alarm_clock_set_index_clock_sub();                   // 当前选中的闹钟时间减 1
bool page_alarm_clock_get_flag();                              // 获取闹钟开启或关闭状态
void page_alarm_clock_set_flag(bool flag);                     // 设置闹钟开启或关闭

#endif // __PAGE_H
