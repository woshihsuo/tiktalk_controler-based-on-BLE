CUR_MK_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CUR_MK_DIR := $(dir $(CUR_MK_PATH))
CUR_MK_DIR := $(realpath $(CUR_MK_DIR))

define calc_relpath
$(shell realpath $(1) --relative-to=$(2))
endef

_TOP_DIR := $(realpath $(CUR_MK_DIR)/../../../../)

PLATFORM_OSAL_DIR := $(_TOP_DIR)/driver/platform/osal
PLATFORM_LIBC_SEC_DIR := $(_TOP_DIR)/driver/platform/libc_sec
BSLE_INC_DIR := $(_TOP_DIR)/driver/foundation/wpa/inc
MIDDLEWARE_SLE := $(BSLE_INC_DIR)
SLE_CONFIG_PATH := $(TOPDIR)/build/config
SLE_SAMPLE_PATH := $(TOPDIR)/application/sample/ble/src

# rtos 平台编译适配
ROOT_DIR := $(_TOP_DIR)
INC_NATIVE := $(_TOP_DIR)/driver/platform/drv/include
INC_CMSIS := $(_TOP_DIR)/driver/platform/drv/include