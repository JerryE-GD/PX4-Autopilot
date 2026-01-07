/****************************************************************************
 *
 *   Copyright (C) 2021 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file usb.c
 *
 * Board-specific USB functions.
 */

#include "board_config.h"
#include <nuttx/usb/usbdev.h>
#include <nuttx/usb/usbdev_trace.h>
#include <stm32_otg.h>
#include <debug.h>
#include <stdbool.h>
#include <stdint.h>

/************************************************************************************
 * Private Definitions (新增：Bootloader 专用，兼容PX4原生逻辑)
 ************************************************************************************/
static bool g_usb_initialized = false; // USB初始化状态

/************************************************************************************
 * Name: stm32_usbinitialize
 *
 * Description:
 *   Called to setup USB-related GPIO pins for the board.
 *
 ************************************************************************************/

__EXPORT void stm32_usbinitialize(void)
{
	/* The OTG FS has an internal soft pull-up */

	/* Configure the OTG FS VBUS sensing GPIO, Power On, and Overcurrent GPIOs */

#ifdef CONFIG_STM32H7_OTGFS
	stm32_configgpio(GPIO_OTGFS_VBUS);
#endif
}

/************************************************************************************
 * Name:  stm32_usbsuspend
 *
 * Description:
 *   Board logic must provide the stm32_usbsuspend logic if the USBDEV driver is
 *   used.  This function is called whenever the USB enters or leaves suspend mode.
 *   This is an opportunity for the board logic to shutdown clocks, power, etc.
 *   while the USB is suspended.
 *
 ************************************************************************************/
__EXPORT void stm32_usbsuspend(FAR struct usbdev_s *dev, bool resume)
{
	uinfo("resume: %d\n", resume);
}

/************************************************************************************
 * 新增：Bootloader 专用 USB 操作函数（完全适配PX4/STM32原生接口）
 ************************************************************************************/

/**
 * @brief Bootloader专用：检测USB VBUS是否连接
 * @return 1: USB已连接（VBUS有效）  0: USB未连接
 */
__EXPORT uint8_t USB_Connect_Detect(void)
{
#ifdef CONFIG_STM32H7_OTGFS
    // 使用PX4原生VBUS GPIO检测（复用原版配置）
    return stm32_gpioread(GPIO_OTGFS_VBUS) ? 1 : 0;
#else
    // 无OTG FS配置时，默认返回未连接
    return 0;
#endif
}

/**
 * @brief Bootloader专用：初始化USB硬件（仅调用原版函数，无驱动初始化）
 * @return 0: 成功  -1: 失败
 */
__EXPORT int USB_Init_Device(void)
{
    // 避免重复初始化
    if (g_usb_initialized) {
        return 0;
    }

    // 调用原版USB GPIO初始化函数（唯一正确的初始化方式）
    stm32_usbinitialize();
    g_usb_initialized = true;

    return 0;
}

/**
 * @brief Bootloader专用：USB数据接收占位函数（简化版，无驱动依赖）
 * @param buf 接收缓冲区
 * @param len 期望接收长度
 * @return 实际接收字节数（Bootloader阶段暂返回0，后续按需扩展）
 */
__EXPORT uint32_t USB_Recv_Firmware(uint8_t *buf, uint32_t len)
{
    // 入参合法性检查
    if (buf == NULL || len == 0) {
        return 0;
    }

    // 检测USB是否连接
    if (USB_Connect_Detect() == 0 || !g_usb_initialized) {
        return 0;
    }

    // Bootloader阶段简化实现：暂返回0（后续可基于PX4 CDC-ACM扩展）
    return 0;
}
