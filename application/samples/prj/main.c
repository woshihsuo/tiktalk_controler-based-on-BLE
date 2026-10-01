#include "main.h"
#include "securec.h"

/* Touch screen HID report (Android Digitizer), wire format:
 * [report id=1][status][x_lo][x_hi][y_lo][y_hi] = 6 bytes
 * status bit0=Tip Switch(1=touching), bit1=In Range
 * X/Y absolute, range 0~10000, Android maps them onto the real screen.
 */
#define HID_TOUCH_REPORT_LEN            6
#define HID_TOUCH_REPORT_ID             1
#define HID_TOUCH_TIP_SWITCH            0x01
#define HID_TOUCH_IN_RANGE              0x02

/* swipe geometry (in 0~10000 coords): horizontal center, vertical middle band,
 * kept away from screen edges to avoid edge gestures / accidental taps
 */
#define TOUCH_X_CENTER                  5000
#define TOUCH_Y_START                   7500
#define TOUCH_Y_END                     2500
#define SWIPE_STEPS                     10
#define SWIPE_STEP_DELAY_MS             15

static uint8_t g_touch_report[HID_TOUCH_REPORT_LEN];

static void ble_touch_send(uint8_t tip, uint16_t x, uint16_t y)
{
    g_touch_report[0] = HID_TOUCH_REPORT_ID;
    g_touch_report[1] = tip ? (HID_TOUCH_TIP_SWITCH | HID_TOUCH_IN_RANGE) : 0x00;
    g_touch_report[2] = (uint8_t)(x & 0xFF);
    g_touch_report[3] = (uint8_t)((x >> 8) & 0xFF);
    g_touch_report[4] = (uint8_t)(y & 0xFF);
    g_touch_report[5] = (uint8_t)((y >> 8) & 0xFF);
    errcode_t ret = ble_hiddev_keyboard_server_send_input_report_by_uuid(g_touch_report, HID_TOUCH_REPORT_LEN);
    printf("touch tip=%d y=%u ret=%d\r\n", tip, y, ret);
}

/* one complete Android touch gesture: down -> move -> up.
 * move_up=true : finger y 7500 -> 2500, content scrolls up (Douyin: next video)
 */
static void ble_swipe(bool move_up)
{
    uint16_t y_start = move_up ? TOUCH_Y_START : TOUCH_Y_END;
    uint16_t y_end   = move_up ? TOUCH_Y_END   : TOUCH_Y_START;

    ble_touch_send(1, TOUCH_X_CENTER, y_start); /* touch down */
    osDelay(20);
    for (uint16_t i = 1; i <= SWIPE_STEPS; i++) {
        uint16_t y = (uint16_t)(y_start + ((int32_t)y_end - y_start) * i / SWIPE_STEPS);
        ble_touch_send(1, TOUCH_X_CENTER, y); /* move */
        osDelay(SWIPE_STEP_DELAY_MS);
    }
    ble_touch_send(0, TOUCH_X_CENTER, y_end); /* touch up: full clean release */
    osDelay(20);
}


static void hid_task(void *arg)
{
    unused(arg);

    while (1) {
        ble_t *ble = get_Blefam();
        if (ble->conn_state == GAP_BLE_STATE_CONNECTED) {
            ble_swipe(true); /* single direction: next video. false = scroll back */
            /* ble_tap(5000, 9000); */ /* example: tap at a fixed position (e.g. like button) */
        }
        uapi_gpio_toggle(LED_PIN);
        osDelay(2000);
    }
}

static void task_entry(void)
{

    led_init();
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
