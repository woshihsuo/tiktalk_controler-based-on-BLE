#ifndef BLE_H
#define BLE_H

#include "main.h"
#include "soc_osal.h"

#include "stdbool.h"
#include "product.h"
#include "common_def.h"
#include "osal_debug.h"
#include "securec.h"
#include "bts_device_manager.h"
#include "bts_le_gap.h"
#include "bts_gatt_stru.h"
#include "bts_gatt_server.h"

typedef struct
{
    uint16_t conn_id;
    gap_ble_conn_state_t conn_state;
}ble_t;

void ble_hid_server_init(void);
ble_t* get_Blefam(void);

/* Ble Adv type Flag length */
#define BLE_ADV_FLAG_LEN 0x03
/* Ble Adv data length */
#define BLE_GENERAL_BYTE_1 1
/* Ble Adv appearance length */
#define BLE_ADV_APPEARANCE_LENGTH 4
/* Ble Adv appearance type */
#define BLE_ADV_APPEARANCE_DATA_TYPE 0x19
/* Ble keyboard appearance type */
#define BLE_ADV_CATEGORY_KEYBOARD_VALUE 0x0080
/* Ble keyboard categorylength */
#define BLE_ADV_CATEGORY_LEN 2
/* Ble name adv param type length */
#define BLE_ADV_PARAM_DATATYPE_LENGTH 1
/* Ble name adv name type */
#define BLE_ADV_LOCAL_NAME_DATA_TYPE 0x09
/* Ble name adv tx power type */
#define BLE_ADV_TX_POWER_LEVEL       0x0A
/* Ble name adv tx power response type */
#define BLE_SCAN_RSP_TX_POWER_LEVEL_LEN 0x03
/* Ble adv flag data */
#define BLE_ADV_FLAG_DATA 0x05

#define HID_SERVICE 0x1812
#define BLE_SERVICE_DATA_16_UUID 0x16

typedef struct {
    uint8_t length;
    uint8_t adv_data_type;
    uint8_t flags;
} ble_adv_flag;

typedef struct {
    uint8_t length;
    uint8_t adv_data_type;
    uint8_t catogory_id[BLE_ADV_CATEGORY_LEN];
} ble_appearance_t;

/**
 * @if Eng
 * @brief Definitaion of BLE device name.
 * @else
 * @brief BLE 广播设备名称。
 * @endif
 */
typedef struct {
    uint8_t length;
    uint8_t adv_data_type;
    int8_t *name;
} ble_local_name_t;

/**
 * @if Eng
 * @brief Definitaion of BLE Tx power.
 * @else
 * @brief BLE 广播发送功率。
 * @endif
 */
typedef struct {
    uint8_t length;
    uint8_t adv_data_type;
    uint8_t tx_power_value;
} ble_tx_power_level_t;

/**
 * @if Eng
 * @brief Definitaion value range for typedef struct ble_adv_para.adv_filter_policy.
 * @else
 * @brief Ble adv filter policy定义值范围。
 * @endif
 */
typedef enum ble_adv_filter_policy {
    BLE_ADV_FILTER_POLICY_SCAN_ANY_CONNECT_ANY =                     0x00,
    BLE_ADV_FILTER_POLICY_SCAN_WHITE_LIST_CONNECT_ANY =              0x01,
    BLE_ADV_FILTER_POLICY_SCAN_ANY_CONNECT_WHITE_LIST =              0x02,
    BLE_ADV_FILTER_POLICY_SCAN_WHITE_LIST_CONNECT_WHITE_LIST =       0x03
} ble_adv_filter_policy_t;

/**
 * @if Eng
 * @brief Definitaion value range for adv type.
 * @else
 * @brief Ble adv 类型范围。
 * @endif
 */
typedef enum ble_adverting_type {
    BLE_ADV_TYPE_CONNECTABLE_UNDIRECTED =                            0x00,
    BLE_ADV_TYPE_CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED =              0x01,
    BLE_ADV_TYPE_SCANNABLE_UNDIRECTED =                              0x02,
    BLE_ADV_TYPE_NON_CONNECTABLE_UNDIRECTED =                        0x03,
    BLE_ADV_TYPE_CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED =               0x04
} ble_adverting_type_t;


/**
 * @if Eng
 * @brief Definitaion value range for adv channel map.
 * @else
 * @brief Ble 广播信道范围。
 * @endif
 */
typedef enum ble_adv_channel_map {
    BLE_ADV_CHANNEL_MAP_CH_37 =                      0x01,
    BLE_ADV_CHANNEL_MAP_CH_38 =                      0x02,
    BLE_ADV_CHANNEL_MAP_CH_39 =                      0x04,
    BLE_ADV_CHANNEL_MAP_CH_37_CH_38 =                0x03,
    BLE_ADV_CHANNEL_MAP_CH_37_CH_39 =                0x05,
    BLE_ADV_CHANNEL_MAP_CH_38_CH_39 =                0x06,
    BLE_ADV_CHANNEL_MAP_CH_DEFAULT =                 0x07
} ble_adv_channel_map_t;

/**
 * @if Eng
 * @brief Definitaion value range for adv addr type.
 * @else
 * @brief Ble 广播地址类型。
 * @endif
 */
typedef enum {
    BLE_PUBLIC_DEVICE_ADDRESS =                             0x00,
    BLE_RANDOM_DEVICE_ADDRESS =                             0x01,
    BLE_PUBLIC_IDENTITY_ADDRESS =                           0x02,
    BLE_RANDOM_STATIC_IDENTITY_ADDRESS =                    0x03
} ble_address_type;

/* Ble adv min interval */
#define BLE_ADV_MIN_INTERVAL 0x30
/* Ble adv max interval */
#define BLE_ADV_MAX_INTERVAL 0x60
/* Ble adv handle */
#define BTH_GAP_BLE_ADV_HANDLE_DEFAULT 0x01
/* Ble adv duration */
#define BTH_GAP_BLE_ADV_FOREVER_DURATION 0

/**
 * @if Eng
 * @brief  Enable BLE adv.
 * @attention  NULL
 * @retval BT_STATUS_SUCCESS    Excute successfully
 * @retval BT_STATUS_FAIL       Execute fail
 * @par Dependency:
 * @li bts_def.h
 * @else
 * @brief  使能BLE广播。
 * @attention  NULL
 * @retval BT_STATUS_SUCCESS    执行成功
 * @retval BT_STATUS_FAIL       执行失败
 * @par 依赖:
 * @li bts_def.h
 * @endif
 */
uint8_t ble_hid_start_adv(void);

/**
 * @if Eng
 * @brief  BLE adv data config.
 * @attention  NULL
 * @retval BT_STATUS_SUCCESS    Excute successfully
 * @retval BT_STATUS_FAIL       Execute fail
 * @par Dependency:
 * @li bts_def.h
 * @else
 * @brief  BLE广播数据配置。
 * @attention  NULL
 * @retval BT_STATUS_SUCCESS    执行成功
 * @retval BT_STATUS_FAIL       执行失败
 * @par 依赖:
 * @li bts_def.h
 * @endif
 */
uint8_t ble_hid_set_adv_data(void);


/* Human Interface Device UUID */
#define BLE_UUID_HUMAN_INTERFACE_DEVICE 0x1812
/* HID Information UUID */
#define BLE_UUID_HID_INFORMATION 0x2A4A
/* Report Map UUID */
#define BLE_UUID_REPORT_MAP 0x2A4B
/* HID Control Point UUID */
#define BLE_UUID_HID_CONTROL_POINT 0x2A4C
/* Boot Keyboard Input Report UUID */
#define BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT 0x2A22
/* Boot Keyboard Output Report UUID */
#define BLE_UUID_BOOT_KEYBOARD_OUTPUT_REPORT 0x2A32
/* Report UUID */
#define BLE_UUID_REPORT 0x2A4D
/* Protocol Mode UUID */
#define BLE_UUID_PROTOCOL_MODE 0x2A4E
/* Client Characteristic Configuration UUID */
#define BLE_UUID_CLIENT_CHARACTERISTIC_CONFIGURATION 0x2902
/* External Report Reference UUID */
#define BLE_UUID_EXTERNAL_REPORT_REFERENCE 0x2907
/* Report Reference UUID */
#define BLE_UUID_REPORT_REFERENCE 0x2908

/**
 * @if Eng
 * @brief  Use this funtion to change name value.
 * @par name, len.
 * @attention  NULL
 * @param  NULL
 * @retval NULL
 * @par Dependency:
 * @li ble_hid_server.h
 * @else
 * @brief  设置注册服务时的name
 * @par NULL
 * @attention  NULL
 * @param  NULL
 * @retval NULL
 * @par 依赖:
 * @li ble_hid_server.h
 * @endif
 */
void ble_hiddev_set_device_name_value(const uint8_t *name, const uint8_t len);

/**
 * @if Eng
 * @brief  Use this funtion to change appearance value.
 * @par appearance.
 * @attention  NULL
 * @param  NULL
 * @retval NULL
 * @par Dependency:
 * @li ble_hid_server.h
 * @else
 * @brief  设置注册服务时的appearance
 * @par NULL
 * @attention  NULL
 * @param  NULL
 * @retval NULL
 * @par 依赖:
 * @li ble_hid_server.h
 * @endif
 */
void ble_hiddev_set_device_appearance_value(uint16_t appearance);

/**
 * @if Eng
 * @brief  Use this funtion to init HID service.
 * @par Description:add HID service, character and character descriptor, then start service.
 * @attention  NULL
 * @param  NULL
 * @retval NULL
 * @par Dependency:
 * @li ble_hid_server.h
 * @else
 * @brief  初始化HID服务。
 * @par 说明:添加HID服务，特征，特征描述符，然后启动服务。
 * @attention  NULL
 * @param  NULL
 * @retval NULL
 * @par 依赖:
 * @li ble_hid_server.h
 * @endif
 */
void ble_hiddev_keyboard_server_init(void);

/**
 * @if Eng
 * @brief  Notify remote device the press button value.
 * @par Description:Notify remote device the press button value..
 * @attention  NULL
 * @param  [in]  value  Press button value.
 * @param  [in]  len    Length of press button value。
 * @retval ERRCODE_BT_SUCCESS    Excute successfully
 * @retval ERRCODE_BT_FAIL       Execute fail
 * @par Dependency:
 * @li ble_hid_server.h
 * @else
 * @brief  通知对端按键变化。
 * @par 说明：通知对端按键变化。
 * @attention  NULL
 * @param  [in]  data   按键值。
 * @param  [in]  len    按键值数据长度。
 * @retval ERRCODE_BT_SUCCESS    执行成功
 * @retval ERRCODE_BT_FAIL       执行失败
 * @par 依赖:
 * @li ble_hid_server.h
 * @endif
 */
errcode_t ble_hiddev_keyboard_server_send_input_report(const uint8_t *data, uint8_t len);

/**
 * @if Eng
 * @brief  Notify remote device the press button value.
 * @par Description:Notify remote device the press button value..
 * @attention  NULL
 * @param  [in]  data  Press button value.
 * @param  [in]  len   Length of press button value。
 * @retval ERRCODE_BT_SUCCESS    Excute successfully
 * @retval ERRCODE_BT_FAIL       Execute fail
 * @par Dependency:
 * @li ble_hid_server.h
 * @else
 * @brief  通知对端按键变化。
 * @par 说明：通知对端按键变化。
 * @attention  NULL
 * @param  [in]  data   按键值。
 * @param  [in]  len    按键值数据长度。
 * @retval ERRCODE_BT_SUCCESS    执行成功
 * @retval ERRCODE_BT_FAIL       执行失败
 * @par 依赖:
 * @li ble_hid_server.h
 * @endif
 */
errcode_t ble_hiddev_keyboard_server_send_input_report_by_uuid(const uint8_t *data, uint8_t len);


#endif