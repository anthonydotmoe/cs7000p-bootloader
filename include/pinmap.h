#pragma once

#include "stm32h7xx.h"

/* Power control */
#define BAT_DETECT   GPIOB,2
#define MAIN_PWR_DET GPIOA,6

/* Push-to-talk and side keys */
#define PTT_SW       GPIOA,8
#define PTT_EXT      GPIOE,11
#define SIDE_KEY1    GPIOD,15
#define SIDE_KEY2    GPIOE,12
#define SIDE_KEY3    GPIOE,13
#define ALARM_KEY    GPIOE,10

/* USB */
#define USB_DP       GPIOA,12
#define USB_DM       GPIOA,11

#define LCD_BACKLIGHT GPIOC,9

/* SN74HC595 GPIO extenders */
#define GPIOEXT_CLK  GPIOE,7
#define GPIOEXT_DAT  GPIOE,9
#define GPIOEXT_STR  GPIOE,8
