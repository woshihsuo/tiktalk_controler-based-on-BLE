#include "main.h"
#include "securec.h"

#define BLE_KEYBOARD_MAX_KEY_LENTH              6
#define BLE_KEYBOARD_REPORT_LENTH               7
#define USB_HID_SPECIAL_KEY_MIN                 0xE0
#define USB_HID_SPECIAL_KEY_MAX                 0xE7

/* HID keyboard keycode: Keyboard DownArrow */
#define HID_KEY_DOWN_ARROW                      0x51

typedef struct usb_hid_keyboard_report {
    uint8_t special_key;                         /*!< 8bit special key(Lctrl Lshift Lalt Lgui Rctrl Rshift Ralt Rgui) */
    uint8_t key[BLE_KEYBOARD_MAX_KEY_LENTH];     /*!< Normal key */
} usb_hid_keyboard_report_t;

static usb_hid_keyboard_report_t g_send_key_msg;

static void ble_keyboard_fill_report(uint8_t key_num, uint8_t key_value[])
{
    uint8_t normal_key_num = BLE_KEYBOARD_MAX_KEY_LENTH - 1;

    (void)memset_s(&g_send_key_msg, sizeof(g_send_key_msg), 0, sizeof(g_send_key_msg));
    for (uint8_t i = 0; i < key_num; i++) {
        if (key_value[i] >= USB_HID_SPECIAL_KEY_MIN && key_value[i] <= USB_HID_SPECIAL_KEY_MAX) {
            g_send_key_msg.special_key |= (1 << (key_value[i] - USB_HID_SPECIAL_KEY_MIN));
        } else {
            g_send_key_msg.key[normal_key_num] = key_value[i];
            normal_key_num--;
        }
    }
}

static void hid_task(void *arg)
{
    unused(arg);
    uint8_t press_key[1] = {HID_KEY_DOWN_ARROW};

    while (1) {
        ble_t *ble = get_Blefam();
        if ((ble->conn_state == GAP_BLE_STATE_CONNECTED)&&(uapi_gpio_get_val(S_MGPIO8)==GPIO_LEVEL_LOW)) {
            /* press DownArrow */
            ble_keyboard_fill_report(sizeof(press_key), press_key);
            errcode_t ret1 = ble_hiddev_keyboard_server_send_input_report_by_uuid(
                (uint8_t *)(uintptr_t)&g_send_key_msg, BLE_KEYBOARD_REPORT_LENTH);
            osDelay(20);
            /* release: argc = 0 in the keyscan sample means an all-zero report */
            ble_keyboard_fill_report(0, NULL);
            errcode_t ret2 = ble_hiddev_keyboard_server_send_input_report_by_uuid(
                (uint8_t *)(uintptr_t)&g_send_key_msg, BLE_KEYBOARD_REPORT_LENTH);
            printf("down ret=%d, up ret=%d\r\n", ret1, ret2);
            uapi_gpio_toggle(LED_PIN);
        }
        
        osDelay(1000);
    }
}

static void task_entry(void)
{

    led_init();

    uapi_pin_set_mode(S_MGPIO8, HAL_PIO_FUNC_GPIO);

    uapi_gpio_set_dir(S_MGPIO8, GPIO_DIRECTION_INPUT);
    uapi_pin_set_pull(S_MGPIO8,PIN_PULL_UP);

    ble_hid_server_init();
    osThreadAttr_t attr;

    attr.name = "TasksTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x1000;
    attr.priority = 17;

    if (osThreadNew(hid_task, NULL, &attr) == NULL) {
        /* Create task fail. */
    }
}

/* Run the task_entry. */
app_run(task_entry);
