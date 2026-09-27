/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2024. All rights reserved.
 *
 * Description: head file of secure boot efuse \n
 *
 * History: \n
 * 2024-08-06, Create file. \n
 */
#ifndef SECURE_EFUSE_H
#define SECURE_EFUSE_H
#include "errcode.h"

/*****************************************************************************************
 * This function is used for secure boot use case.
 * This function is used to enable secure boot and disable jtag by fuse
 *******************************************************************************************/
errcode_t secure_boot_efuse(void);

/*****************************************************************************************
 * This function is used for non-secure boot use case.
 * For no-secure booting, we have flashboot_A and flashboot_B, if flashboot_A is corrupted,
 * need jump to flashboot_B for running. So bootrom need check if flashboot is corrupted.
 * This function is used to enable bootrom checking flashboot hash. *
 *******************************************************************************************/
errcode_t hash_check_enable_efuse(void);

#endif
