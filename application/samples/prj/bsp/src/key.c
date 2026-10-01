#include "key.h"
#include "cmsis_os2.h"
#include "securec.h"
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"

static unsigned long g_queue_id=0;
static void key_task (void *argument)
{
    osThreadAttr_t *attr=(osThreadAttr_t *)argument;
    printf("%s\r\n",attr->name);
    key_time_t s_key_time={0};
    uint8_t flag=0;
    while (1)
    {
        if(KEY_PRESSED && !flag)
        {
            s_key_time.start_time=uapi_tcxo_get_ms();
            printf("KEY_PRESSED\r\n");
            flag=1;
        }
        if(KEY_RELEASED && flag)
        {
            s_key_time.end_time=uapi_tcxo_get_ms();
            printf("KEY_RELEASED\r\n");
            flag=0;
            osal_msg_queue_write_copy(g_queue_id,&s_key_time,sizeof(s_key_time),0xffffffff);
        }
    }
}

key_time_t* get_Key_time(void)
{
    static key_time_t key_time={0};
    uint32_t size=sizeof(key_time);
    osal_msg_queue_read_copy(g_queue_id,&key_time,&size,0xffffffff);
    return &key_time;
}

void key_init(void)
{
    uapi_pin_set_mode(S_MGPIO8, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(S_MGPIO8, GPIO_DIRECTION_INPUT);
    uapi_pin_set_pull(S_MGPIO8,PIN_PULL_UP);

    uapi_tcxo_init();
    osal_msg_queue_create("key",1,&g_queue_id,0,(uint16_t)sizeof(key_time_t));

    static osThreadAttr_t attr={0};

    attr.name = "KeyTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x1000;
    attr.priority = 17;
    osThreadNew(key_task, &attr, &attr);
}
