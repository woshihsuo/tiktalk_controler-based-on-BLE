#include "led.h"

void led_init(void)
{
    uapi_pin_set_mode(LED_PIN, HAL_PIO_FUNC_GPIO);

    uapi_gpio_set_dir(LED_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(LED_PIN, GPIO_LEVEL_LOW);
}