#ifndef _IMG_H_
#define _IMG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// 图片资源声明
extern const lv_image_dsc_t time_bg;
extern const lv_image_dsc_t alarm_bg;
extern const lv_image_dsc_t alarm_off;
extern const lv_image_dsc_t alarm_on;
extern const lv_image_dsc_t left_on;
extern const lv_image_dsc_t left_off;
extern const lv_image_dsc_t right_off;
extern const lv_image_dsc_t right_on;
extern const lv_image_dsc_t warning_bg;
extern const lv_image_dsc_t warning_off;
extern const lv_image_dsc_t warning_on;
// 引入 time_page_mascot 图片资源
#include "time_page_mascot/time_page_mascot.h"

#ifdef __cplusplus
}
#endif

#endif /* _IMG_H_ */
