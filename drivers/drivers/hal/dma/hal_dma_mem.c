/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: Provides hal dma mem alloc\n
 *
 * History: \n
 * 2023-04-20， Create file. \n
 */

#include "hal_dma_mem.h"
#include "common_def.h"
#ifdef BUILD_NOOSAL
#include "irmalloc.h"
#else
#include "memory/osal_addr.h"
#endif

#ifdef CONFIG_DMA_LLI_NODE_FIX_MEM
STATIC void *g_hal_dma_lli_node = NULL;
STATIC uint8_t g_hal_dma_lli_index[DMA_CHANNEL_MAX_NUM] = {0};
STATIC uint32_t g_hal_dma_lli_node_maxnum = CONFIG_DMA_LLI_NODE_MAX;

/* 消除Wmissing-prototypes编译选项引起的告警 */
void hal_dma_set_lli_node_num(uint32_t val);

void hal_dma_set_lli_node_num(uint32_t val)
{
    g_hal_dma_lli_node_maxnum = val;
}
#endif

/**
 * @brief  Dma mem init.
 * @param  [in]  mem node size.
 * @return The result of dma mem init.
 */
errcode_t hal_dma_mem_init(uint32_t node_size)
{
#ifdef CONFIG_DMA_LLI_NODE_FIX_MEM
    if (g_hal_dma_lli_node == NULL) {
#ifdef BUILD_NOOSAL
        g_hal_dma_lli_node = irmalloc(node_size * DMA_CHANNEL_MAX_NUM * g_hal_dma_lli_node_maxnum);
#else
        g_hal_dma_lli_node = osal_kmalloc(node_size * DMA_CHANNEL_MAX_NUM * g_hal_dma_lli_node_maxnum, 0);
#endif
        if (g_hal_dma_lli_node == NULL) {
            return ERRCODE_MALLOC;
        }
    }
#else
    unused(node_size);
#endif
    return ERRCODE_SUCC;
}

/**
 * @brief  Dma mem deinit.
 */
void hal_dma_mem_deinit(void)
{
#ifdef CONFIG_DMA_LLI_NODE_FIX_MEM
    if (g_hal_dma_lli_node != NULL) {
#ifdef BUILD_NOOSAL
        irfree(g_hal_dma_lli_node);
#else
        osal_kfree(g_hal_dma_lli_node);
#endif
        g_hal_dma_lli_node = NULL;
    }
#endif
}

/**
 * @brief  Dma alloc mem node.
 * @param  [in]  channel The DMA channel. For details, see @ref dma_channel_t.
           [in]  mem node size.
 * @return mem node.
 */
void *hal_dma_mem_alloc(dma_channel_t ch, uint32_t node_size)
{
#ifdef CONFIG_DMA_LLI_NODE_FIX_MEM
    if ((g_hal_dma_lli_index[ch] >= g_hal_dma_lli_node_maxnum) || (g_hal_dma_lli_node == NULL)) {
        return NULL;
    }
    g_hal_dma_lli_index[ch]++;
    return (uint8_t *)g_hal_dma_lli_node +
        ((uint8_t)ch * g_hal_dma_lli_node_maxnum + g_hal_dma_lli_index[ch] - 1) * node_size;
#else
    unused(ch);
#ifdef BUILD_NOOSAL
    return irmalloc(node_size);
#else
    return osal_kmalloc(node_size, 0);
#endif
#endif
}

/**
 * @brief  Dma free mem node.
 * @param  [in]  channel The DMA channel. For details, see @ref dma_channel_t.
 *         [in]  free mem addr.
 */
void hal_dma_mem_free(dma_channel_t ch, void *addr)
{
#ifdef CONFIG_DMA_LLI_NODE_FIX_MEM
    if (g_hal_dma_lli_index[ch] > 0) {
        g_hal_dma_lli_index[ch]--;
    }
    unused(addr);
#else
    unused(ch);
#ifdef BUILD_NOOSAL
    return irfree(addr);
#else
    return osal_kfree(addr);
#endif
#endif
}
