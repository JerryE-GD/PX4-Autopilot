/****************************************************************************
 *
 *   Copyright (c) 2021 PX4 Development Team. All rights reserved.
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
 * @file bootloader_main.c
 *
 * FMU-specific early startup code for bootloader
*/

#include "board_config.h"
#include "bl.h"
#include "hw_config.h"  // 新增：引入硬件配置
#include "init.c"       // 新增：引入你写的初始化/串口输出函数
#include <stdint.h>     // 新增：兼容串口输出的类型定义
#include <nuttx/config.h>
#include <nuttx/board.h>
#include <chip.h>
#include <stm32_uart.h>
#include <arch/board/board.h>
#include "arm_internal.h"
#include <px4_platform_common/init.h>

// 新增：前置声明（外设检测+FLASH操作）
uint8_t SD_Card_Detect(void);
uint8_t USB_Device_Detect(void);
uint8_t FLASH_Erase_App_Area(void);
uint8_t FLASH_Write_Firmware(uint32_t addr, uint8_t *buf, uint32_t len);

// 新增：函数指针（用于跳转到PX4应用程序）
typedef void (*pFunction)(void);
pFunction JumpToApplication;

extern int sercon_main(int c, char **argv);

__EXPORT void board_on_reset(int status) {}

__EXPORT void stm32_boardinitialize(void)
{
	/* configure USB interfaces */
	stm32_usbinitialize();
}

__EXPORT int board_app_initialize(uintptr_t arg)
{
    hw_config_init(); 
    // 新增：Bootloader核心逻辑（初始化→检测→升级→跳转）
    uint8_t usb_connected = USB_Device_Detect();  // 检测USB连接
    uint8_t sd_present = SD_Card_Detect();        // 检测SD卡插入

    // 固件升级逻辑
    if (usb_connected || sd_present) {
        // 擦除应用程序区域
        if (FLASH_Erase_App_Area()) {
            uint8_t firmware_buf[1024] = {0};
            uint32_t firmware_len = 0;

            // 从USB/SD卡读取固件（预留接口）
            if (usb_connected) {
                // firmware_len = USB_Receive_Firmware(firmware_buf, sizeof(firmware_buf));
            } else if (sd_present) {
                // firmware_len = SD_Read_Firmware(0x00, firmware_buf, sizeof(firmware_buf));
            }

            // 写入固件到FLASH
            if (firmware_len > 0) {
                FLASH_Write_Firmware(APPLICATION_BASE, firmware_buf, firmware_len);
            }
        }
    }

    // 跳转到PX4应用程序（STM32标准流程）
    if (((*(__IO uint32_t*)APPLICATION_BASE) & 0x2FFE0000) == 0x20000000) {
        JumpToApplication = (pFunction)(*(__IO uint32_t*)(APPLICATION_BASE + 4));
        __set_MSP(*(__IO uint32_t*)APPLICATION_BASE);
        JumpToApplication();
    }

	return 0;
}

void board_late_initialize(void)
{
	sercon_main(0, NULL);
}

extern void sys_tick_handler(void);
void board_timerhook(void)
{
	sys_tick_handler();
}
