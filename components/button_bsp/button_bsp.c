#include "button_bsp.h"
#include "multi_button.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "user_config.h"

EventGroupHandle_t pwr_groups;
EventGroupHandle_t l_groups;
EventGroupHandle_t r_groups;

// 板载电源按键
static Button button_pwr;   // 申请按键
#define BUTTON_PWR 16       // 实际的GPIO
#define button_pwr_id 1     // 按键的ID
#define button_pwr_active 0 // 有效电平

// 床头按键
static Button button_L;      // 申请按键
#define BUTTON_L BUTTON_PIN1 // 实际的GPIO
#define button_l_id 2        // 按键的ID
#define button_l_active 0    // 有效电平

static Button button_R;      // 申请按键
#define BUTTON_R BUTTON_PIN2 // 实际的GPIO
#define button_r_id 3        // 按键的ID
#define button_r_active 0    // 有效电平

/* ***************************** 回调事件声明 ***************************** */

static void on_button_pwr_long_press_start(Button *btn_handle);
static void on_button_pwr_long_press_hold(Button *btn_handle);

static void on_button_l_single_click(Button *btn_handle);
static void on_button_l_double_click(Button *btn_handle);
static void on_button_l_long_press_start(Button *btn_handle);
static void on_button_r_single_click(Button *btn_handle);
static void on_button_r_double_click(Button *btn_handle);
static void on_button_r_long_press_start(Button *btn_handle);

/**************************************************************************/

static void clock_task_callback(void *arg)
{
  button_ticks(); // 状态回调
}
static uint8_t read_button_GPIO(uint8_t button_id) // 返回GPIO电平
{
  switch (button_id)
  {
  case button_pwr_id:
    return gpio_get_level(BUTTON_PWR);
  case button_l_id:
    return gpio_get_level(BUTTON_L);
  case button_r_id:
    return gpio_get_level(BUTTON_R);
  default:
    break;
  }
  return 1;
}

static void gpio_init(void)
{
  gpio_config_t gpio_conf = {};
  gpio_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_conf.mode = GPIO_MODE_INPUT;
  gpio_conf.pin_bit_mask = ((uint64_t)0x01 << BUTTON_PWR) | ((uint64_t)0x01 << BUTTON_L) | ((uint64_t)0x01 << BUTTON_R);
  gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;

  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));
}
void button_Init(void)
{
  pwr_groups = xEventGroupCreate();
  l_groups = xEventGroupCreate();
  r_groups = xEventGroupCreate();

  if (!pwr_groups || !l_groups || !r_groups)
  {
    ESP_LOGE("button_bsp", "EventGroup create failed: pwr=%p l=%p r=%p",
             pwr_groups, l_groups, r_groups);
  }

  gpio_init();

  button_init(&button_pwr, read_button_GPIO, button_pwr_active, button_pwr_id);     // 初始化 初始化对象 回调函数 触发电平 按键ID
  button_attach(&button_pwr, BTN_LONG_PRESS_START, on_button_pwr_long_press_start); // 长按事件
  button_attach(&button_pwr, BTN_LONG_PRESS_HOLD, on_button_pwr_long_press_hold);   // 长按保持事件

  button_init(&button_L, read_button_GPIO, button_l_active, button_l_id);
  button_attach(&button_L, BTN_SINGLE_CLICK, on_button_l_single_click);         // 单击事件
  button_attach(&button_L, BTN_DOUBLE_CLICK, on_button_l_double_click);         // 双击事件
  button_attach(&button_L, BTN_LONG_PRESS_START, on_button_l_long_press_start); // 长按事件
  button_init(&button_R, read_button_GPIO, button_r_active, button_r_id);
  button_attach(&button_R, BTN_SINGLE_CLICK, on_button_r_single_click);         // 单击事件
  button_attach(&button_R, BTN_DOUBLE_CLICK, on_button_r_double_click);         // 双击事件
  button_attach(&button_R, BTN_LONG_PRESS_START, on_button_r_long_press_start); // 长按事件

  const esp_timer_create_args_t clock_tick_timer_args =
      {
          .callback = &clock_task_callback,
          .name = "clock_task",
          .arg = NULL,
      };
  esp_timer_handle_t clock_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&clock_tick_timer_args, &clock_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(clock_tick_timer, 1000 * 5)); // 5ms

  button_start(&button_pwr); // 启动按键
  button_start(&button_L);
  button_start(&button_R);
}

/*
事件:
SINGLE_CLICK :单击
DOUBLE_CLICK :双击
PRESS_DOWN :按下
PRESS_UP :弹起事件
PRESS_REPEAT :重复按下
LONG_PRESS_START :长按触发一次
LONG_PRESS_HOLD :长按一直触发
*/

/* ***************************** 板载电源按键事件 ***************************** */
/*长按*/
static void on_button_pwr_long_press_start(Button *btn_handle)
{
  if (pwr_groups)
  {
    xEventGroupSetBits(pwr_groups, set_bit_button(1));
  }
}
/*长按保持*/
static void on_button_pwr_long_press_hold(Button *btn_handle)
{
  if (pwr_groups)
  {
    xEventGroupSetBits(pwr_groups, set_bit_button(2));
  }
}

/* ***************************** 床头两个按键事件 ***************************** */
/*单击*/
static void on_button_l_single_click(Button *btn_handle)
{
  xEventGroupSetBits(l_groups, set_bit_button(0));
}
static void on_button_r_single_click(Button *btn_handle)
{
  xEventGroupSetBits(r_groups, set_bit_button(0));
}
/* 双击 */
static void on_button_l_double_click(Button *btn_handle)
{
  xEventGroupSetBits(l_groups, set_bit_button(1));
}
static void on_button_r_double_click(Button *btn_handle)
{
  xEventGroupSetBits(r_groups, set_bit_button(1));
}
/* 长按 */
static void on_button_l_long_press_start(Button *btn_handle)
{
  xEventGroupSetBits(l_groups, set_bit_button(2));
}
static void on_button_r_long_press_start(Button *btn_handle)
{
  xEventGroupSetBits(r_groups, set_bit_button(2));
}