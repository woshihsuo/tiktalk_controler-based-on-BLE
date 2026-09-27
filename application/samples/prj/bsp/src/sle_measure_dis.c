/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: SLE MEASURE_DIS sample (merged). Client (carkey) and server (anchor)
 *              coexist in one translation unit.
 *
 * History:
 * 2023-04-03, Create client files.
 * 2023-07-17, Create server files.
 */
#include "sle_measure_dis.h"


/* 两个角色共用的编译期常量（原 client/server 各自重复定义） */
#define BLE_SLE_TAG_TASK_DURATION_MS 10
/* UUID只支持2字节和16字节 */
#define SLEM_UUID_LEN SLE_UUID_LEN
#define SLEM_IQ_DATALEN 512

/* ########################################################################## */
/* #                        Client（钥匙 / carkey）                          # */
/* ########################################################################## */

#define ADV_TO_SERVER 2
#define KEY_CONNECT_DELAY_TIME 500

slem_smoothed_dis_result g_dis_result={0};

/* -------------------- 客户端全局变量 -------------------- */
/* IQ 存储（原 sle_slemc.c） */
static measure_dis_stored_iq_data_t g_client_local_iq_data;
static uint8_t g_client_next_idx;
static uint8_t g_client_iq_debug_flag = 0;
/* 客户端主逻辑（原 sle_slemc_client.c） */
uint8_t iq_msg_buff[sizeof(measure_ids_msg_t) + sizeof(measure_dis_stored_iq_data_t)];

static uint8_t g_client_stack_enable_status = 0;

uint8_t g_client_measure_dis_server_addr[SLE_ADDR_LEN] = { 1, 1, 1, 1, 1, 1 };
uint8_t g_client_measure_dis_client_addr[SLE_ADDR_LEN] = { 2, 2, 2, 2, 2, 2 };
uint8_t g_client_conn_id = SLEM_CONNET_INVAILD;

static measure_dis_server_data_t g_client_server_data = {
    .server_uuid = {0x11, 0x22, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    .service_uuid = {0x11, 0x33, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    .property_uuid = {0x11, 0x44, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0},
};

/* -------------------- 客户端 IQ 处理（原 sle_slemc.c） -------------------- */
static void measure_dis_client_print_iq_data(uint8_t num, uint8_t role,
                                             sle_channel_sounding_iq_trans_t *report)
{
    if (!g_client_iq_debug_flag) {
        return;
    }
    osal_printk("[iq_data]%d,%d,%d,%d,0x%x,",
                num, role, report->timestamp_sn, report->samp_cnt, report->rssi);
    sle_channel_sounding_qte_trans_t *data = report->data;
    uint16_t i = 0;
    while (i < POSALG_DATA_NUM) {
        osal_printk("[%d]:0x%x,0x%x", i, data[i].i_data, data[i].q_data);
        i++;
        if (i == POSALG_DATA_NUM) {
            osal_printk(".\r\n");
        } else {
            osal_printk(";");
        }
    }
}

static errcode_t measure_dis_client_store_local_iq(sle_channel_sounding_iq_report_t *report)
{
    if (report->report_idx == 0) {
        g_client_local_iq_data.samp_cnt = 0;
        g_client_local_iq_data.rssi = report->rssi[0];
        g_client_local_iq_data.es_sn = report->es_sn;
        g_client_local_iq_data.timestamp_sn = report->timestamp_sn;
#if (defined(GLE_CS_MODE3_SUPPORT))
        g_client_local_iq_data.tof_result = report->tof_result;
#endif
        g_client_next_idx = 0;
    }
    if ((report->report_idx >= MEASURE_DIS_IQ_REPORT_CNT_MAX ||
         report->timestamp_sn != g_client_local_iq_data.timestamp_sn) ||
        (g_client_next_idx != report->report_idx)) {
        osal_printk("SLEM STORE LOCAL IQ FAIL. report_idx:%d, timestamp:%d, g_next_idx:%d. \r\n",
                    report->report_idx, report->timestamp_sn, g_client_next_idx);
        return ERRCODE_INVALID_PARAM;
    }
    g_client_local_iq_data.samp_cnt += report->samp_cnt;
    uint8_t offset = report->report_idx * SLE_CS_IQ_REPORT_COUNT;
    for (uint8_t i = 0; i < report->samp_cnt; i++) {
        g_client_local_iq_data.data[offset + i].i_data = report->i_data[i];
        g_client_local_iq_data.data[offset + i].q_data = report->q_data[i];
    }
    osal_printk("local iq es:%d, time_stamp:%d group:%d, num:%d \r\n",
                report->es_sn, report->timestamp_sn, report->report_idx, 0);
    g_client_next_idx = report->report_idx + 1;

#if (defined(GLE_CS_MODE3_SUPPORT))
    osal_printk("local tof_result = %d.%03d \r\n", report->tof_result / MEASURE_DIS_NUM_CARRY_1000,
                report->tof_result % MEASURE_DIS_NUM_CARRY_1000);
#endif

    return ERRCODE_SUCC;
}

static errcode_t measure_dis_client_recv_local_iq(uint16_t conn_id, sle_channel_sounding_iq_report_t *report)
{
    osal_printk("enter slem_recv_local_iq conn_id:%d. \r\n", conn_id);
    errcode_t ret = ERRCODE_SUCC;

    ret = measure_dis_client_store_local_iq(report);
    if (ret != ERRCODE_SUCC) {
        return ERRCODE_INVALID_PARAM;
    }

    if (report->report_idx + 1 == MEASURE_DIS_IQ_REPORT_CNT_MAX) {
        /* 如果是钥匙则发送给从锚点 */
        measure_dis_client_print_iq_data(0, 0, (sle_channel_sounding_iq_trans_t *)(&g_client_local_iq_data));
        measure_dis_client_write_server(SLEM_PROFILE_MSG_IQ,
                                        (uint8_t *)&g_client_local_iq_data, sizeof(sle_channel_sounding_iq_trans_t));
    }
    return ret;
}

static void measure_dis_client_read_local_cs_caps_cb(sle_channel_sounding_caps_t *caps, errcode_t status)
{
    UNUSED(caps);
    osal_printk("SLEM READ LOCAL CAPS. status:%d. \r\n", status);
}

static void measure_dis_client_read_remote_cs_caps_cb(uint16_t conn_id, sle_channel_sounding_caps_t *caps,
                                                      errcode_t status)
{
    UNUSED(caps);
    UNUSED(conn_id);
    osal_printk("SLEM READ REMOTE CAPS. status:%d. \r\n", status);
}

// static void measure_dis_client_set_cs_param_cb(uint16_t conn_id, errcode_t status)
// {
//     UNUSED(conn_id);
//     osal_printk("SLEM SET PARAM. status:%d. \r\n", status);
// }

static void measure_dis_client_cs_state_changed_cb(uint8_t slem_status, errcode_t status)
{
    unused(slem_status);
    osal_printk("SLEM STATE CHANGED. status:%d. \r\n", status);
}

static void measure_dis_client_cs_iq_report_cb(uint16_t conn_id, sle_channel_sounding_iq_report_t *report)
{
    if (report->report_idx == 0) {
        osal_printk("RECEIVE LOCAL IQ. timestamp_sn:%d \r\n",  report->timestamp_sn);
    }
    measure_dis_client_recv_local_iq(conn_id, report);
}

errcode_t measure_dis_client_reg_callbacks(void)
{
    sle_hadm_callbacks_t scd_cbks = {0};

    scd_cbks.read_local_cs_caps_cb = measure_dis_client_read_local_cs_caps_cb;
    scd_cbks.read_remote_cs_caps_cb = measure_dis_client_read_remote_cs_caps_cb;
    scd_cbks.cs_state_changed_cb = measure_dis_client_cs_state_changed_cb;
    scd_cbks.cs_iq_report_cb = measure_dis_client_cs_iq_report_cb;

    errcode_t ret = sle_hadm_register_callbacks(&scd_cbks);
    osal_printk("SCD SET CALLBACK DONE. ret = %x.\r\n", ret);
    return ret;
}

/* ----------------- 客户端主逻辑（原 sle_slemc_client.c） ----------------- */
static errcode_t measure_dis_client_slem_set_param(uint16_t conn_id)
{
    sle_set_channel_sounding_param_ex_t param = {
        .acb_interval = 0,
        .con_anchor_num = 1,
        .cs_interval = 1,
        .freq_space = 0,
        .is_cs_param_chg = 0,
        .refresh_rate = 1,
    };
    errcode_t ret = sle_set_channel_sounding_param_ex(conn_id, &param);
    if ((ret != ERRCODE_SUCC) || (conn_id == SLEM_CONNET_INVAILD)) {
        osal_printk("slem set param failed, ret = %d.\n", ret);
    }
    osal_printk("slem set param finish. refresh_rate:%d \n", param.refresh_rate);
    return ERRCODE_SUCC;
}

static errcode_t measure_dis_client_slem_set_enable(uint16_t conn_id)
{
    osal_printk("SLEM SET ENABLE conn_id:%x.\r\n", conn_id);
    errcode_t ret = sle_set_channel_sounding_enable(conn_id);
    /* 解决测距使能之后，马上断链导致没有底层回调 */
    if ((ret != ERRCODE_SUCC) || (conn_id == SLEM_CONNET_INVAILD)) {
        osal_printk("slem set enable failed, ret = %d.\n", ret);
    }
    return ERRCODE_SUCC;
}

static int measure_dis_client_discover_key(uint8_t type)
{
    uint32_t ret = ERRCODE_SLE_FAIL;
    uint8_t conn_id = g_client_conn_id;

    ssapc_find_structure_param_t param = {0};
    param.type = type,
    param.start_hdl = 1,
    param.end_hdl = 0xFFFF,
    param.uuid.len = SLEM_UUID_LEN;
    if (type == SSAP_FIND_TYPE_PRIMARY_SERVICE) {
        if (memcpy_s(param.uuid.uuid, SLEM_UUID_LEN,
            g_client_server_data.service_uuid, SLEM_UUID_LEN) != EOK) {
            return ERRCODE_MEMCPY;
        }
    } else if (type == SSAP_FIND_TYPE_PROPERTY) {
        if (memcpy_s(param.uuid.uuid, SLEM_UUID_LEN,
            g_client_server_data.property_uuid, SLEM_UUID_LEN) != EOK) {
            return ERRCODE_MEMCPY;
        }
    }
    ret = ssapc_find_structure(0, conn_id, &param);
    osal_printk("SLE START CLIENT DISCOVER SERVER. conn_id:%d, ret:%d \r\n", conn_id, ret);

    return ret;
}

static void measure_dis_client_cm_conn_state_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                                  sle_acb_state_t conn_state,
                                                  sle_pair_state_t pair_state,
                                                  sle_disc_reason_t disc_reason)
{
    unused(pair_state);
    unused(addr);
    unused(disc_reason);
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        osal_printk("Connected. conn_id:%d\r\n", conn_id);
        g_client_conn_id = conn_id;
        osal_msleep(MEASURE_DIS_NUM_CARRY_1000);
        measure_dis_client_discover_key(SSAP_FIND_TYPE_PROPERTY);
        ssap_exchange_info_t info = { 0 };
        info.mtu_size = SLEM_IQ_DATALEN;
        info.version = 1;
        ssapc_exchange_info_req(0, conn_id, &info);

        (void)osDelay(KEY_CONNECT_DELAY_TIME);
        measure_dis_client_slem_set_param(conn_id);
        (void)osDelay(KEY_CONNECT_DELAY_TIME);
        measure_dis_client_slem_set_enable(conn_id);
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        osal_printk("Disconnected disc_reason:0x%x .\r\n", disc_reason);
        g_client_conn_id = SLEM_CONNET_INVAILD;
        measure_dis_start_scan();
    }
}

static errcode_t measure_dis_client_cm_register_cbks(void)
{
    sle_connection_callbacks_t cm_cbks = { 0 };
    cm_cbks.connect_state_changed_cb = measure_dis_client_cm_conn_state_cbk;
    cm_cbks.connect_param_update_req_cb = NULL;
    cm_cbks.connect_param_update_cb = NULL;
    cm_cbks.auth_complete_cb = NULL;
    cm_cbks.pair_complete_cb = NULL;
    cm_cbks.read_rssi_cb = NULL;

    return sle_connection_register_callbacks(&cm_cbks);
}

static void measure_dis_client_dd_enable_cbk(uint8_t status)
{
    osal_printk("sle enable result statue:0x%x .\r\n", status);
    g_client_stack_enable_status = 1;
}

static void measure_dis_client_dd_disenable_cbk(uint8_t status)
{
    osal_printk("sle disable result statue:0x%x .\r\n", status);
    g_client_stack_enable_status = 0;
}

static void measure_dis_client_seek_result_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data == NULL) {
        return;
    }

    if ((memcmp(g_client_measure_dis_server_addr, seek_result_data->addr.addr, SLE_ADDR_LEN) == 0)) {
        sle_stop_seek();
        sle_connect_remote_device(&(seek_result_data->addr));
    }
}

static errcode_t measure_dis_client_dm_register_cbks(void)
{
    sle_dev_manager_callbacks_t dm_cbks = {
        .sle_power_on_cb = NULL,
        .sle_enable_cb = measure_dis_client_dd_enable_cbk,
        .sle_disable_cb = measure_dis_client_dd_disenable_cbk,
    };
    return sle_dev_manager_register_callbacks(&dm_cbks);
}

static errcode_t measure_dis_client_dd_register_cbks(void)
{
    sle_announce_seek_callbacks_t dd_cbks  = {
        .announce_enable_cb = NULL,
        .announce_disable_cb = NULL,
        .announce_terminal_cb = NULL,
        .seek_enable_cb = NULL,
        .seek_disable_cb = NULL,
        .seek_result_cb = measure_dis_client_seek_result_cbk,
    };
    return sle_announce_seek_register_callbacks(&dd_cbks);
}

static void measure_dis_client_ssapc_find_structure_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_service_result_t *service, errcode_t status)
{
    osal_printk("[ssap client] find structure cbk client: %d conn_id:%d status: %d \n",
        client_id, conn_id, status);
    osal_printk("[ssap client] find structure cbk start_hdl: %d end_hdl:%d \n",
        service->start_hdl, service->end_hdl);

    g_client_server_data.begin_hdl = service->start_hdl;
    g_client_server_data.end_hdl = service->end_hdl;
}

static void measure_dis_client_ssapc_find_property_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status)
{
    osal_printk("[ssap client] find property cbk client: %d conn_id:%d status: %d \n",
        client_id, conn_id, status);
    osal_printk("[ssap client] find property cbk handle: %d \n", property->handle);

    g_client_server_data.property_handle = property->handle;
}

static void measure_dis_client_ssapc_find_structure_complete_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status)
{
    osal_printk("[ssap client] find structure complete cbk client: %d conn_id:%d status: %d \n",
                client_id, conn_id, status);
    if (structure_result == NULL) {
        return;
    }

    osal_printk("SERVICES DISCOVER COMPLETE. \r\n");
}

static void measure_dis_client_msg_proc(uint8_t *data, uint16_t data_len)
{
    unused(data_len);
    uint32_t ret = ERRCODE_SLE_FAIL;
    measure_ids_msg_t *slem_profile_msg = (measure_ids_msg_t *)(data);

    switch (slem_profile_msg->type) {
        case SLEM_PROFILE_MSG_IQ:
            break;
        default:
            for (int i = 0; i < data_len; i++) {
                osal_printk("[%d]:%d\r\n", i, data[i]);
            }
            break;
    }

    if (unlikely(ret != ERRCODE_SLE_SUCCESS)) {
        osal_printk("client proc msg failed MSG_TYPE:0x%x ret:0x%x \r\n", slem_profile_msg->type, ret);
    }
}

/**
 * @brief  数据通知处理函数
 */
static void measure_dis_client_ssapc_notification_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(status);
    osal_printk("[ssap client] notification info cbk client handle:%d, conn_id:0x%x, data_len:0x%x\n",
        data->handle, conn_id, data->data_len);

    measure_dis_client_msg_proc(data->data, data->data_len);
}

/**
 * @brief  数据指示处理函数
 */
static void measure_dis_client_ssapc_indication_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    osal_printk("[ssap client] indication info cbk client %d,mtu_size:%d, version:0x%x, status:0x%x\n",
        conn_id, data->handle, data->type, status);
}

/**
 * @brief  MTU信息交换响应处理函数
 */
static void measure_dis_client_ssapc_exchange_info_cbk(uint8_t client_id, uint16_t conn_id,
    ssap_exchange_info_t *param, errcode_t status)
{
    unused(client_id);
    osal_printk("[ssap client] exchange info cbk client %d,mtu_size:%d, version:0x%x, status:0x%x\n",
        conn_id, param->mtu_size, param->version, status);
}

static errcode_t measure_dis_client_ssapc_register_cbks(void)
{
    ssapc_callbacks_t ssapc_cbks;

    osal_printk("[ssap client] client register cbk\r\n");
    /* 服务发现模块回调函数 */
    ssapc_cbks.find_structure_cb = measure_dis_client_ssapc_find_structure_cbk;
    ssapc_cbks.ssapc_find_property_cbk = measure_dis_client_ssapc_find_property_cbk;
    ssapc_cbks.find_structure_cmp_cb = measure_dis_client_ssapc_find_structure_complete_cbk;
    ssapc_cbks.read_cfm_cb = NULL;
    ssapc_cbks.read_by_uuid_cmp_cb = NULL;
    ssapc_cbks.write_cfm_cb = NULL;
    ssapc_cbks.exchange_info_cb = measure_dis_client_ssapc_exchange_info_cbk;
    ssapc_cbks.notification_cb = measure_dis_client_ssapc_notification_cbk;
    ssapc_cbks.indication_cb = measure_dis_client_ssapc_indication_cb;
    return ssapc_register_callbacks(&ssapc_cbks);
}

static errcode_t measure_dis_client_set_local_addr(uint8_t *addr)
{
    sle_addr_t sle_addr = {0};
    if (memcpy_s(sle_addr.addr, SLE_ADDR_LEN, addr, SLE_ADDR_LEN) != EOK) {
        return ERRCODE_MEMCPY;
    };
    return sle_set_local_addr(&sle_addr);
}

static errcode_t measure_dis_client_set_local_name(uint8_t *name, uint8_t len)
{
    return sle_set_local_name(name, len);
}

static void measure_dis_client_protocol_stack_init(uint8_t *addr, uint8_t *name)
{
    uint32_t ret = ERRCODE_SLE_SUCCESS;

    ret |= measure_dis_client_dm_register_cbks();
    ret |= measure_dis_client_cm_register_cbks();
    ret |= measure_dis_client_dd_register_cbks();
    ret = enable_sle();
    while (g_client_stack_enable_status != 1) {
        osal_msleep(BLE_SLE_TAG_TASK_DURATION_MS);
    }
    measure_dis_client_set_local_addr(addr);
    measure_dis_client_set_local_name(name, strlen((char *)name));
}

static int measure_dis_client_write_proc(uint8_t type, uint16_t len, uint8_t *data)
{
    errcode_t ret = ERRCODE_FAIL;
    ssapc_write_param_t param = {0};
    uint8_t conn_id = g_client_conn_id;

    param.handle = g_client_server_data.property_handle;
    param.type = type;
    param.data_len = len;
    param.data = data;

    ret = ssapc_write_cmd(0, conn_id, &param);
    if (ret != ERRCODE_SUCC) {
        osal_printk("CLIENT SEND (KEY) FAIL. cid:%d, handle:%d \r\n", conn_id, param.handle);
    }

    return ret;
}

int measure_dis_client_write_server(uint32_t type, uint8_t *data, uint32_t data_len)
{
    uint32_t ret = ERRCODE_SUCC;
    uint16_t len = sizeof(measure_ids_msg_t) + data_len;
    osal_printk("client send to server, len:0x%x.\r\n", len);
    measure_ids_msg_t *slem_msg = (measure_ids_msg_t *)osal_kmalloc(len, 0);
    if (slem_msg == NULL) {
        return ERRCODE_MALLOC;
    }
    slem_msg->type = type;
    slem_msg->len = data_len;
    if (memcpy_s(slem_msg->data, data_len, data, data_len) != EOK) {
        osal_kfree(slem_msg);
        return ERRCODE_MEMCPY;
    }
    ret = measure_dis_client_write_proc(0, len, (uint8_t *)(slem_msg));
    if (ret != ERRCODE_SUCC) {
        osal_printk("client send to server failed.\n");
    }
    osal_kfree(slem_msg);

    return ret;
}

static errcode_t measure_dis_client_set_scan(void)
{
    errcode_t ret;
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 1;
    param.seek_filter_policy = 1;
    param.seek_phys = 1;
    param.seek_type[0] = 1;
    param.seek_interval[0] = 0x12c0;
    param.seek_window[0] = 0x500;
    ret = sle_set_seek_param(&param);

    return ret;
}

int measure_dis_start_scan(void)
{
    errcode_t ret;
    ret = measure_dis_client_set_scan();
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ret;
    }
    ret = sle_start_seek();
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

int measure_dis_client_init(void)
{
    uint32_t ret = ERRCODE_SLE_SUCCESS;
    uint8_t measure_dis_name[SLE_NAME_MAX_LEN] = {'s', 'l', 'e', 'm', '-', 'c', '\0'};
    osal_printk("START CLIENT INIT .\r\n");
    measure_dis_client_protocol_stack_init(g_client_measure_dis_client_addr, measure_dis_name);
    measure_dis_client_reg_callbacks();
    ret = measure_dis_client_ssapc_register_cbks();
    measure_dis_start_scan();
    return ret;
}
/* 钥匙 */

/* ########################################################################## */
/* #                        Server（锚点 / anchor）                         # */
/* ########################################################################## */

/* -------------------- 广播配置（原 sle_slems_adv.c） -------------------- */
typedef struct {
    uint8_t length;
    uint8_t type;
    uint8_t value;
} sle_adv_common_value;

errcode_t measure_dis_stop_adv(uint8_t announce_id)
{
    return sle_stop_announce(announce_id);
}

errcode_t measure_dis_start_adv(uint8_t announce_id)
{
    return sle_start_announce(announce_id);
}

static uint16_t sle_set_adv_local_name(uint8_t *adv_data, uint16_t max_len)
{
    uint8_t index = 0;
    uint8_t local_name[SLE_NAME_MAX_LEN];
    uint8_t local_name_len;

    sle_get_local_name(local_name, &local_name_len);

    adv_data[index++] = local_name_len + 1;
    adv_data[index++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;
    if (memcpy_s(&adv_data[index], max_len - index, local_name, local_name_len) != EOK) {
        return 0xFFFF;
    }

    return (uint16_t)index + local_name_len;
}

static uint16_t sle_set_adv_data(uint8_t *adv_data, uint8_t adv_data_len)
{
    size_t len = 0;
    uint16_t idx = 0;

    len = sizeof(sle_adv_common_value);
    sle_adv_common_value adv_disc_level = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL,
        .value = SLE_ANNOUNCE_LEVEL_NORMAL,
    };
    if (memcpy_s(&adv_data[idx], adv_data_len - idx, &adv_disc_level, len) != EOK) {
        return 0xFFFF;
    }
    idx += len;

    len = sizeof(sle_adv_common_value);
    sle_adv_common_value adv_access_mode = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE,
        .value = 0,
    };
    if (memcpy_s(&adv_data[idx], adv_data_len - idx, &adv_access_mode, len) != EOK) {
        return 0xFFFF;
    }
    idx += len;
    return idx;
}

static uint16_t sle_set_scan_response_data(uint8_t *scan_rsp_data, uint8_t adv_data_len)
{
    uint16_t idx = 0;
    size_t scan_rsp_data_len = sizeof(sle_adv_common_value);

    sle_adv_common_value tx_power_level = {
        .length = scan_rsp_data_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL,
        .value = SLE_ADV_TX_POWER,
    };
    if (memcpy_s(scan_rsp_data, adv_data_len, &tx_power_level, scan_rsp_data_len) != EOK) {
        return 0xFFFF;
    }
    idx += scan_rsp_data_len;

    /* set local name */
    idx += sle_set_adv_local_name(&scan_rsp_data[idx], adv_data_len - idx);
    return idx;
}

static errcode_t measure_dis_set_default_announce_param(uint8_t announce_id, sle_addr_t *sle_addr)
{
    sle_announce_param_t param = {0};
    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_handle = announce_id;
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_NO_NEGO;
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map = SLE_ADV_CHANNEL_MAP_DEFAULT;
    param.announce_interval_min = SLE_ADV_INTERVAL_SLAVE_DEFAULT;
    param.announce_interval_max = SLE_ADV_INTERVAL_SLAVE_DEFAULT;
    param.conn_interval_min = 0x64;
    param.conn_interval_max = 0x64;
    param.conn_max_latency = 0;
    param.conn_supervision_timeout = 0x1F4;
    param.own_addr = *sle_addr;

    return sle_set_announce_param(param.announce_handle, &param);
}

static errcode_t measure_dis_set_default_announce_data(uint8_t announce_id)
{
    uint8_t announce_data_len = 0;
    uint8_t seek_data_len = 0;
    sle_announce_data_t data = {0};
    uint8_t adv_handle = announce_id;
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};

    announce_data_len = sle_set_adv_data(announce_data, SLE_ADV_DATA_LEN_MAX);
    data.announce_data = announce_data;
    data.announce_data_len = announce_data_len;

    seek_data_len = sle_set_scan_response_data(seek_rsp_data, SLE_ADV_DATA_LEN_MAX);
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = seek_data_len;

    sle_set_announce_data(adv_handle, &data);

    return ERRCODE_SLE_SUCCESS;
}

errcode_t measure_dis_sle_set_adv(uint8_t announce_id, uint8_t *addr)
{
    sle_addr_t sle_addr = {0};
    if (memcpy_s(sle_addr.addr, SLE_ADDR_LEN, addr, SLE_ADDR_LEN) != EOK) {
        return ERRCODE_MEMCPY;
    }

    measure_dis_set_default_announce_param(announce_id, &sle_addr);
    measure_dis_set_default_announce_data(announce_id);

    return ERRCODE_SLE_SUCCESS;
}

/* -------------------- IQ 与测距算法（原 sle_slems_alg.c） -------------------- */
#define TOF_DEFAULT 2070
#define DIS_ALG_MODE 4
#define DIS_ALG_RSSI_LIMIT (-120)
#define DIS_ALG_THRESHOULD_COND2 20
#define DIS_ALG_R_START 2

static uint8_t g_server_iq_debug_flag = 0;
static uint32_t counter = 0;

static measure_dis_stored_iq_data_t g_md_local_iq_data;
static uint8_t g_md_store_local_iq_complete;
static uint8_t g_server_next_idx;

void slem_posalg_set_base_para(slem_alg_para_dis *alg_para, uint8_t key_id)
{
    unused(key_id);
    alg_para->calib_val = 0;
    alg_para->tof_calib = TOF_DEFAULT;
    alg_para->ranging_method = DIS_ALG_MODE;
}

static void measure_dis_print_cal_dis(float dist_first, float dist_ori, float prob, uint32_t time)
{
    unused(dist_ori);
    unused(prob);
    osal_printk(
        "SLEM get distance done. distance:%d.%03d, time = %d ms. \r\n",
        (int32_t)(dist_first), ((uint32_t)(dist_first * MEASURE_DIS_NUM_CARRY_1000) % MEASURE_DIS_NUM_CARRY_1000),
        (uint32_t)(time));
}

slem_smoothed_dis_result* get_caled_result(void)
{
    return &g_dis_result;
}

static void measure_dis_posalg_set_distance(slem_smoothed_dis_result *dis,
    measure_dis_profile_msg_dis_t *measure_dis_temp)
{
    measure_dis_temp->dist_first = dis->dis_smoothed;
    measure_dis_temp->dist_second = dis->dis_ori;
    measure_dis_temp->dist_double = dis->dis_slight_smoothed;
    measure_dis_temp->high = dis->height;
    measure_dis_temp->prob = dis->prob;
    measure_dis_temp->rssi = dis->rssi;
    measure_dis_temp->smooth_num = dis->smooth_num;
}

void measure_dis_posalg_set_base_para(slem_alg_para_dis *alg_para)
{
    alg_para->calib_val = 0;
    alg_para->tof_calib = TOF_DEFAULT;
    alg_para->ranging_method = DIS_ALG_MODE;
}

void measure_dis_posalg_get_distance(measure_dis_profile_msg_dis_t *measure_dis_temp,
                                     sle_channel_sounding_iq_trans_t *remote_iq_data)
{
    slem_alg_para_dis *alg_para = (slem_alg_para_dis *)osal_kmalloc(sizeof(slem_alg_para_dis), 0);

    slem_posalg_set_base_para(alg_para, 0);
    alg_para->rssi_rtd = g_md_local_iq_data.rssi;
    alg_para->rssi_dut = remote_iq_data[0].rssi;
#if (defined(GLE_CS_MODE3_SUPPORT))
    alg_para->tof_rtd = g_md_local_iq_data.tof_result;
    alg_para->tof_dut = remote_iq_data[0].tof_result;
#endif
    alg_para->iq_rtd = (slem_alg_iq *)&(g_md_local_iq_data.data[0]);
    alg_para->iq_dut = (slem_alg_iq *)&(remote_iq_data[0].data[0]);
    g_md_store_local_iq_complete = false;

    slem_para_pair para_limit = {.rssi_limit = DIS_ALG_RSSI_LIMIT,
                                 .threshold_cond2 = DIS_ALG_THRESHOULD_COND2,
                                 .r_start = DIS_ALG_R_START};
    alg_para->para_limit = para_limit;
    alg_para->key_id = 0;
    /* remote iq 是 dut， local iq 是 rtd */
    uint32_t time_start = (uint32_t)uapi_tcxo_get_ms();

    alg_para->cur_count = counter;
    alg_para->cur_time = time_start;
    
    
    slem_alg_calc_smoothed_dis(&g_dis_result, alg_para);
    counter++;
    
    unused(measure_dis_print_cal_dis);
    // uint32_t time_end = (uint32_t)uapi_tcxo_get_ms();
    // measure_dis_print_cal_dis(g_dis_result.dis_smoothed, g_dis_result.dis_ori,
    //                           g_dis_result.prob, time_end - time_start);

    measure_dis_posalg_set_distance(&g_dis_result, measure_dis_temp);
    osal_kfree(alg_para);
}

static void measure_dis_server_print_iq_data(uint8_t num, uint8_t role,
                                             sle_channel_sounding_iq_trans_t *report)
{
    if (!g_server_iq_debug_flag) {
        return;
    }
    osal_printk("[iq_data]%d,%d,%d,%d,0x%x,",
                num, role, report->timestamp_sn, report->samp_cnt, report->rssi);
    sle_channel_sounding_qte_trans_t *data = report->data;
    uint16_t i = 0;
    while (i < POSALG_DATA_NUM) {
        osal_printk("[%d]:0x%x,0x%x", i, data[i].i_data, data[i].q_data);
        i++;
        if (i == POSALG_DATA_NUM) {
            osal_printk(".\r\n");
        } else {
            osal_printk(";");
        }
    }
}

static errcode_t measure_dis_server_store_local_iq(sle_channel_sounding_iq_report_t *report)
{
    if (report->report_idx == 0) {
        g_md_local_iq_data.samp_cnt = 0;
        g_md_local_iq_data.rssi = report->rssi[0];
        g_md_local_iq_data.es_sn = report->es_sn;
        g_md_local_iq_data.timestamp_sn = report->timestamp_sn;
#if (defined(GLE_CS_MODE3_SUPPORT))
        g_md_local_iq_data.tof_result = report->tof_result;
#endif
        g_server_next_idx = 0;
    }
    if ((report->report_idx >= MEASURE_DIS_IQ_REPORT_CNT_MAX ||
         report->timestamp_sn != g_md_local_iq_data.timestamp_sn) ||
        (g_server_next_idx != report->report_idx)) {
        osal_printk("MEASURE_DIS STORE LOCAL IQ FAIL. report_idx:%d, timestamp:%d, g_next_idx:%d. \r\n",
                    report->report_idx, report->timestamp_sn, g_server_next_idx);
        return ERRCODE_INVALID_PARAM;
    }
    g_md_local_iq_data.samp_cnt += report->samp_cnt;
    uint8_t offset = report->report_idx * SLE_CS_IQ_REPORT_COUNT;
    for (uint8_t i = 0; i < report->samp_cnt; i++) {
        g_md_local_iq_data.data[offset + i].i_data = report->i_data[i];
        g_md_local_iq_data.data[offset + i].q_data = report->q_data[i];
    }
    g_server_next_idx = report->report_idx + 1;

#if (defined(GLE_CS_MODE3_SUPPORT))
    osal_printk("local tof_result = %d.%03d \r\n", report->tof_result / MEASURE_DIS_NUM_CARRY_1000,
                report->tof_result % MEASURE_DIS_NUM_CARRY_1000);
#endif

    return ERRCODE_SUCC;
}

static errcode_t measure_dis_proc_local_iq(uint16_t conn_id, sle_channel_sounding_iq_report_t *report)
{
    unused(conn_id);
    if (report->report_idx == 0) {
        osal_printk("RECEIVE LOCAL IQ. timestamp_sn:%d \r\n",  report->timestamp_sn);
    }
    errcode_t ret = ERRCODE_SUCC;

    ret = measure_dis_server_store_local_iq(report);
    if (ret != ERRCODE_SUCC) {
        return ERRCODE_INVALID_PARAM;
    }

    if (report->report_idx + 1 == MEASURE_DIS_IQ_REPORT_CNT_MAX) {
        g_md_store_local_iq_complete = true;
        osal_printk("store local iq data commplete . \r\n");
        /* 如果是钥匙则发送给从锚点 */
        measure_dis_server_print_iq_data(0, 0, (sle_channel_sounding_iq_trans_t *)(&g_md_local_iq_data));
    }
    return ret;
}

static errcode_t measure_dis_proc_remote_iq(uint16_t conn_id, sle_channel_sounding_iq_trans_t *report)
{
    unused(conn_id);
    uint8_t num = 0; /* SLAVE只有一路MEASURE_DIS业务 */
    measure_dis_server_print_iq_data(0, 1, report);
    if (abs((int32_t)(g_md_local_iq_data.timestamp_sn - report->timestamp_sn)) >= MEASURE_DIS_TIMESTAMP_DIFF_MAX) {
        osal_printk("local:%d, remote:%d. \r\n",
                    g_md_local_iq_data.timestamp_sn, report->timestamp_sn);
        return ERRCODE_INVALID_PARAM;
    }

    osal_printk("local ts = %d, remote ts = %d.  num[%d], local commplete status:%d.\r\n",
                g_md_local_iq_data.timestamp_sn, report->timestamp_sn, num, g_md_store_local_iq_complete);

    if (g_md_store_local_iq_complete) {
        g_md_store_local_iq_complete = false;
        measure_dis_profile_msg_dis_t measure_dis_dis_temp = { report->timestamp_sn, 0, 0, 0, 0, 0, 0, 0};
        measure_dis_posalg_get_distance(&measure_dis_dis_temp, (sle_channel_sounding_iq_trans_t *)report);
    }

    return ERRCODE_SUCC;
}

static void measure_dis_server_read_local_cs_caps_cb(sle_channel_sounding_caps_t *caps, errcode_t status)
{
    unused(caps);
    osal_printk("SLEM READ LOCAL CAPS. status:%d. \r\n", status);
}

static void measure_dis_server_read_remote_cs_caps_cb(uint16_t conn_id, sle_channel_sounding_caps_t *caps,
                                                      errcode_t status)
{
    unused(caps);
    unused(conn_id);
    osal_printk("SLEM READ REMOTE CAPS. status:%d. \r\n", status);
}

// static void measure_dis_server_set_cs_param_cb(uint16_t conn_id, errcode_t status)
// {
//     unused(conn_id);
//     osal_printk("SLEM SET PARAM. status:%d. \r\n", status);
// }

static void measure_dis_server_cs_state_changed_cb(uint8_t slem_status, errcode_t status)
{
    unused(slem_status);
    osal_printk("SLEM STATE CHANGED. status:%d. \r\n", status);
}

static void measure_dis_server_cs_iq_report_cb(uint16_t conn_id, sle_channel_sounding_iq_report_t *report)
{
    errcode_t ret = ERRCODE_FAIL;

    sle_channel_sounding_iq_report_t *report_local = NULL;
    uint32_t len = sizeof(sle_channel_sounding_iq_report_t);
    report_local = (sle_channel_sounding_iq_report_t *)osal_kmalloc(len, 0);
    if (report_local == NULL) {
        return;
    }
    if (memcpy_s(report_local, sizeof(sle_channel_sounding_iq_report_t), report, len) != EOK) {
        osal_kfree(report_local);
        return;
    }
    measure_dis_msg_node_t msg_node = {conn_id, SLEM_MSG_LOCAL_IQ, 0, report_local};
    ret = sle_measure_dis_msg_add(&msg_node);
    if (ret != ERRCODE_SUCC) {
        osal_kfree(report_local);
    }
}

errcode_t measure_dis_server_reg_callbacks(void)
{
    sle_hadm_callbacks_t scd_cbks = {0};

    scd_cbks.read_local_cs_caps_cb = measure_dis_server_read_local_cs_caps_cb;
    scd_cbks.read_remote_cs_caps_cb = measure_dis_server_read_remote_cs_caps_cb;
    scd_cbks.cs_state_changed_cb = measure_dis_server_cs_state_changed_cb;
    scd_cbks.cs_iq_report_cb = measure_dis_server_cs_iq_report_cb;

    errcode_t ret = sle_hadm_register_callbacks(&scd_cbks);
    osal_printk("SCD SET CALLBACK DONE. ret = %x.\r\n", ret);
    return ret;
}

static errcode_t measure_dis_server_recv_local_iq(measure_dis_msg_node_t *msg_node)
{
    /* key send to anchor */
    errcode_t ret = measure_dis_proc_local_iq(msg_node->conn_id,
                                              (sle_channel_sounding_iq_report_t *)(msg_node->data));
    osal_kfree(msg_node->data);
    osal_event_write(&measure_dis_evt, MEASURE_DIS_MSG_EVENT);
    return ret;
}

static errcode_t measure_dis_recv_remote_iq(measure_dis_msg_node_t *msg_node)
{
    osal_printk("slem recv remote iq.\n");
    errcode_t ret = measure_dis_proc_remote_iq(msg_node->conn_id,
                                               (sle_channel_sounding_iq_trans_t *)(msg_node->data));
    osal_kfree(msg_node->data);
    osal_event_write(&measure_dis_evt, MEASURE_DIS_MSG_EVENT);
    return ret;
}

errcode_t measure_dis_match_msg(measure_dis_msg_node_t *msg_node)
{
    errcode_t ret = ERRCODE_FAIL;
    osal_printk("---enter sle_slem_msg_proc, type:%d---\n", msg_node->type);
    switch (msg_node->type) {
        case SLEM_MSG_LOCAL_IQ:
            ret = measure_dis_server_recv_local_iq(msg_node);
            break;
        case SLEM_MSG_REMOTE_IQ:
            ret = measure_dis_recv_remote_iq(msg_node);
            break;
        default:
            break;
    }
    return ret;
}

errcode_t measure_dis_remote_iq(uint16_t len, uint8_t *value)
{
    UNUSED(len);
    errcode_t ret = ERRCODE_FAIL;
    sle_channel_sounding_iq_trans_t *report = NULL;
    report = (sle_channel_sounding_iq_trans_t *)osal_kmalloc(sizeof(sle_channel_sounding_iq_trans_t), 0);
    if (report == NULL) {
        return ERRCODE_MALLOC;
    }
    if (memcpy_s(report, sizeof(sle_channel_sounding_iq_trans_t), value, len) != EOK) {
        osal_kfree(report);
        return ERRCODE_MEMCPY;
    }
    osal_printk("RECEIVE REMOTE IQ. timestamp_sn:%d \r\n", report->timestamp_sn);
    measure_dis_msg_node_t msg_node = {g_server_conn_id, SLEM_MSG_REMOTE_IQ, 0, report};
    ret = sle_measure_dis_msg_add(&msg_node);
    if (ret != ERRCODE_SUCC) {
        osal_kfree(report);
    }

    return ret;
}

/* -------------------- 服务端主逻辑（原 sle_slems_server.c） -------------------- */
#define DATA_LEN 2
#define ADV_TO_CLIENT 1

static uint8_t g_server_stack_enable_status = 0;

/* sle server handle */
static uint8_t g_server_id = 0;
/* sle service handle */
static uint16_t g_service_handle = 0;
/* sle ntf property handle */
static uint16_t g_property_handle = 0;

uint8_t g_server_measure_dis_server_addr[SLE_ADDR_LEN] = { 1, 1, 1, 1, 1, 1 };
uint8_t g_server_measure_dis_client_addr[SLE_ADDR_LEN] = { 2, 2, 2, 2, 2, 2 };
uint8_t g_server_conn_id = SLEM_CONNET_INVAILD;

osal_event measure_dis_evt;
static unsigned long g_measure_dis_queue;
static measure_dis_msg_node_t g_msg_data;

static measure_dis_server_data_t g_server_server_data = {
    .server_uuid = {0x11, 0x22, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    .service_uuid = {0x11, 0x33, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    .property_uuid = {0x11, 0x44, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0},
};

static void measure_dis_server_cm_conn_state_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                                  sle_acb_state_t conn_state,
                                                  sle_pair_state_t pair_state,
                                                  sle_disc_reason_t disc_reason)
{
    unused(pair_state);
    unused(addr);
    unused(disc_reason);
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        osal_printk("Connected. conn_id:%d\r\n", conn_id);
        g_server_conn_id = conn_id;
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        osal_printk("Disconnected disc_reason:0x%x .\r\n", disc_reason);
        g_server_conn_id = SLEM_CONNET_INVAILD;
        measure_dis_start_adv(ADV_TO_CLIENT);
    }
}

static errcode_t measure_dis_server_cm_register_cbks(void)
{
    sle_connection_callbacks_t cm_cbks = {0};
    cm_cbks.connect_state_changed_cb = measure_dis_server_cm_conn_state_cbk;
    cm_cbks.connect_param_update_req_cb = NULL;
    cm_cbks.connect_param_update_cb = NULL;
    cm_cbks.auth_complete_cb = NULL;
    cm_cbks.pair_complete_cb = NULL;
    cm_cbks.read_rssi_cb = NULL;

    return sle_connection_register_callbacks(&cm_cbks);
}

static void measure_dis_server_dd_enable_cbk(uint8_t status)
{
    osal_printk("sle enable result statue:0x%x .\r\n", status);
    g_server_stack_enable_status = 1;
}

static void measure_dis_server_dd_disenable_cbk(uint8_t status)
{
    osal_printk("sle disable result statue:0x%x .\r\n", status);
    g_server_stack_enable_status = 0;
}

static void measure_dis_dd_announce_enable_cbk(uint32_t discovery_id, errcode_t status)
{
    osal_printk("discovery_enable_callback :discover_id:0x%x, status:0x%x \r\n",
                discovery_id, status);
}

static void measure_dis_server_seek_result_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data == NULL) {
        return;
    }

    if ((memcmp(g_server_measure_dis_server_addr, seek_result_data->addr.addr, SLE_ADDR_LEN) == 0)) {
        sle_stop_seek();
        sle_connect_remote_device(&(seek_result_data->addr));
    }
}

static errcode_t measure_dis_server_dm_register_cbks(void)
{
    sle_dev_manager_callbacks_t dm_cbks  = {
        .sle_enable_cb = measure_dis_server_dd_enable_cbk,
        .sle_disable_cb = measure_dis_server_dd_disenable_cbk,
    };
    return sle_dev_manager_register_callbacks(&dm_cbks);
}

static errcode_t measure_dis_server_dd_register_cbks(void)
{
    sle_announce_seek_callbacks_t dd_cbks  = {
        .announce_enable_cb = measure_dis_dd_announce_enable_cbk,
        .announce_disable_cb = NULL,
        .announce_terminal_cb = NULL,
        .seek_enable_cb = NULL,
        .seek_disable_cb = NULL,
        .seek_result_cb = measure_dis_server_seek_result_cbk,
    };
    return sle_announce_seek_register_callbacks(&dd_cbks);
}

static void measure_dis_ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id,
    ssaps_req_read_cb_t *read_cb_para, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(read_cb_para);
    unused(status);
}

static void measure_dis_server_msg_proc(uint8_t *data, uint16_t data_len)
{
    unused(data_len);
    uint32_t ret = ERRCODE_SLE_FAIL;
    measure_ids_msg_t *slem_profile_msg = (measure_ids_msg_t *)(data);

    switch (slem_profile_msg->type) {
        case SLEM_PROFILE_MSG_IQ:
            osal_printk("enter handle recv remote iq\r\n");
            ret = measure_dis_remote_iq(slem_profile_msg->len, slem_profile_msg->data);
            break;
        default:
            for (int i = 0; i < data_len; i++) {
                osal_printk("[%d]:%d\r\n", i, data[i]);
            }
            break;
    }

    if (unlikely(ret != ERRCODE_SLE_SUCCESS)) {
        osal_printk("client proc msg failed MSG_TYPE:0x%x ret:0x%x \r\n", slem_profile_msg->type, ret);
    }
}

static void measure_dis_ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id,
    ssaps_req_write_cb_t *write_cb_para, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(status);
    osal_printk("server recv msg length:0x%x status:0x%x \r\n", write_cb_para->length, status);
    measure_dis_server_msg_proc(write_cb_para->value, write_cb_para->length);
}

static void measure_dis_ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id,
    ssap_exchange_info_t *mtu_size, errcode_t status)
{
    osal_printk("[scd server] ssaps myu change cbk server_id:0x%x, conn_id:0x%x, mtu_size:0x%x, status:0x%x\r\n",
                server_id, conn_id, mtu_size->mtu_size, status);
}

static void measure_dis_ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    osal_printk("[scd server] start service cbk server_id:0x%x, handle:0x%x, status:0x%x\r\n",
                server_id, handle, status);
}

static void measure_dis_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.start_service_cb = measure_dis_ssaps_start_service_cbk;
    ssaps_cbk.mtu_changed_cb = measure_dis_ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = measure_dis_ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = measure_dis_ssaps_write_request_cbk;
    ssaps_register_callbacks(&ssaps_cbk);
}

static errcode_t measure_dis_server_service_add(void)
{
    errcode_t ret;
    sle_uuid_t service_uuid = {0};
    service_uuid.len = SLEM_UUID_LEN;
    if (memcpy_s(service_uuid.uuid, SLE_UUID_LEN, g_server_server_data.service_uuid, SLEM_UUID_LEN) != EOK) {
        return ERRCODE_MEMCPY;
    }
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t measure_dis_server_property_add(void)
{
    errcode_t ret;
    uint8_t property_data[DATA_LEN] = {11, 11};
    uint8_t descriptor_data[DATA_LEN] = {0x01, 0x00};
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};

    property.uuid.len = SLEM_UUID_LEN;
    if (memcpy_s(property.uuid.uuid, SLE_UUID_LEN, g_server_server_data.property_uuid, SLEM_UUID_LEN) != EOK) {
        return ERRCODE_MEMCPY;
    }
    property.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    property.value = property_data;
    property.value_len = DATA_LEN;

    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property,  &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ERRCODE_SLE_FAIL;
    }
    descriptor.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    descriptor.value = descriptor_data;
    descriptor.value_len = DATA_LEN;
    descriptor.type = SSAP_DESCRIPTOR_CLIENT_CONFIGURATION;

    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ERRCODE_SLE_FAIL;
    }

    return ERRCODE_SLE_SUCCESS;
}

static int measure_dis_server_set_mtu(uint16_t mtu_size, uint16_t version)
{
    ssap_exchange_info_t info = {
        .mtu_size = mtu_size,
        .version = version
    };

    return ssaps_set_info(g_server_id, &info);
}

static int measure_dis_server_add(void)
{
    errcode_t ret;
    sle_uuid_t app_uuid = {0};

    measure_dis_server_set_mtu(SLEM_IQ_DATALEN, 1);  /* 需要在连接建立之前 */
    measure_dis_ssaps_register_cbks();
    osal_printk("[server] sle uuid add service in\r\n");
    app_uuid.len = SLEM_UUID_LEN;
    if (memcpy_s(app_uuid.uuid, SLE_UUID_LEN, g_server_server_data.server_uuid, SLEM_UUID_LEN) != EOK) {
        return ERRCODE_SLE_FAIL;
    }
    ssaps_register_server(&app_uuid, &g_server_id);

    if (measure_dis_server_service_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }

    if (measure_dis_server_property_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    osal_printk("[server] server_id:%x, service_handle:%x, property_handle:%x\r\n",
        g_server_id, g_service_handle, g_property_handle);
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ERRCODE_SLE_FAIL;
    }
    osal_printk("[server] sle uuid add service out\r\n");
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t measure_dis_server_set_local_addr(uint8_t *addr)
{
    sle_addr_t sle_addr = {0};
    if (memcpy_s(sle_addr.addr, SLE_ADDR_LEN, addr, SLE_ADDR_LEN) != EOK) {
        return ERRCODE_MEMCPY;
    };
    return sle_set_local_addr(&sle_addr);
}

static errcode_t measure_dis_server_set_local_name(uint8_t *name, uint8_t len)
{
    return sle_set_local_name(name, len);
}

static void measure_dis_server_protocol_stack_init(uint8_t *addr, uint8_t *name)
{
    uint32_t ret = ERRCODE_SLE_SUCCESS;

    ret |= measure_dis_server_dm_register_cbks();
    ret |= measure_dis_server_cm_register_cbks();
    ret |= measure_dis_server_dd_register_cbks();
    ret = enable_sle();
    while (g_server_stack_enable_status != 1) {
        osal_msleep(BLE_SLE_TAG_TASK_DURATION_MS);
    }
    measure_dis_server_set_local_addr(addr);
    measure_dis_server_set_local_name(name, strlen((char *)name));
}

static int measure_dis_sle_server_ntf_by_addr(uint16_t len, uint8_t *data)
{
    /* type must be 0 */
    errcode_t ret = ERRCODE_FAIL;
    ssaps_ntf_ind_by_uuid_t param = {
        .uuid.len = SLEM_UUID_LEN,
        .start_handle = g_service_handle,
        .end_handle = g_property_handle,
        .type = 0,
        .value_len = len,
        .value = data,
    };
    if (memcpy_s(param.uuid.uuid, SLE_UUID_LEN, g_server_server_data.property_uuid, SLEM_UUID_LEN) != EOK) {
        return ERRCODE_MEMCPY;
    }
    uint8_t conn_id = g_server_conn_id;
    ret = ssaps_notify_indicate_by_uuid(g_server_id, conn_id, &param);

    return ret;
}

int measure_dis_server_write_client(uint32_t type, uint8_t *data, uint32_t data_len)
{
    uint16_t len = sizeof(measure_ids_msg_t) + data_len;
    measure_ids_msg_t *slem_msg = (measure_ids_msg_t *)osal_kmalloc(len, 0);
    if (slem_msg == NULL) {
        return ERRCODE_MALLOC;
    }
    slem_msg->type = type;
    slem_msg->len = data_len;
    if (memcpy_s(slem_msg->data, data_len, data, data_len) != EOK) {
        osal_kfree(slem_msg);
        return ERRCODE_MEMCPY;
    }
    uint32_t ret = measure_dis_sle_server_ntf_by_addr(len, (uint8_t *)(slem_msg));
    if (ret != ERRCODE_SUCC) {
        osal_printk("server send to client failed.\n");
    }
    osal_kfree(slem_msg);

    return ret;
}

void sle_measure_dis_msg_proc(void)
{
    measure_dis_msg_node_t *msg_node = NULL;
    uint32_t msg_data_size = sizeof(measure_dis_msg_node_t);
    errcode_t ret = ERRCODE_SUCC;
    ret = osal_msg_queue_read_copy(g_measure_dis_queue, &g_msg_data, &msg_data_size, 0);
    if (ret != ERRCODE_SUCC) {
        osal_printk("not find msg node \r\n");
        return;
    }
    msg_node = &g_msg_data;
    ret = measure_dis_match_msg(msg_node);
    if (ret != ERRCODE_SUCC) {
        osal_printk("MSG PROC ERROR type:%d \r\n", msg_node->type);
    }
}

errcode_t sle_measure_dis_msg_add(measure_dis_msg_node_t *msg)
{
    errcode_t ret = ERRCODE_FAIL;

    do {
        if ((msg == NULL) || (g_measure_dis_queue == 0)) {
            break;
        }
        if (osal_msg_queue_write_copy(g_measure_dis_queue,
                                      (void *)msg, sizeof(measure_dis_msg_node_t), 0) != OSAL_SUCCESS) {
            ret = ERRCODE_FAIL;
            break;
        }
        if (osal_event_write(&measure_dis_evt, MEASURE_DIS_MSG_EVENT) != OSAL_SUCCESS) {
            ret = ERRCODE_FAIL;
            break;
        }
        ret = ERRCODE_SUCC;
    } while (0);

    if (ret != ERRCODE_SUCC) {
        uint8_t msg_num = osal_msg_queue_get_msg_num(g_measure_dis_queue);
        osal_printk("MSG ADD FAIL. msg_num:%d ret:%d. \r\n", msg_num, ret);
    }

    return ret;
}


static void sle_measure_dis_init(void)
{
    if (osal_event_init(&measure_dis_evt) != ERRCODE_SUCC) {
        osal_printk("touch osal_event_init fail! \r\n");
        return;
    }

    if (osal_msg_queue_create("measure_dis_queue", MEASURE_DIS_MSG_QUEUE_SIZE,
                              &g_measure_dis_queue, 0, sizeof(measure_dis_msg_node_t)) != ERRCODE_SUCC) {
        osal_printk("osal_queue_init init failed \r\n");
        return;
    }
}

int measure_dis_server_init(void)
{
    uint32_t ret = ERRCODE_SLE_SUCCESS;
    uint8_t measure_dis_name[SLE_NAME_MAX_LEN] = {'s', 'l', 'e', 'm', '-', 's', '\0'};
    sle_measure_dis_init();
    osal_printk("START SERVER INIT .\r\n");
    measure_dis_server_protocol_stack_init(g_server_measure_dis_server_addr, measure_dis_name);
    measure_dis_server_reg_callbacks();
    ret = measure_dis_server_add();
    check_rc_return_rc(ret, "server add");
    measure_dis_sle_set_adv(ADV_TO_CLIENT, g_server_measure_dis_server_addr);
    measure_dis_start_adv(ADV_TO_CLIENT);
    return ret;
}
/* 锚点 */

/* 公共回调（两侧实现完全相同，只保留一份） */
void carkey_sle_rssi_report_cbk(uint16_t conn_id, int8_t rssi)
{
    osal_printk("SLE RSSI REPORT conn_id:%d, rssi:%d .\r\n", conn_id, rssi);
}
