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
#include "hw_config.h"  // 新增：引入Bootloader硬件配置
#include "stm32h7xx_hal.h"  // 新增：HAL库用于USB状态检测
#include <nuttx/usb/usbdev.h>
#include <nuttx/usb/usbdev_trace.h>
#include <stm32_otg.h>
#include <debug.h>

// 新增：全局USB OTG句柄（供检测函数使用）
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

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

	// 新增：初始化USB OTG外设（供Bootloader检测使用）
	HW_Init(); // 调用init.c中的硬件初始化函数
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

/****************************************************************************
 * 新增：Bootloader专用USB检测&固件接收函数
 ****************************************************************************/

/**
 * @brief Bootloader专用：检测USB是否连接到主机
 * @return 1: USB已连接并枚举  0: USB未连接
 */
uint8_t USB_Device_Detect(void)
{
    // 基于STM32 HAL库检测USB OTG状态
    if (&hpcd_USB_OTG_FS == NULL) {
        return 0;
    }

    // PCD_STATE_CONFIGURED：USB已与主机完成枚举和配置
    if (HAL_PCD_GetState(&hpcd_USB_OTG_FS) == PCD_STATE_CONFIGURED) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * @brief Bootloader专用：从USB接收固件数据（预留接口）
 * @param buf  数据缓冲区
 * @param len  期望接收长度
 * @return 实际接收字节数（当前预留为0，后续扩展）
 */
uint32_t USB_Receive_Firmware(uint8_t *buf, uint32_t len)
{
    // 1. 检测USB连接状态
    if (USB_Device_Detect() == 0 || buf == NULL || len == 0) {
        return 0;
    }

    // 2. 预留CDC-ACM接收逻辑（后续对接PX4 CDC-ACM驱动）
    UNUSED(buf);
    UNUSED(len);

    // 暂时返回0，后续补全实际接收代码
    return 0;
}
