/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2024. All rights reserved.
 *
 * Description: Application core main function for standard \n
 *
 * History: \n
 * 2022-07-27, Create file. \n
 */
#include "securec.h"
#include "main_init.h"
#include "chip_io.h"
#include "application_version.h"

#ifdef CONFIG_FLASHBOOT_INFO_TO_APP
flashboot_info_t g_flashboot_to_app_info = { 0 };
#endif

/*
 *  芯片初始化入口。
 *  芯片初始化信息请参考：drivers/chips/bs2x/main_init/main_init.c main_init函数。
 *  软件线程初始化请参考：drivers/chips/bs2x/main_init/app_os_init.c app_os_init函数。
 */
void main(uint32_t *addr)
{
#ifdef CONFIG_FLASHBOOT_INFO_TO_APP
    (void)memcpy_s(&g_flashboot_to_app_info, sizeof(flashboot_info_t), addr, sizeof(flashboot_info_t));
#else
    UNUSED(addr);
#endif
    main_init();
}
