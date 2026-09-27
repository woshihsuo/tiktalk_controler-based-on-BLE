/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Description: IR STUDY Sample Source. \n
 *
 * History: \n
 * 2024-02-02, Create file. \n
 */
#include "tcxo.h"
#include "ir_tx.h"
#include "debug_print.h"
#include "osal_task.h"
#include "soc_osal.h"
#include "common_def.h"
#include "ir_porting.h"
#include "gpio.h"
#include "ir_study.h"

#define IR_TICK_COUNT                   100
#define IR_TICK_COUNT_OFFSET            5
#define IR_TIMER_DELAY                  10
#define IR_TIMER1_OUT_US                70
#define IR_TIMER0_OUT_US                400000
#define IR_CARRIER_MIN                  10
#define IR_CARRIER_MAX                  40
#define IR_CARRIER_TCXO                 20
#define IR_FREQ_36K                     36700
#define IR_FREQ_38K                     38000
#define IR_FREQ_56K                     56000
#define IR_FREQ_56K_OFFSET1             1
#define IR_FREQ_56K_OFFSET2             2
#define IR_FREQ_56K_OFFSET3             3
#define IR_FREQ_56K_OFFSET4             4
#define IR_FREQ_56K_OFFSET5             5
#define IR_CYCLE_36K                    27
#define IR_CYCLE_38K                    26
#define IR_CYCLE_56K                    17
#define IR_FLASH_REG_OFFSET             2
#define IR_FLASH_WRITE_OFFSET           3
#define IR_FLASH_INDEX_OFFSET           4

#define PATTERN_LEN                     200
#define IR_KEY_NUM                      10
#define IR_ADDR_OFFSET                  0x400

#define IR_CMP_STUDY_INIT_DELAY         100
#define IR_STUDY_START_FLAG             0xAABBAABB
#define IR_STUDY_END_FLAG               0xBBAABBAA

static int32_t g_rx_pattern[PATTERN_LEN] = { 0 };
static uint8_t g_rx_count = 0;
static uint8_t g_rx_key = 0;
static uint64_t g_rx_start = 0;

static uint32_t g_carrier_count = 0;
static uint64_t g_carrier_timer_start = 0;
static uint64_t g_carrier_timer = 0;
static bool g_carrier_flag = false;
static bool g_timer_flag = false;

static ir_callback_t g_ir_callback;
static bool g_ir_cb_register_flag = false;
typedef struct rx_pattern_info {
    uint32_t freq;
    uint32_t length;
    int32_t *rx_buff;
} rx_pattern_info_t;

static uint8_t g_study_tmp_buff[0x1000] = { 0 };

static const uint32_t g_rx_addr_map[IR_KEY_NUM]  = {IR_FLASH_OFFSET, IR_FLASH_OFFSET + IR_ADDR_OFFSET,
                                                    IR_FLASH_OFFSET + IR_ADDR_OFFSET * 2,
                                                    IR_FLASH_OFFSET + IR_ADDR_OFFSET * 3,
                                                    IR_FLASH_OFFSET + IR_ADDR_OFFSET * 4,
                                                    IR_FLASH_OFFSET + IR_ADDR_OFFSET * 5,
                                                    IR_FLASH_OFFSET + IR_ADDR_OFFSET * 6,
                                                    IR_FLASH_OFFSET + IR_ADDR_OFFSET * 7,
                                                    IR_FLASH_OFFSET + IR_ADDR_OFFSET * 8,
                                                    IR_FLASH_OFFSET + IR_ADDR_OFFSET * 9};
static const uint8_t g_rx_key_index[IR_KEY_NUM]  = {0xCA, 0xC1, 0xCE, 0xD2, 0x99, 0x6, 0x1, 0x2, 0x3, 0x4};

static void ir_clock_check_study_end(uint32_t us)
{
    uint32_t lock_int_save = osal_irq_lock();
    ir_port_timer0_init(us);
    osal_irq_restore(lock_int_save);
}

static void ir_clockstart(uint32_t us)
{
    uint32_t lock_int_save = osal_irq_lock();
    ir_port_timer1_init(us);
    osal_irq_restore(lock_int_save);
}

static void ir_tickentry(void)
{
    uint64_t current_time = uapi_tcxo_get_us();

    if (g_rx_count < PATTERN_LEN) {
        g_rx_pattern[g_rx_count] = (int32_t)(current_time - IR_TIMER1_OUT_US + IR_CARRIER_TCXO - g_rx_start);
        g_carrier_timer += current_time - IR_TIMER1_OUT_US + IR_CARRIER_TCXO - g_rx_start;
        g_rx_count++;
        g_rx_start = current_time  - IR_TIMER1_OUT_US;
        g_timer_flag = true;
    }
    if (g_rx_count == 1) {
        ir_clock_check_study_end(IR_TIMER0_OUT_US);
    }
    ir_port_tick_timer1_eoi_clr();
    osal_irq_clear(TIMER_1_IRQN);
}

static void ir_study_freq_set(uint32_t* freq)
{
    uint64_t carrier_time = 0;
    carrier_time = g_carrier_timer / g_carrier_count;
    switch (carrier_time) {
        case IR_CYCLE_36K :
            *freq = IR_FREQ_36K;
            break;
        case IR_CYCLE_38K :
            *freq = IR_FREQ_38K;
            break;
        case IR_CYCLE_56K :
        case IR_CYCLE_56K + IR_FREQ_56K_OFFSET1 :
        case IR_CYCLE_56K + IR_FREQ_56K_OFFSET2 :
        case IR_CYCLE_56K + IR_FREQ_56K_OFFSET3 :
        case IR_CYCLE_56K + IR_FREQ_56K_OFFSET4 :
        case IR_CYCLE_56K + IR_FREQ_56K_OFFSET5 :
            *freq = IR_FREQ_56K;
            break;
        default :
            *freq = 0;
            break;
    }
}

errcode_t uapi_ir_register_irq_callback(ir_callback_t callback)
{
    g_ir_callback = callback;
    g_ir_cb_register_flag = true;
    return ERRCODE_SUCC;
}

void uapi_ir_unregister_irq_callback(void)
{
    g_ir_callback = NULL;
    g_ir_cb_register_flag = false;
}

static void ir_study_end_tickentry(void)
{
    ir_port_tick_timer1_disable();
    ir_port_tick_timer0_disable();
    ir_port_gpio_mask_interrupt();
    uint32_t addr = 0;
    uint8_t write_index = 0;
    rx_pattern_info_t rx_info;
    rx_info.length = (uint32_t)g_rx_count;
    g_rx_count = 0;
    rx_info.rx_buff = g_rx_pattern;
    uint32_t irq_sts = osal_irq_lock();
    for (uint8_t i = 0; i < IR_KEY_NUM; i++) {
        if (g_rx_key == g_rx_key_index[i]) {
            write_index = i;
            addr = g_rx_addr_map[i];
            break;
        }
    }
    uint32_t erase_addr = IR_FLASH_OFFSET + (0x1000 * (write_index / IR_FLASH_INDEX_OFFSET));
    write_index = write_index % IR_FLASH_INDEX_OFFSET;
    ir_study_freq_set(&rx_info.freq);
    osal_printk("[ir_study] study freq : %lu \r\n", rx_info.freq);

    if (rx_info.freq == 0 && g_ir_cb_register_flag == true) {
        osal_irq_restore(irq_sts);
        osal_irq_clear(TIMER_0_IRQN);
        g_ir_callback(IR_STUDY_FAILURE);
        return;
    }

    if (addr < IR_FLASH_OFFSET || addr > IR_FLASH_OFFSET + IR_FLASH_LENGTH) {
        osal_irq_restore(irq_sts);
        osal_irq_clear(TIMER_0_IRQN);
        osal_printk("ir study addr error:0x%x\n", addr);
        return;
    }

    ir_port_flash_reg_read(erase_addr, (uint8_t*)(g_study_tmp_buff), 0x1000);
    (void)memcpy_s(g_study_tmp_buff + (write_index * IR_ADDR_OFFSET), sizeof(uint32_t) * IR_FLASH_REG_OFFSET,
                   (uint8_t *)(&rx_info), sizeof(uint32_t) * IR_FLASH_REG_OFFSET);
    (void)memcpy_s(g_study_tmp_buff + (write_index * IR_ADDR_OFFSET) + sizeof(uint32_t) * IR_FLASH_REG_OFFSET,
                   (rx_info.length) * sizeof(int32_t), (uint8_t *)(rx_info.rx_buff),
                   (rx_info.length) * sizeof(int32_t));
    ir_port_flash_reg_erase(erase_addr, 0x1000);
    ir_port_flash_reg_write(erase_addr, g_study_tmp_buff, 0x1000);
    (void)memset_s(g_study_tmp_buff, IR_ADDR_OFFSET * sizeof(int32_t), 0, IR_ADDR_OFFSET * sizeof(int32_t));
    if (g_ir_cb_register_flag == true) {
        g_ir_callback(IR_STUDY_SUCCESS);
    }
    osal_irq_restore(irq_sts);
    ir_port_tick_timer0_eoi_clr();
    osal_irq_clear(TIMER_0_IRQN);
}

static void ir_cmp_study_end_tickentry(void)
{
    ir_port_tick_timer1_disable();
    ir_port_tick_timer0_disable();
    uint32_t addr = 0;
    uint32_t erase_addr = 0;
    uint8_t write_index = 0;
    rx_pattern_info_t rx_info;
    rx_info.length = (uint32_t)g_rx_count;
    g_rx_count = 0;
    rx_info.rx_buff = g_rx_pattern;
    uint32_t irq_sts = osal_irq_lock();
    for (uint8_t i = 0; i < IR_KEY_NUM; i++) {
        if (g_rx_key == g_rx_key_index[i]) {
            write_index = i;
            addr = g_rx_addr_map[i];
            break;
        }
    }
    erase_addr = IR_FLASH_OFFSET + (0x1000 * (write_index / IR_FLASH_INDEX_OFFSET));
    write_index = write_index % IR_FLASH_INDEX_OFFSET;

    ir_study_freq_set(&rx_info.freq);
    if (addr < IR_FLASH_OFFSET || addr > IR_FLASH_OFFSET + IR_FLASH_LENGTH) {
        osal_irq_restore(irq_sts);
        ir_port_cmp_deinit();
        osal_irq_clear(TIMER_0_IRQN);
        osal_printk("ir study addr error:0x%x\n", addr);
        return;
    }

    ir_port_flash_reg_read(erase_addr, (uint8_t*)(g_study_tmp_buff), 0x1000);
    (void)memcpy_s(g_study_tmp_buff + (write_index * IR_ADDR_OFFSET), sizeof(uint32_t) * IR_FLASH_REG_OFFSET,
                   (uint8_t *)(&rx_info), sizeof(uint32_t) * IR_FLASH_REG_OFFSET);
    (void)memcpy_s(g_study_tmp_buff + (write_index * IR_ADDR_OFFSET) + sizeof(uint32_t) * IR_FLASH_REG_OFFSET,
                   (rx_info.length) * sizeof(int32_t), (uint8_t *)(rx_info.rx_buff),
                   (rx_info.length) * sizeof(int32_t));
    ir_port_flash_reg_erase(erase_addr, 0x1000);
    ir_port_flash_reg_write(erase_addr, g_study_tmp_buff, 0x1000);
    (void)memset_s(g_study_tmp_buff, IR_ADDR_OFFSET * sizeof(int32_t), 0, IR_ADDR_OFFSET * sizeof(int32_t));

    osal_irq_restore(irq_sts);
    ir_port_cmp_deinit();
    osal_irq_clear(TIMER_0_IRQN);
}

static int ir_gpio_irq_handler(int irq_num, const void *tmp)
{
    unused(irq_num);
    unused(tmp);
    uint32_t int_status = ir_port_gpio_get_intr_state();
    ir_port_gpio_eoi_clr_all();
    osal_irq_clear(GPIO_0_IRQN);
    if (int_status == 1) {
        if (g_carrier_count == 0) {
            /* 记录红外学习载波开始时间 */
            g_rx_start = uapi_tcxo_get_us();
        }
        g_carrier_count++;
        /* 统计无载波的时间 */
        if (g_timer_flag == true) {
            uint64_t time_tmp = g_rx_start;
            g_rx_start = uapi_tcxo_get_us();
            if (g_rx_count < PATTERN_LEN) {
                g_rx_pattern[g_rx_count] = (0 - (int32_t)(g_rx_start - time_tmp));
                g_rx_count++;
                g_timer_flag = false;
            }
        }
        ir_port_tick_timer1_disable();
        ir_clockstart(IR_TIMER1_OUT_US);
    }
    return 0;
}

void uapi_ir_study_start(uint8_t key_value)
{
    g_rx_key = key_value;

    g_rx_count = 0;
    g_rx_start = 0;
    g_carrier_count = 0;
    g_carrier_timer_start = 0;
    g_carrier_timer = 0;
    g_carrier_flag = false;
    g_timer_flag = false;
    (void)memset_s(g_rx_pattern, PATTERN_LEN * sizeof(int32_t), 0, PATTERN_LEN * sizeof(int32_t));

    ir_port_unregister_irq(TIMER_1_IRQN);
    ir_port_register_irq(TIMER_1_IRQN, (osal_irq_handler)ir_tickentry);

    ir_port_unregister_irq(TIMER_0_IRQN);
    ir_port_register_irq(TIMER_0_IRQN, (osal_irq_handler)ir_study_end_tickentry);

    ir_port_gpio_init();

    ir_port_unregister_irq(GPIO_0_IRQN);
    ir_port_register_irq(GPIO_0_IRQN, (osal_irq_handler)ir_gpio_irq_handler);
}

void uapi_ir_study_send(uint8_t key_value, uint32_t length, uint32_t freq)
{
    uint32_t addr = 0;
    int32_t rx_pattern[PATTERN_LEN] = { 0 };
    errcode_t ret;
    for (uint8_t i = 0; i < IR_KEY_NUM; i++) {
        if (key_value == g_rx_key_index[i]) {
            addr = g_rx_addr_map[i];
            break;
        }
    }
    if (addr < IR_FLASH_OFFSET || addr > IR_FLASH_OFFSET + IR_FLASH_LENGTH) {
        osal_printk("ir study addr error:0x%x\n", addr);
        return;
    }
    ret = ir_port_flash_reg_read(addr + sizeof(int32_t) * IR_FLASH_REG_OFFSET, (uint8_t *)rx_pattern,
                                 length * sizeof(int32_t));
    if (ret != ERRCODE_SUCC) {
        osal_printk("flash read error:0x%x\n", ret);
        return;
    }

    uapi_ir_init(CONFIG_SAMPLE_IR_SEND_PIN);
    uapi_ir_transmit(freq, rx_pattern, length);
}

uint32_t uapi_ir_study_check(uint8_t key_value, uint32_t* len)
{
    uint32_t addr = 0;
    uint32_t read_buff[2] = { 0 };
    errcode_t ret;
    for (uint8_t i = 0; i < IR_KEY_NUM; i++) {
        if (key_value == g_rx_key_index[i]) {
            addr = g_rx_addr_map[i];
            break;
        }
    }
    if (addr < IR_FLASH_OFFSET || addr > IR_FLASH_OFFSET + IR_FLASH_LENGTH) {
        osal_printk("ir study addr error:0x%x\n", addr);
        return 0;
    }
    ret = ir_port_flash_reg_read(addr, (uint8_t *)(read_buff), sizeof(uint32_t) * IR_FLASH_REG_OFFSET);
    if (ret != ERRCODE_SUCC) {
        osal_printk("flash read error:0x%x\n", ret);
    }
    *len = read_buff[1];

    return read_buff[0];
}

void uapi_ir_restore_default_setting(void)
{
    ir_port_flash_reg_erase(IR_FLASH_OFFSET, 0x1000);
    ir_port_flash_reg_erase(IR_FLASH_OFFSET + 0x1000, 0x1000);
}

static void cmp_tickentry(void)
{
    if (ir_port_cmp_irq_clear() == 1) {
        g_carrier_count++;
        ir_port_tick_timer1_disable();
        ir_clockstart(IR_TIMER1_OUT_US);
        return;
    }

    if (g_carrier_count == 0) {
        /* 记录红外学习载波开始时间 */
        g_rx_start = uapi_tcxo_get_us();
    }

    /* 统计无载波的时间 */
    if (g_timer_flag == true) {
        uint64_t time_tmp = g_rx_start;
        g_rx_start = uapi_tcxo_get_us();
        if (g_rx_count < PATTERN_LEN) {
            g_rx_pattern[g_rx_count] = (int32_t)(g_rx_start - time_tmp);
            g_rx_count++;
            g_timer_flag = false;
        }
    }

    return;
}

void uapi_ir_study_by_cmp_start(uint8_t key_value)
{
    g_rx_key = key_value;

    g_rx_count = 0;
    g_rx_start = 0;
    g_carrier_count = 0;
    g_carrier_timer_start = 0;
    g_carrier_timer = 0;
    g_carrier_flag = false;
    g_timer_flag = false;

    (void)memset_s(g_rx_pattern, PATTERN_LEN * sizeof(int32_t), 0, PATTERN_LEN * sizeof(int32_t));

    ir_port_unregister_irq(TIMER_1_IRQN);
    ir_port_register_irq(TIMER_1_IRQN, (osal_irq_handler)ir_tickentry);

    ir_port_unregister_irq(TIMER_0_IRQN);
    ir_port_register_irq(TIMER_0_IRQN, (osal_irq_handler)ir_cmp_study_end_tickentry);

    ir_port_cmp_init();
    osal_msleep(IR_CMP_STUDY_INIT_DELAY);
    uapi_reg_write(IR_CFG_ISO, 0x00000001);
    uapi_reg_write(IR_CFG_CMP_1, 0x00000001);
    uapi_reg_write(IR_CFG_CMP_1, 0x00000000);
    uapi_reg_clrbit(IR_CFG_ANA_3, 0);
    uapi_reg_write(IR_CFG_ISO, 0x00000000);
    osal_irq_clear(SEC_INT_IRQN);
    ir_port_unregister_irq(SEC_INT_IRQN);
    ir_port_register_irq(SEC_INT_IRQN, (osal_irq_handler)cmp_tickentry);
}