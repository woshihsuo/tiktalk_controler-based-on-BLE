/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2018-2019. All rights reserved.
 * Description:   LOG OAM LOGGER MODULE
 */
#include "log_oam_logger.h"

// btc无头日志接口
void log_event_bt_print0(uint32_t presspara)
{
    log_event_print0(log_head_press(OM_BT), presspara);
}

void log_event_bt_print1(uint32_t presspara, uint32_t para1)
{
    log_event_print1(log_head_press(OM_BT), presspara, para1);
}

void log_event_bt_print2(uint32_t presspara, uint32_t para1, uint32_t para2)
{
    log_event_print2(log_head_press(OM_BT), presspara, para1, para2);
}

void log_event_bt_print3(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3)
{
    log_event_print3(log_head_press(OM_BT), presspara, para1, para2, para3);
}

void log_event_bt_print4(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4)
{
    log_event_print4(log_head_press(OM_BT), presspara, para1, para2, para3, para4);
}

// bth无头日志接口
void log_event_bth_print0(uint32_t presspara)
{
    log_event_print0(log_head_press(OM_BTH), presspara);
}

void log_event_bth_print1(uint32_t presspara, uint32_t para1)
{
    log_event_print1(log_head_press(OM_BTH), presspara, para1);
}

void log_event_bth_print2(uint32_t presspara, uint32_t para1, uint32_t para2)
{
    log_event_print2(log_head_press(OM_BTH), presspara, para1, para2);
}

void log_event_bth_print3(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3)
{
    log_event_print3(log_head_press(OM_BTH), presspara, para1, para2, para3);
}

void log_event_bth_print4(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4)
{
    log_event_print4(log_head_press(OM_BTH), presspara, para1, para2, para3, para4);
}

// BT模块按level分类的打印接口
void log_event_bt_err_print0(uint32_t presspara)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print0(log_head_press(OM_BT), presspara);
}

void log_event_bt_err_print1(uint32_t presspara, uint32_t para1)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print1(log_head_press(OM_BT), presspara, para1);
}

void log_event_bt_err_print2(uint32_t presspara, uint32_t para1, uint32_t para2)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print2(log_head_press(OM_BT), presspara, para1, para2);
}

void log_event_bt_err_print3(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print3(log_head_press(OM_BT), presspara, para1, para2, para3);
}

void log_event_bt_err_print4(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print4(log_head_press(OM_BT), presspara, para1, para2, para3, para4);
}

void log_event_bt_wrn_print0(uint32_t presspara)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print0(log_head_press(OM_BT), presspara);
}

void log_event_bt_wrn_print1(uint32_t presspara, uint32_t para1)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print1(log_head_press(OM_BT), presspara, para1);
}

void log_event_bt_wrn_print2(uint32_t presspara, uint32_t para1, uint32_t para2)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print2(log_head_press(OM_BT), presspara, para1, para2);
}

void log_event_bt_wrn_print3(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print3(log_head_press(OM_BT), presspara, para1, para2, para3);
}

void log_event_bt_wrn_print4(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print4(log_head_press(OM_BT), presspara, para1, para2, para3, para4);
}

void log_event_bt_info_print0(uint32_t presspara)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print0(log_head_press(OM_BT), presspara);
}

void log_event_bt_info_print1(uint32_t presspara, uint32_t para1)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print1(log_head_press(OM_BT), presspara, para1);
}

void log_event_bt_info_print2(uint32_t presspara, uint32_t para1, uint32_t para2)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print2(log_head_press(OM_BT), presspara, para1, para2);
}

void log_event_bt_info_print3(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print3(log_head_press(OM_BT), presspara, para1, para2, para3);
}

void log_event_bt_info_print4(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4)
{
    presspara = para_press(LOG_BTMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print4(log_head_press(OM_BT), presspara, para1, para2, para3, para4);
}

// BTH模块按level分类的打印接口
void log_event_bth_err_print0(uint32_t presspara)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print0(log_head_press(OM_BTH), presspara);
}

void log_event_bth_err_print1(uint32_t presspara, uint32_t para1)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print1(log_head_press(OM_BTH), presspara, para1);
}

void log_event_bth_err_print2(uint32_t presspara, uint32_t para1, uint32_t para2)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print2(log_head_press(OM_BTH), presspara, para1, para2);
}

void log_event_bth_err_print3(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print3(log_head_press(OM_BTH), presspara, para1, para2, para3);
}

void log_event_bth_err_print4(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_ERROR, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print4(log_head_press(OM_BTH), presspara, para1, para2, para3, para4);
}

void log_event_bth_wrn_print0(uint32_t presspara)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print0(log_head_press(OM_BTH), presspara);
}

void log_event_bth_wrn_print1(uint32_t presspara, uint32_t para1)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print1(log_head_press(OM_BTH), presspara, para1);
}

void log_event_bth_wrn_print2(uint32_t presspara, uint32_t para1, uint32_t para2)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print2(log_head_press(OM_BTH), presspara, para1, para2);
}

void log_event_bth_wrn_print3(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print3(log_head_press(OM_BTH), presspara, para1, para2, para3);
}

void log_event_bth_wrn_print4(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_WARNING, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print4(log_head_press(OM_BTH), presspara, para1, para2, para3, para4);
}

void log_event_bth_info_print0(uint32_t presspara)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print0(log_head_press(OM_BTH), presspara);
}

void log_event_bth_info_print1(uint32_t presspara, uint32_t para1)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print1(log_head_press(OM_BTH), presspara, para1);
}

void log_event_bth_info_print2(uint32_t presspara, uint32_t para1, uint32_t para2)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print2(log_head_press(OM_BTH), presspara, para1, para2);
}

void log_event_bth_info_print3(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print3(log_head_press(OM_BTH), presspara, para1, para2, para3);
}

void log_event_bth_info_print4(uint32_t presspara, uint32_t para1, uint32_t para2, uint32_t para3, uint32_t para4)
{
    presspara = para_press(LOG_BTHMODULE, LOG_LEVEL_INFO, get_fileid_opt(presspara), get_line_opt(presspara));
    log_event_print4(log_head_press(OM_BTH), presspara, para1, para2, para3, para4);
}