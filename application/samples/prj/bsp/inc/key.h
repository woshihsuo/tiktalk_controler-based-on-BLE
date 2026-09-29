#ifndef KEY_H
#define KEY_H

#include "main.h"
#include "pinctrl.h"
#include "gpio.h"
#include "timer.h"
#include "tcxo.h"
#include "chip_core_irq.h"

#define KEY_PIN S_MGPIO8

#define KEY_PRESSED (GPIO_LEVEL_LOW==uapi_gpio_get_val(KEY_PIN))
#define KEY_RELEASED (GPIO_LEVEL_HIGH==uapi_gpio_get_val(KEY_PIN))

typedef enum{
    KEY_LONG_PRESSED,
    KEY_CLICKED,
    KEY_DOUBLE_CLICKED
}key_state_t;

void key_init(void);
key_state_t key_read_status(void);

#endif
