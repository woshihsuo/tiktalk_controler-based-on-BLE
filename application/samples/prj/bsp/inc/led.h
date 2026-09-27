#ifndef LED_H
#define LED_H

#include "common_def.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"

#include "pinctrl.h"
#include "gpio.h"

#define LED_PIN S_MGPIO31

void led_init(void);

#endif