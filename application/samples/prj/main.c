#include "main.h"
#include "securec.h"

/* Mouse HID report, wire format:
 * [report id=1][buttons][dx][dy][wheel] = 5 bytes
 * dx/dy: relative movement, -127~127; dy>0 = pointer moves down
 * (matches the Report Map declared in ble_hid.c, which has Report Id 1)
 */
#define HID_MOUSE_REPORT_LEN            5
#define HID_MOUSE_REPORT_ID             1
#define HID_MOUSE_BTN_LEFT              0x01

/* one swipe = press left button + SWIPE_STEPS moves of SWIPE_STEP + release.
 * total drag distance = SWIPE_STEPS * SWIPE_STEP (pointer units), tune to taste
 */
#define SWIPE_STEPS                     3
#define SWIPE_STEP                      30
#define SWIPE_STEP_DELAY_MS             25

static uint8_t g_mouse_report[HID_MOUSE_REPORT_LEN];

static void ble_mouse_send(uint8_t buttons, int8_t dx, int8_t dy)
{
    g_mouse_report[0] = HID_MOUSE_REPORT_ID;
    g_mouse_report[1] = buttons;
    g_mouse_report[2] = (uint8_t)dx;
    g_mouse_report[3] = (uint8_t)dy;
    g_mouse_report[4] = 0x00; /* wheel */
    errcode_t ret = ble_hiddev_keyboard_server_send_input_report_by_uuid(g_mouse_report, HID_MOUSE_REPORT_LEN);
    printf("mouse btn=0x%02x dy=%d ret=%d\r\n", buttons, dy, ret);
}

/* one swipe gesture via left-button drag (iOS AssistiveTouch pointer) */
static void ble_swipe(bool move_up)
{
    int8_t dy = move_up ? -(int8_t)SWIPE_STEP : (int8_t)SWIPE_STEP;

    ble_mouse_send(HID_MOUSE_BTN_LEFT, 0, 0); /* press left button */
    osDelay(20);
    for (uint8_t i = 0; i < SWIPE_STEPS; i++) { /* drag */
        ble_mouse_send(HID_MOUSE_BTN_LEFT, 0, dy);
        osDelay(SWIPE_STEP_DELAY_MS);
    }
    ble_mouse_send(0, 0, 0); /* release */
    osDelay(20);
}

static void hid_task(void *arg)
{
    unused(arg);
    bool swipe_up = false; /* alternate swipe up / down each cycle */

    while (1) {
        ble_t *ble = get_Blefam();
        key_time_t* time = get_Key_time();
        printf("%d-%d=%d\r\n",time->end_time,time->start_time,time->end_time-time->start_time);
        if ((time->end_time-time->start_time<=300)&(ble->conn_state == GAP_BLE_STATE_CONNECTED)) {
            ble_swipe(swipe_up);
            uapi_gpio_toggle(LED_PIN);
        }
        // osDelay(1000);
    }
}

// static void test_task(void *arg)
// {
//     unused(arg);
//     while (1) {
        
//         printf("%d-%d=%d\r\n",time->end_time,time->start_time,time->end_time-time->start_time);
//     }
// }

static void task_entry(void)
{

    led_init();
    
    ble_hid_server_init();

    key_init();
    osThreadAttr_t attr;

    attr.name = "hid_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x1000;
    attr.priority = 17;

    if (osThreadNew(hid_task, NULL, &attr) == NULL) {
        /* Create task fail. */
    }

    // if (osThreadNew(test_task, NULL, &attr) == NULL) {
    //     /* Create task fail. */
    // }
}

/* Run the task_entry. */
app_run(task_entry);
