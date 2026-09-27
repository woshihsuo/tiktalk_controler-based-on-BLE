/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: SLE MEASURE_DIS sample (merged). Client (carkey) and server (anchor)
 *              coexist in one translation unit. Conflicting symbols are renamed with
 *              client_/server_ prefixes.
 *
 * History:
 * 2023-04-03, Create client files.
 * 2023-07-17, Create server files.
 */
#ifndef SLE_MEASURE_DIS_H
#define SLE_MEASURE_DIS_H

#include <stdint.h>
#include <securec.h>
#include "errcode.h"
#include "std_def.h"
#include "soc_osal.h"
#include "common_def.h"
#include "sle_common.h"
#include "sle_ssap_client.h"
#include "sle_ssap_server.h"
#include "sle_hadm_manager.h"

#include "sle_errcode.h"
#include "sle_common.h"
#include "sle_device_manager.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_ssap_client.h"
#include "sle_ssap_server.h"
#include "sle_hadm_manager.h"
#include "slem_smooth.h"
#include "slem_alg_smooth_dis.h"
#include "tcxo.h"
#include "cmsis_os2.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* ========================================================================== */
/*                              公共常量                                       */
/* ========================================================================== */
#define MEASURE_DIS_NUM_CARRY_10_7     10000000
#define MEASURE_DIS_NUM_CARRY_1000     1000
#define MEASURE_DIS_NUM_CARRY_100      100
#define MEASURE_DIS_NUM_CARRY_10       10
#define MEASURE_DIS_NUM_CARRY_2        2

#define SLEM_CONNET_INVAILD 0xFF

/* IQ 数据相关 */
#define MEASURE_DIS_IQ_REPORT_CNT_MAX 1
#define POSALG_DATA_NUM 79
#define IQ_DATA_MAX (MEASURE_DIS_IQ_REPORT_CNT_MAX * SLE_CS_IQ_REPORT_COUNT)

/* 服务端算法参数 */
#define MEASURE_DIS_INVALID (-1)
#define MEASURE_DIS_RSSI_INVALID (-128)
#define MEASURE_DIS_HIGH_INVALID (-10)
#define POSALG_RSSI_OFFSET 256
#define POSALG_CALIB_VAL 0.8
#define POSALG_IQ_FLIP_BARRIER 1023
#define POSALG_IQ_FLIP_OFFSET 2048
#define MEASURE_DIS_TIMESTAMP_DIFF_MAX 20

/* 服务端消息队列/事件 */
#define MEASURE_DIS_MSG_EVENT 1
#define MEASURE_DIS_MSG_QUEUE_SIZE 10

/* 广播参数 */
#define SLE_ADV_TX_POWER                            6
#define SLE_ADV_RSSI_MIN  (-127)
/* 最大广播数据长度 */
#define SLE_ADV_DATA_LEN_MAX                        251
/* 连接调度间隔300ms，单位125us */
#define SLE_ADV_INTERVAL_KEY_DEFAULT                2400
/* 连接调度间隔25ms，单位125us */
#define SLE_ADV_INTERVAL_SLAVE_DEFAULT              200
#define SLE_ADV_CHANNEL_MAP_DEFAULT                 0x07

// HCI_LE_ADV_FILTER Max length:1 Oct action + 1 Oct index + 29 Oct Manu + 29 Oct Manu Mask
#define HCI_GLE_ADV_FILTER_SIZE  60
// HCI_LE_ADV_FILTER BLE_META_PF_MANU_DATA max len of data
#define NAME_AND_MANU_MAX_LEN   29
// HCI_LE_ADV_FILTER UUID 一个UUID最大字节数16
#define GLE_UUID_MAX_LEN 16

/* ========================================================================== */
/*                              公共类型                                       */
/* ========================================================================== */
typedef enum {
    SLEM_PROFILE_MSG_IQ = 0xFFFFFFEA,
} slem_profile_msg_type_t;

typedef enum {
    SLEM_MSG_LOCAL_IQ,
    SLEM_MSG_REMOTE_IQ,
} slem_msg_node_type_t;

typedef struct {
    uint32_t        type;                             /*!< 消息类型 */
    uint32_t        len;                              /*!< 消息长度 */
    uint8_t         data[0];
} measure_ids_msg_t;

typedef struct server_data {
    uint8_t property_uuid[SLE_UUID_LEN];
    uint8_t server_uuid[SLE_UUID_LEN];
    uint8_t service_uuid[SLE_UUID_LEN];
    uint16_t begin_hdl;
    uint16_t end_hdl;
    uint16_t property_handle;
} measure_dis_server_data_t;

typedef struct {
    uint16_t            conn_id;                          /*!< 连接句柄 */
    uint16_t            type;                             /*!< 消息类型 */
    uint16_t            result;                           /*!< 执行结果 */
    void                *data;
} measure_dis_msg_node_t;

typedef struct {
    uint16_t i_data;
    uint16_t q_data;
} measure_dis_stored_qte_trans_t;

typedef struct {
    uint8_t samp_cnt;
    uint8_t rssi;
    uint16_t es_sn;
    uint32_t timestamp_sn;
    measure_dis_stored_qte_trans_t data[IQ_DATA_MAX];
#if (defined(GLE_CS_MODE3_SUPPORT))
    uint32_t tof_result;
#endif
} measure_dis_stored_iq_data_t;

typedef struct {
    uint16_t i_data;
    uint16_t q_data;
} sle_channel_sounding_qte_trans_t;

typedef struct {
    uint8_t samp_cnt;
    uint8_t rssi;
    uint16_t es_sn;
    uint32_t timestamp_sn;
    sle_channel_sounding_qte_trans_t data[IQ_DATA_MAX];
    uint32_t tof_result;
} sle_channel_sounding_iq_trans_t;

typedef struct {
    int key_num;
    int key_num_all;
} measure_dis_keys_info;

typedef struct {
    uint32_t                  sn;                               /*!< 测量SN */
    float                     dist_first;                       /*!< 第一个测距值 */
    float                     dist_second;                      /*!< 第二个测距值 */
    float                     dist_double;                      /*!< 第二个测距值 */
    float                     rssi;                             /*!< 信号强度 */
    float                     high;                             /*!< 置信度相关系数 */
    float                     prob;                             /*!< 置信度 */
    unsigned char             smooth_num;                       /*!<有效平滑次数 */
} measure_dis_profile_msg_dis_t;

/**
 * @if Eng
 * @brief Definitaion of SLE ADV Data Type.
 * @else
 * @brief SLE 广播数据类型
 * @endif
 */
typedef enum {
    SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL                              = 0x01,   /*!< 发现等级 */
    SLE_ADV_DATA_TYPE_ACCESS_MODE                                  = 0x02,   /*!< 接入层能力 */
    SLE_ADV_DATA_TYPE_SERVICE_DATA_16BIT_UUID                      = 0x03,   /*!< 标准服务数据信息 */
    SLE_ADV_DATA_TYPE_SERVICE_DATA_128BIT_UUID                     = 0x04,   /*!< 自定义服务数据信息 */
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_16BIT_SERVICE_UUIDS         = 0x05,   /*!< 完整标准服务标识列表 */
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_128BIT_SERVICE_UUIDS        = 0x06,   /*!< 完整自定义服务标识列表 */
    SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_16BIT_SERVICE_UUIDS       = 0x07,   /*!< 部分标准服务标识列表 */
    SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_128BIT_SERVICE_UUIDS      = 0x08,   /*!< 部分标准服务标识列表 */
    SLE_ADV_DATA_TYPE_SERVICE_STRUCTURE_HASH_VALUE                 = 0x09,   /*!< 服务结构散列值 */
    SLE_ADV_DATA_TYPE_SHORTENED_LOCAL_NAME                         = 0x0A,   /*!< 设备缩写本地名称 */
    SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME                          = 0x0B,   /*!< 设备完整本地名称 */
    SLE_ADV_DATA_TYPE_TX_POWER_LEVEL                               = 0x0C,   /*!< 广播发送功率 */
    SLE_ADV_DATA_TYPE_SLB_COMMUNICATION_DOMAIN                     = 0x0D,   /*!< SLB通信域域名 */
    SLE_ADV_DATA_TYPE_SLB_MEDIA_ACCESS_LAYER_ID                    = 0x0E,   /*!< SLB媒体接入层标识 */
    SLE_ADV_DATA_TYPE_EXTENDED                                     = 0xFE,   /*!< 数据类型扩展 */
    SLE_ADV_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA                   = 0xFF    /*!< 厂商自定义信息 */
} sle_adv_data_type;

#define check_rc_return_rc(rc, err)                                                             \
    do {                                                                                         \
        if ((rc) != ERRCODE_SUCC) {                                                              \
            osal_printk("CARKEY ERROR: %s fail!: call %s return 0x%x!\n", err, __FUNCTION__, rc);   \
        }                                                                                        \
    } while (0)

/* ========================================================================== */
/*                         Client（钥匙 carkey）接口                            */
/* ========================================================================== */
int measure_dis_client_write_server(uint32_t type, uint8_t *data, uint32_t data_len);
int measure_dis_client_init(void);
int measure_dis_start_scan(void);
errcode_t measure_dis_client_reg_callbacks(void);

/* ========================================================================== */
/*                         Server（锚点 anchor）接口                            */
/* ========================================================================== */
/* 广播 */
errcode_t measure_dis_sle_set_adv(uint8_t announce_id, uint8_t *addr);
errcode_t measure_dis_stop_adv(uint8_t announce_id);
errcode_t measure_dis_start_adv(uint8_t announce_id);

/* 主入口 */
int measure_dis_server_write_client(uint32_t type, uint8_t *data, uint32_t data_len);
int measure_dis_server_init(void);

/* 消息队列/事件 */
errcode_t sle_measure_dis_msg_add(measure_dis_msg_node_t *msg);
void sle_measure_dis_msg_proc(void);

/* IQ/算法 */
errcode_t measure_dis_server_reg_callbacks(void);
errcode_t measure_dis_match_msg(measure_dis_msg_node_t *msg_node);
errcode_t measure_dis_remote_iq(uint16_t len, uint8_t *value);

/* 服务端全局变量（定义在 .c，alg 模块提前引用） */
extern osal_event measure_dis_evt;
extern uint8_t g_server_conn_id;

slem_smoothed_dis_result* get_caled_result(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
