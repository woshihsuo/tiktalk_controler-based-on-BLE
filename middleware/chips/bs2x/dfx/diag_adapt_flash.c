/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: diag adapt flash
 * This file should be changed only infrequently and with great care.
 */

#include "debug_print.h"
#include "diag.h"
#include "dfx_adapt_layer.h"
#include "sfc.h"
#if defined(SUPPORT_EXTERN_FLASH)
#include "flash.h"
#endif
#include "memory_config_common.h"
#include "partition.h"
#include "uapi_crc.h"
#include "log_file.h"
#include "diag_adapt_flash.h"

#define OFFLINE_HEAD_MAGIC 0x4B3C
#define FILE_HEAD_LENGTH (sizeof(store_file_head_t))

#define SIZE_4K                         0x1000
#define SECTOR_ERASE_DELAY_MS           0x200

#define EXTERN_FLASH_OTA_START_ADDRESS    0x80000
static int32_t diag_adapt_flash_addr_get(uint8_t opt_type, uint32_t offset)
{
    uint32_t addr = offset;
    if (opt_type == FLASH_OP_TYPE_OTA) {
        // 读取分区表获取flash存储地址
        partition_information_t info;
        if (uapi_partition_get_info(PARTITION_FOTA_DATA, &info) != ERRCODE_SUCC) {
            return -1;
        }
        addr += info.part_info.addr_info.addr;
    } else if (opt_type == FLASH_OP_TYPE_LOG_FILE) {
        addr += LOG_FILE_FLASH_START_ADDRESS;
    } else if (opt_type == FLASH_OP_TYPE_FLASH_DATA) {
        addr -= FLASH_START;
    } else {
        return -1;
    }
    return addr;
}

/* flash 读写 */
int32_t diag_adapt_flash_read(uint8_t opt_type, uint32_t offset, uint8_t *buf, uint32_t size)
{
    int32_t addr = diag_adapt_flash_addr_get(opt_type, offset);
    if (addr == -1) { return -1; }
    uint32_t length = size;
    uint32_t irq = osal_irq_lock();
#if defined(SUPPORT_EXTERN_FLASH)
    length = uapi_flash_read_data(0, (addr - EXTERN_FLASH_PARTITION_OFFSET), buf, size);
#else
    if (uapi_sfc_reg_read((uint32_t)addr, buf, size) != ERRCODE_SUCC) {
        length = 0;
    }
#endif
    osal_irq_restore(irq);
    return (int32_t)length;
}

int32_t diag_adapt_flash_write(uint8_t opt_type, uint32_t offset, const uint8_t *buf, uint32_t size,
    bool do_erase)
{
    unused(opt_type);
    unused(do_erase);
    int32_t addr = diag_adapt_flash_addr_get(opt_type, offset);
    if (addr == -1) { return -1; }
    uint32_t length = size;
    uint32_t irq = osal_irq_lock();
#if defined(SUPPORT_EXTERN_FLASH)
    length = uapi_flash_write_data(0, (addr - EXTERN_FLASH_PARTITION_OFFSET), buf, size);
#else
    if (uapi_sfc_reg_write((uint32_t)addr, (uint8_t *)buf, size) != ERRCODE_SUCC) {
        length = 0;
    }
#endif
    osal_irq_restore(irq);
    return (int32_t)length;
}

int32_t diag_adapt_flash_info_write(uint8_t opt_type, uint32_t offset, const uint8_t *buf, uint32_t size,
    bool do_erase)
{
    unused(opt_type);
    unused(do_erase);
    int32_t addr = offset;
    if (opt_type == FLASH_OP_TYPE_OTA) {
        partition_information_t info;
        if (uapi_partition_get_info(PARTITION_FOTA_DATA, &info) != ERRCODE_SUCC) {
            return -1;
        }
        addr += info.part_info.addr_info.addr;
    } else if (opt_type == FLASH_OP_TYPE_LOG_FILE) {
        addr += LOG_FILE_FLASH_START_ADDRESS;
    } else {
        return -1;
    }
    uint32_t length = size;
    uint32_t irq = osal_irq_lock();
#if defined(SUPPORT_EXTERN_FLASH)
    length = uapi_flash_write_data(0, (addr - EXTERN_FLASH_PARTITION_OFFSET), buf, size);
#else
    if (uapi_sfc_reg_write((uint32_t)addr, (uint8_t *)buf, size) != ERRCODE_SUCC) {
        length = 0;
    }
#endif
    osal_irq_restore(irq);
    return (int32_t)length;
}

/*
 * flash擦除数据接口
 */
errcode_t diag_adapt_flash_erase(uint8_t opt_type, const uint32_t offset, const uint32_t size)
{
    unused(opt_type);
    unused(size);

    errcode_t ret = ERRCODE_SUCC;
    int32_t addr = diag_adapt_flash_addr_get(opt_type, offset);
    if (addr == -1) { return ERRCODE_FAIL; }

#if defined(SUPPORT_EXTERN_FLASH)
    for (uint32_t i = addr; i < (addr + size); i += SIZE_4K) {
        ret = uapi_flash_sector_erase(0, (i - EXTERN_FLASH_PARTITION_OFFSET), false);
        while (uapi_flash_is_processing(0)) {
            osal_msleep(SECTOR_ERASE_DELAY_MS);
        }
        if (ret) { return ret; }
    }
#else
    for (uint32_t i = addr; i < (addr + size); i += SIZE_4K) {
        ret = uapi_sfc_reg_erase(i, SIZE_4K);
        if (ret) { return ret; }
    }
#endif
    return ret;
}

typedef struct {
    uint16_t magic;
    uint16_t len;           /* 记录头 + 数据的总长度 */
    uint8_t type;           /* 类型 */
    uint8_t rev;            /* 是否有效 */
    uint16_t index;
    uint16_t crc;           /* CRC16 校验 */
    uint8_t data[0];
} diag_offline_info_t;

typedef struct {
    uint8_t sof;            /* 数据帧开始，固定位0xAF */
    uint8_t ctrl;           /* 控制码，固定为0 */
    uint16_t len;           /* len成员后到尾部的长度 */
    uint8_t model_id;       /* 业务ID，固定位5 */
    uint8_t command_id;     /* 命令ID，固定位0xFE */
    uint8_t data[0];
} diag_online_info_t;

static diag_offline_output_func_t g_diag_offline_output_func = NULL;

static void diag_get_header(uint32_t addr, diag_offline_info_t *record_header)
{
    uapi_sfc_reg_read(addr, (uint8_t *)record_header, sizeof(diag_offline_info_t));
}

static bool diag_check_head_valid(diag_offline_info_t *record_header)
{
    if (record_header->magic == OFFLINE_HEAD_MAGIC) {
        uint16_t crc = uapi_crc16(0, (uint8_t *)record_header, sizeof(diag_offline_info_t) - sizeof(uint16_t));
        if (crc == record_header->crc) {
            return true;
        }
    }
    return false;
}

static void diag_translate_offline_output(uint32_t cur_addr, diag_offline_info_t *cur_info)
{
    /* Step1 : 计算长度并开辟空间 */
    uint16_t online_log_length = cur_info->len - sizeof(diag_offline_info_t) + sizeof(diag_online_info_t);
    uint8_t *online_log_data = osal_kmalloc(online_log_length, OSAL_GFP_ATOMIC);
    if (online_log_data == NULL) {
        return;
    }
    /* Step2 : 构造并拷贝头部信息 */
    uint8_t *log_data_ptr = online_log_data;
    diag_online_info_t online_info = {0};
    online_info.sof = 0xAF;
    online_info.ctrl = 0x0;
    online_info.len = online_log_length - 0x4;
    online_info.model_id = 0x5;
    online_info.command_id = 0xFE;
    memcpy_s(log_data_ptr, sizeof(diag_online_info_t), &online_info, sizeof(diag_online_info_t));
    log_data_ptr += sizeof(diag_online_info_t);
    /* Step3 : 读取日志数据 */
    uint32_t data_length = online_log_length - sizeof(diag_online_info_t);
    uapi_sfc_reg_read(cur_addr, log_data_ptr, data_length);
    /* Step4 : 发送日志数据 */
    if (g_diag_offline_output_func != NULL) {
        g_diag_offline_output_func(online_log_data, online_log_length);
    }
    /* Step5 : 释放在线日志空间 */
    osal_kfree(online_log_data);
}

void uapi_diag_offline_log_output(void)
{
    /* Step1 : 暂停离线日志。 */
    uapi_logfile_suspend(STORE_DIAG);
    /* Step2 : 获取离线日志头 */
    uint32_t cur_addr = LOG_FILE_FLASH_START_ADDRESS + FILE_HEAD_LENGTH;
    uint32_t end_addr = LOG_FILE_FLASH_START_ADDRESS + DFX_OFFLINE_LOG_FILE_SIZE;
    diag_offline_info_t cur_info = {0};
    while (cur_addr < end_addr) {
        diag_get_header(cur_addr, &cur_info);
        if (!diag_check_head_valid(&cur_info)) {
            /* Step3-a : 非法头部，偏移1字节重新读取 */
            cur_addr += 1;
            /* 头部断在文件尾，遍历后的某一次非法头部检查将使得 cur_addr > end_addr */
            continue;
        }
        /* Step3-b : 合法头部，读取数据并发送 */
        /* 离线日志理论尾地址(不考虑循环buffer) */
        uint32_t data_end_addr = cur_addr + cur_info.len;
        if (data_end_addr >= end_addr) {
            /* Step4-a : 数据断在文件尾，直接退出循环。 */
            break;
        }
        cur_addr += sizeof(diag_offline_info_t);
        diag_translate_offline_output(cur_addr, &cur_info);
        /* Step4-b : 读取地址偏移，清空当前头信息 */
        cur_addr += cur_info.len - sizeof(diag_offline_info_t);
        memset_s(&cur_info, sizeof(cur_info), 0, sizeof(cur_info));
    }
    /* Step5 : 恢复离线日志。 */
    uapi_logfile_resume(STORE_DIAG);
}

void uapi_diag_register_offline_output_func(diag_offline_output_func_t func)
{
    g_diag_offline_output_func = func;
}