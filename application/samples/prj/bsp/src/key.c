#include "key.h"

void key_init(void)
{
    uapi_pin_set_mode(S_MGPIO8, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(S_MGPIO8, GPIO_DIRECTION_INPUT);
    uapi_pin_set_pull(S_MGPIO8,PIN_PULL_UP);

    uapi_tcxo_init();

    // static timer_handle_t timer_handler={0};
    // uapi_timer_deinit();
    // uapi_timer_init();
    // uapi_timer_adapter(TIMER_INDEX_0,TIMER_1_IRQN,1);
    // uapi_timer_create(TIMER_INDEX_0,&timer_handler);
    // uapi_timer_start(timer_handler,);
}

static uint8_t kf=0;
static uint32_t start_time=0;
key_state_t key_read_status(void)
{
    if (KEY_PRESSED)
    {
        if(kf==0)
        {
            start_time=uapi_tcxo_get_ms();
            kf=1;
        }
        if (KEY_RELEASED&&kf==1)
        {
            printf("start_time:%d\r\n",start_time);
            uint32_t end_time=uapi_tcxo_get_ms();
            printf("end_time:%d\r\n",end_time);
            if (end_time-start_time<=500)
            {
                kf=0;
                return KEY_CLICKED;
            }

            if (end_time-start_time>=1000)
            {
                kf=0;
                return KEY_LONG_PRESSED;
            }
            
        }

        
    }

    return 0xff;
}