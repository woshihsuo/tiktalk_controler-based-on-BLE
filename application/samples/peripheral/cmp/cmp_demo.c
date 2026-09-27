/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 * Description: ADC Sample Source. \n
 *
 * History: \n
 * 2025-12-22, Create file. \n
 */
#include "pinctrl.h"
#include "gpio.h"
#include "adc.h"
#include "adc_porting.h"
#include "common_def.h"
#include "soc_osal.h"
#include "chip_core_irq.h"
#include "arch_port.h"
#include "app_init.h"

#define CMP_CFG_CLKEN               0x57036004
#define CMP_CFG_CMP_0               0x570362F8
#define CMP_CFG_AMUX_1              0x570362C4
#define CMP_CFG_AMUX_2              0x570362C8
#define CMP_CFG_ANA_2               0x570363DC
#define CMP_CFG_ANA_3               0x570363E0
#define CMP_CFG_ISO                 0x5703601C
#define CMP_RPT_CMP_1               0x57036304
#define CMP_CFG_CMP_1               0x570362FC
#define CMP_RPT_CMP_0               0x57036300

#define CMP_TASK_DURATION_MS        1000

#define CMP_CFG_ANA_3_POS           12
#define CMP_CFG_ANA_3_BITS          8

#define CMP_REFERENCE_VOL_OFFSET    1000
#define CMP_REFERENCE_VOL_MAX       1100
#define CMP_RDAC_CODE_MAX           63

#if defined (CONFIG_CMP_SUPPORT_INT_MODE)
static void cmp_irq_handler(void)
{
    osal_irq_clear(SEC_INT_IRQN);
    uapi_reg_write32(0x5703601C, 0x00000001);
    uapi_reg_write32(0x570362FC, 0x00000001);
    uapi_reg_write32(0x570362FC, 0x00000000);
    uapi_reg_write32(0x5703601C, 0x00000000);

    osal_printk("cmp irq tragger\r\n");
}

void cmp_register_irq(uint32_t irq_id, osal_irq_handler ir_handler)
{
    uint32_t ret = osal_irq_request(irq_id, ir_handler, NULL, NULL, NULL);
    osal_irq_set_priority(irq_id,  irq_prio(irq_id));
    osal_irq_enable(irq_id);
    if (ret != 0) {
        osal_printk("oal_int_create ret:0x%x, irq_id:%d\n", ret, irq_id);
    }
}

void cmp_unregister_irq(uint32_t irq_id)
{
    osal_irq_free(irq_id, NULL);
}
#endif /* CONFIG_CMP_SUPPORT_INT_MODE */

static void cmp_entry(void)
{
    osal_printk("start cmp sample test");
    uint32_t cmp_reference_vol = 0;
    uapi_adc_deinit();
    uapi_adc_init(ADC_CLOCK_NONE);
    uapi_adc_power_en(AFE_AMIC_MODE, true);
    /* [20]cfg_gadc_clken_ctrl;[16]cfg_gadc_clken_prechg;[12]cfg_gadc_clken_byp;[8]cfg_gadc_clken_fc;
    [4]cfg_gadc_clken_bc;[0]cfg_clken_tst */
    uapi_reg_write32(CMP_CFG_CLKEN, 0x00111111);
    /* [4]cmp_even模式切换：0：1+0保持模式;1：1+0-模式;[3:0]cmp_out告警上报阈值 */
    uapi_reg_write32(CMP_CFG_CMP_0, 0x00000008);
    /* [23:12]传感器AMUXP通道选择静态配置：<23>2/5分压选择，低电平分压；<22:12>从高到低依次对应vicmref,VSSAFE1,VSSAFE3,
    AIO7~AIO0;[11:0]传感器AMUXN通道选择静态配置：<11>2/5分压选择，低电平分压；<10:0>从高到低依次对应vicmref,VSSAFE1,
    VSSAFE3,AIO7~AIO0 */
    uapi_reg_write32(CMP_CFG_AMUX_1, 0x00800800);
    /* [7:4]音频AMUXP通道选择静态配置：<7:4>从高到低依次对应AIO7~AIO4;[3:0];[3:0}音频AMUXN通道选择静态配置：
    <3:0>从高到低依次对应AIO7~AIO4 */
    uapi_reg_write32(CMP_CFG_AMUX_2, 0x00000000);
    /* [12]cmpen [10:8]ref_sel<2:0>:<10>vin;<9>ipoly;<8>vbat [5:0]rdac_code<5:0> */
    uapi_reg_write32(CMP_CFG_ANA_2, 0x0000020E);
    /* 后6bit转换为10进制, 假设为X */
    /* 参考电压等于1.1*(X/63) */
    cmp_reference_vol = (CONFIG_CMP_REFERENCE_VOLTAGE * CMP_REFERENCE_VOL_OFFSET) / CMP_REFERENCE_VOL_MAX * \
                        CMP_RDAC_CODE_MAX / CMP_REFERENCE_VOL_OFFSET;
    uapi_reg_setbits(CMP_CFG_ANA_2, 0, 0x6, cmp_reference_vol);
    /* [29:28]difmod_sel<1:0>:<29>vin;<28>rdac_out [24]hys_en  [19:12]chp_sel<7:0> [11:4]chn_sel<7:0>
    <2:0>:<2>rdac_test_en;<1>rdac_en;<0>pole_sel */
    uapi_reg_write32(CMP_CFG_ANA_3, 0x11010000);
    uapi_reg_write32(CMP_CFG_ANA_3, 0x11010002);
    uapi_reg_setbits(CMP_CFG_ANA_3, CMP_CFG_ANA_3_POS, CMP_CFG_ANA_3_BITS, (uint32_t)(1 << CONFIG_CMP_CHANNEL));
    uapi_reg_setbits(CMP_CFG_ANA_3, 0, 1, (uint32_t)CONFIG_POLE_SEL);
    /* [12]cmpen [10:8]ref_sel<2:0>:<10>vin;<9>ipoly;<8>vbat [5:0]rdac_code<5:0> */
    uapi_reg_write32(CMP_CFG_ANA_2, 0x0000120E);
    /* 解a2dISO */
    uapi_reg_write32(CMP_CFG_ISO, 0x00000000);

#if defined (CONFIG_CMP_SUPPORT_INT_MODE)
    /* 注册比较器中断, 产生告警后触发中断回调 */
    osal_irq_clear(SEC_INT_IRQN);
    cmp_unregister_irq(SEC_INT_IRQN);
    cmp_register_irq(SEC_INT_IRQN, (osal_irq_handler)cmp_irq_handler);
#else
    while (1) {
        osal_msleep(CMP_TASK_DURATION_MS);
        /* 读取比较器输出状态, 1表示告警 */
        osal_printk("RPT_CMP_1 = %d\r\n", uapi_reg_read_val32(CMP_RPT_CMP_1));
        /* cmp_out告警清零，1表示清零 */
        uapi_reg_write32(CMP_CFG_CMP_1, 0x00000001);
        uapi_reg_write32(CMP_CFG_CMP_1, 0x00000000);
        /* 读取比较器滤波后输出状态, 1表示告警 */
        osal_printk("RPT_CMP_0 = %d\r\n", uapi_reg_read_val32(CMP_RPT_CMP_0));
    }
#endif /* CONFIG_CMP_SUPPORT_INT_MODE */
}

/* Run the cmp_entry. */
app_run(cmp_entry);