#include "ble.h"

static ble_t g_ble={0};

static void ble_power_on_cbk(uint8_t status)
{
    osal_printk("power on status:0x%x\r\r\n", status);
    enable_ble();
}
static void ble_enable_cbk(uint8_t status)
{
    osal_printk("enable status:0x%x\r\r\n", status);
    ble_hiddev_keyboard_server_init();
    ble_hid_set_adv_data();
    ble_hid_start_adv();
}
static void ble_adv_enable_cbk(uint8_t adv_id, adv_status_t status)
{
    osal_printk("adv enable cbk adv_id:%d status:%d\r\n", adv_id, status);
}
static void ble_adv_disable_cbk(uint8_t adv_id, adv_status_t status)
{
    osal_printk("adv disable adv_id: %d, status:%d\r\n", adv_id, status);
}
static void ble_connect_change_cbk(uint16_t conn_id, 
                                bd_addr_t *addr, 
                                gap_ble_conn_state_t conn_state, 
                                gap_ble_pair_state_t pair_state, 
                                gap_ble_disc_reason_t disc_reason)
{
    g_ble.conn_id=conn_id;
    g_ble.conn_state=conn_state;

    osal_printk("connect state change conn_id: %d, status: %d, pair_status:%d, disc_reason 0x%x\r\n",
                conn_id, conn_state, pair_state, disc_reason);
    osal_printk("addr:\r\n");
    for (uint8_t i = 0; i < BD_ADDR_LEN; i++) {
        osal_printk("%2x", addr->addr[i]);
    }
    osal_printk("\r\n");

    if (conn_state == GAP_BLE_STATE_CONNECTED) {
    } else if (conn_state == GAP_BLE_STATE_DISCONNECTED) {
        // ble_keyboard_set_adv_data();
        ble_hid_set_adv_data();
        // ble_keyboard_start_adv();
        ble_hid_start_adv();
    }
}
void ble_hid_server_init(void)
{
    bts_dev_manager_callbacks_t dev_mgr_cb = { 0 };
    gap_ble_callbacks_t gap_cb = { 0 };
    dev_mgr_cb.power_on_cb = ble_power_on_cbk;
    dev_mgr_cb.ble_enable_cb = ble_enable_cbk;
    gap_cb.start_adv_cb = ble_adv_enable_cbk;
    gap_cb.stop_adv_cb = ble_adv_disable_cbk;
    gap_cb.conn_state_change_cb = ble_connect_change_cbk;
    if (gap_ble_register_callbacks(&gap_cb) != ERRCODE_BT_SUCCESS ||
        bts_dev_manager_register_callbacks(&dev_mgr_cb) != ERRCODE_BT_SUCCESS) {
        osal_printk("register ble_enable_cbk failed\r\r\n");
    }
    enable_ble();
}

ble_t* get_Blefam(void)
{
    return &g_ble;
}