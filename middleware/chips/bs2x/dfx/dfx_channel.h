/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: dfx uart channel
 * This file should be changed only infrequently and with great care.
 */
#ifndef DFX_CHANNEL_H
#define DFX_CHANNEL_H

#include <stdint.h>
#include <diag.h>
#include "errcode.h"

#if defined(CONFIG_SUPPORT_LOG_CUSTOM_OUTPUT)
typedef void (*diag_custom_output_func_t)(uint8_t *data, uint8_t length);
typedef void (*diag_trigger_func_t)(void);
#endif  /* CONFIG_SUPPORT_LOG_CUSTOM_OUTPUT */

void diag_uart_rx_proc(uint8_t *buffer, uint16_t length);
errcode_t diag_register_channel(void);

#if defined(CONFIG_SUPPORT_LOG_CUSTOM_OUTPUT)
void uapi_diag_register_custom_output(diag_custom_output_func_t func);
void uapi_diag_set_dest_id(uint8_t dest_id);
void uapi_diag_switch_custom_output(bool en);
bool uapi_diag_get_custom_output_status(void);

bool uapi_diag_block_check(void);
void uapi_diag_block_clear(void);
void uapi_diag_register_trigger_func(diag_trigger_func_t func);
void uapi_diag_switch_trigger_enable(bool en);
void uapi_diag_enable_trans(uint8_t id, uint8_t count);

#endif  /* CONFIG_SUPPORT_LOG_CUSTOM_OUTPUT */
#endif