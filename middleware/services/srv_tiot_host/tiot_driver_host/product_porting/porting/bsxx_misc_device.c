/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description:  \n
 *
 * History: \n
 * 2023-12-26, Create file. \n
 */
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/suspend.h>
#include <linux/delay.h>

#include "tiot_service_interface.h"
#include "tiot_service_interface_ext.h"
#include "tiot_dts_node.h"
#include "tiot_board_log.h"
#include "tiot_types.h"
#include "tiot_osal.h"

#define OPEN_FLAGS_FLOWCTRL_BIT   0

#define MISC_IOCTL_FLOWCTRL_SET     _IOW('t', 0x1, unsigned int)

#define DTS_NODE_COMPATIBLE    "tiot,bsxx"

#define READ_TIMEOUT   0xFFFFFFF
#define TRX_DATA_MAX   9300   /* 按实际单包数据大小最大值修改. */

#define SUBSYS_SLE   0
#define SUBSYS_OM    1

static uint32_t g_open_flags = 1UL << (uint32_t)OPEN_FLAGS_FLOWCTRL_BIT;  /* 默认4线. */

struct tiot_dev {
    osal_mutex misc_dev_mutex;
    struct miscdevice misc;
    tiot_handle handle;
};

static struct tiot_dev g_misc_dev;
static struct tiot_dev g_misc_om_dev;
static const char *g_device_node_name = NULL;
static const char *g_misc_dev_name = NULL;
static const char *g_misc_om_dev_name = NULL;

static char g_misc_rx_data[TRX_DATA_MAX] = { 0 };
static char g_misc_om_rx_data[TRX_DATA_MAX] = { 0 };

static int g_misc_rx_read_idx = 0;
static int g_misc_rx_read_len = 0; /* can read length */

static struct device_node *misc_find_device_node(void)
{
    struct device_node *np = NULL;
    struct device_node *dev_np = NULL;
    /* 获取 top dts节点 */
    np = of_find_compatible_node(NULL, NULL, DTS_NODE_TIOT);
    if (np == NULL) {
        printk("no compatible node.\r\n");
        return NULL;
    }
    /* 获取子dts节点 */
    dev_np = of_find_compatible_node(np, NULL, DTS_NODE_COMPATIBLE);
    if (dev_np == NULL) {
        printk("no device compatible node.\r\n");
        return NULL;
    }
    return dev_np;
}

static int misc_dev_open(struct inode *inode, struct file *filp)
{
    tiot_handle handle = 0;
    struct tiot_dev *dev = container_of(filp->private_data, struct tiot_dev, misc);
    tiot_service_open_param param = { 0 };
    tiot_print_info("%s enter.\n", __func__);

    param.flags = g_open_flags;
    handle = tiot_service_open(g_device_node_name, &param);
    if (handle == 0) {
        return -1;
    }
    dev->handle = handle;
    filp->private_data = dev;
    g_misc_rx_read_idx = 0;
    g_misc_rx_read_len = 0;
    tiot_print_info("%s ok.\n", __func__);
    return 0;
}

static int misc_dev_close(struct inode *inode, struct file *filp)
{
    struct tiot_dev *dev = filp->private_data;
    tiot_print_info("%s enter.\n", __func__);
    tiot_service_close(dev->handle);
    dev->handle = 0;
    tiot_print_info("%s ok.\n", __func__);
    return 0;
}

static ssize_t misc_dev_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    int read_count = count;
    struct tiot_dev *dev = filp->private_data;
    if (count > sizeof(g_misc_rx_data)) {
        return -ERANGE;
    }

    if (g_misc_rx_read_len == 0) {
        /* 这里将timeout_ms设置为0使用非阻塞读 */
        g_misc_rx_read_len = tiot_service_read(dev->handle, g_misc_rx_data,
                                               sizeof(g_misc_rx_data), 0);
        if (g_misc_rx_read_len <= 0) {
            g_misc_rx_read_len = 0;
            return 0;
        }
    }

    if (read_count > g_misc_rx_read_len) {
        read_count = g_misc_rx_read_len;
    }
    if (copy_to_user(buf, g_misc_rx_data + g_misc_rx_read_idx, read_count)) {
        return -ERANGE;
    }
    g_misc_rx_read_idx += read_count;
    g_misc_rx_read_len -= read_count;
    if (g_misc_rx_read_len == 0) {
        g_misc_rx_read_idx = 0;
    }

    return read_count;
}

static uint32_t misc_dev_poll(struct file *filp, poll_table *wait)
{
    uint32_t mask = 0;
    uint32_t ready = 0;
    struct tiot_dev *dev = NULL;
    const tiot_service_rx_param param = { 0, SUBSYS_SLE, filp, wait };

    tiot_print_dbg("misc_dev_poll enter.\n");

    ready = (g_misc_rx_read_len != 0);
    if (ready != 0) {
        mask |= POLLIN | POLLRDNORM;
        return mask;
    }
    if ((filp == NULL) || (wait == NULL)) {
        tiot_print_dbg("invalid filp or poll_table.\n");
        return mask;
    }
    dev = filp->private_data;
    /* param->filp, param->poll_wait参数用于poll接口，用来查询是否有数据可读而非实际的读出操作 */
    if (tiot_service_read_ext(dev->handle, NULL, 0, &param) != 0) {
        mask |= POLLIN | POLLRDNORM;
    }

    return mask;
}

static ssize_t dev_write(struct tiot_dev *dev, const char __user *buf, size_t count)
{
    ssize_t ret;
    char *tx_data = NULL;

    if (count > TRX_DATA_MAX) {
        return -ERANGE;
    }
    tx_data = (char *)kmalloc(count, GFP_KERNEL);
    if (tx_data == NULL) {
        return -ENOMEM;
    }
    if (copy_from_user(tx_data, buf, count)) {
        kfree(tx_data);
        return -ERANGE;
    }
    ret = tiot_service_write(dev->handle, tx_data, count);
    kfree(tx_data);
    return ret;
}

static ssize_t misc_dev_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    struct tiot_dev *dev = filp->private_data;

    return dev_write(dev, buf, count);
}

static long misc_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    volatile unsigned long flags = 0;
    struct tiot_dev *dev = filp->private_data;

    flags = g_open_flags;
    tiot_print_info("misc_dev_ioctl cmd 0x%x, arg 0x%x\n", cmd, arg);
    switch (cmd) {
        case MISC_IOCTL_FLOWCTRL_SET:
            if (arg == 0) {
                clear_bit(OPEN_FLAGS_FLOWCTRL_BIT, &flags);
            } else if (arg == 1) {
                set_bit(OPEN_FLAGS_FLOWCTRL_BIT, &flags);
            } else {
                ret = -EINVAL;
            }
            break;
        default:
            ret = -ENOIOCTLCMD;
            break;
    }
    g_open_flags = (uint32_t)flags;
    return ret;
}

static int misc_dev_om_open(struct inode *inode, struct file *filp)
{
    tiot_handle handle = 0;
    struct tiot_dev *dev = container_of(filp->private_data, struct tiot_dev, misc);
    tiot_service_open_param param = { 0 };
    tiot_print_info("%s enter.\n", __func__);

    param.flags = g_open_flags;
    handle = tiot_service_open(g_device_node_name, &param);
    if (handle == 0) {
        return -1;
    }
    dev->handle = handle;
    filp->private_data = dev;
    tiot_print_info("%s ok.\n", __func__);
    return 0;
}

static int misc_dev_om_close(struct inode *inode, struct file *filp)
{
    struct tiot_dev *dev = filp->private_data;
    tiot_print_info("%s enter.\n", __func__);
    tiot_service_close(dev->handle);
    dev->handle = 0;
    tiot_print_info("%s ok.\n", __func__);
    return 0;
}

static ssize_t misc_dev_om_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    int read_count;
    struct tiot_dev *dev = filp->private_data;
    const tiot_service_rx_param param = { 0, SUBSYS_OM };
    if (count > sizeof(g_misc_om_rx_data)) {
        return -ERANGE;
    }
    read_count = tiot_service_read_ext(dev->handle, g_misc_om_rx_data, sizeof(g_misc_om_rx_data), &param);
    if (copy_to_user(buf, g_misc_om_rx_data, read_count)) {
        return -ERANGE;
    }
    return read_count;
}

static uint32_t misc_dev_om_poll(struct file *filp, poll_table *wait)
{
    uint32_t mask = 0;
    struct tiot_dev *dev = NULL;
    const tiot_service_rx_param param = { 0, SUBSYS_OM, filp, wait };

    tiot_print_dbg("misc_dev_om_poll enter.\n");
    if ((filp == NULL) || (wait == NULL)) {
        tiot_print_dbg("invalid filp or poll_table.\n");
        return mask;
    }
    dev = filp->private_data;
    /* param->filp, param->poll_wait参数用于poll接口，用来查询是否有数据可读而非实际的读出操作 */
    if (tiot_service_read_ext(dev->handle, NULL, 0, &param) != 0) {
        mask |= POLLIN | POLLRDNORM;
    }

    return mask;
}

static ssize_t misc_dev_om_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    /* OM is for read only. */
    return 0;
}

static const struct file_operations misc_fops = {
    .owner = THIS_MODULE,
    .read = misc_dev_read,
    .poll = misc_dev_poll,
    .write = misc_dev_write,
    .unlocked_ioctl = misc_dev_ioctl,
    .open = misc_dev_open,
    .release = misc_dev_close
};

static const struct file_operations misc_om_fops = {
    .owner = THIS_MODULE,
    .read = misc_dev_om_read,
    .poll = misc_dev_om_poll,
    .write = misc_dev_om_write,
    .open = misc_dev_om_open,
    .release = misc_dev_om_close
};

static int misc_pm_suspend(void)
{
    int ret;
    tiot_print_info("suspend notify+++\r\n");
    ret = tiot_service_pm_ctrl(g_misc_dev.handle, TIOT_PM_CTRL_SUSPEND_PREPARE);
    if (ret != 0) {
        tiot_print_err("suspend notify error\r\n");
    }
    return ret;
}

static int misc_pm_resume(void)
{
    int ret;
    tiot_print_info("resume notify---\r\n");
    ret = tiot_service_pm_ctrl(g_misc_dev.handle, TIOT_PM_CTRL_RESUME_PREPARE);
    if (ret != 0) {
        tiot_print_err("resume notify error\r\n");
    }
    return ret;
}

static int misc_pm_callback(struct notifier_block *nb, unsigned long mode, void *_unused)
{
    tiot_unused(nb);
    tiot_unused(_unused);
    tiot_print_info("misc_pm_callback mode:%lu\r\n", mode);
    switch (mode) {
        case PM_SUSPEND_PREPARE:
            misc_pm_suspend();
            break;
        case PM_POST_SUSPEND:
            misc_pm_resume();
            break;
        default:
            break;
    }
    return NOTIFY_OK;
}

static struct notifier_block misc_pm_notifier = {
    .notifier_call = misc_pm_callback,
};

static int bsxx_probe(struct platform_device *pdev)
{
    int ret;
    struct device_node *dev_node = NULL;
    tiot_print_info("probe enter++\r\n");
    dev_node = misc_find_device_node();
    if (dev_node == NULL) {
        return -1;
    }
    g_device_node_name = dev_node->name;
    ret = of_property_read_string(dev_node, DTS_PROP_MISC_DEV_NAME, &g_misc_dev_name);
    ret = of_property_read_string(dev_node, DTS_PROP_OM_MISC_DEV_NAME, &g_misc_om_dev_name);
    of_node_put(dev_node);

    ret = tiot_service_init();
    if (ret != 0) {
        return -1;
    }
    if (g_misc_dev_name) {
        osal_mutex_init(&g_misc_dev.misc_dev_mutex);
        g_misc_dev.misc.minor = MISC_DYNAMIC_MINOR;
        g_misc_dev.misc.name = g_misc_dev_name;  /* 可以自己修改名字 */
        g_misc_dev.misc.fops = &misc_fops;
        g_misc_dev.misc.parent = &pdev->dev;
        ret = misc_register(&g_misc_dev.misc);
        if (ret != 0) {
            printk("misc dev register fail.\r\n");
            return -1;
        }
    }
    if (g_misc_om_dev_name) {
        osal_mutex_init(&g_misc_om_dev.misc_dev_mutex);
        g_misc_om_dev.misc.minor = MISC_DYNAMIC_MINOR;
        g_misc_om_dev.misc.name = g_misc_om_dev_name;
        g_misc_om_dev.misc.fops = &misc_om_fops;
        g_misc_om_dev.misc.parent = &pdev->dev;
        ret = misc_register(&g_misc_om_dev.misc);
        if (ret != 0) {
            printk("misc om dev register fail.\r\n");
            return -1;
        }
    }
    ret = register_pm_notifier(&misc_pm_notifier);
    if (ret != 0) {
        printk("pm notifier register fail.\r\n");
        return -1;
    }
    return 0;
}

static int bsxx_remove(struct platform_device *pdev)
{
    unregister_pm_notifier(&misc_pm_notifier);
    if (g_misc_dev_name) {
        misc_deregister(&g_misc_dev.misc);
        osal_mutex_destroy(&g_misc_dev.misc_dev_mutex);
    }
    if (g_misc_om_dev_name) {
        misc_deregister(&g_misc_om_dev.misc);
        osal_mutex_destroy(&g_misc_om_dev.misc_dev_mutex);
    }
    tiot_service_deinit();
    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int misc_suspend(struct device *dev)
{
    int ret;
    tiot_unused(dev);
    tiot_print_info("platform suspend+++\r\n");
    ret = tiot_service_pm_ctrl(g_misc_dev.handle, TIOT_PM_CTRL_SUSPEND);
    if (ret != 0) {
        tiot_print_err("platform suspend error\r\n");
    }
    return 0;
}

static int misc_resume(struct device *dev)
{
    int ret;
    tiot_unused(dev);
    tiot_print_info("platform resume---\r\n");
    ret = tiot_service_pm_ctrl(g_misc_dev.handle, TIOT_PM_CTRL_RESUME);
    if (ret != 0) {
        tiot_print_err("platform resume error\r\n");
    }
    return 0;
}

static SIMPLE_DEV_PM_OPS(misc_pm_ops, misc_suspend, misc_resume);
#else
static SIMPLE_DEV_PM_OPS(misc_pm_ops, NULL, NULL);
#endif

/* not use of match for now. */
static struct platform_driver bsxx_driver = {
    .probe  = bsxx_probe,
    .remove    = bsxx_remove,
    .driver = {
        .name = "tiot",
        .pm = &misc_pm_ops
    }
};

static int __init tiot_module_init(void)
{
    int ret;
    ret = platform_driver_register(&bsxx_driver);
    if (ret) {
        tiot_print_err("pdrv register failed\r\n");
    } else {
        tiot_print_err("pdrv register ok\r\n");
    }
    return ret;
}

static void __exit tiot_module_exit(void)
{
    platform_driver_unregister(&bsxx_driver);
}

module_init(tiot_module_init);
module_exit(tiot_module_exit);
MODULE_LICENSE("GPL v2");
