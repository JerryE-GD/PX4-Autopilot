/****************************************************************************
 *
 *   Copyright (c) 2021 PX4 Development Team. All rights reserved.
 *
 ****************************************************************************/

/**
 * @file flash.c
 *
 * QEMU模拟环境下的FLASH操作函数（仅验证流程，无真实硬件操作）
 */

#include "hw_config.h"

// 模拟FLASH操作状态（供调试查看）
static bool flash_erased = false;
static bool firmware_written = false;

/**
 * @brief 模拟擦除应用程序区域FLASH（仅标记状态，无真实擦除）
 * @return 1: 擦除成功  0: 擦除失败
 */
uint8_t FLASH_Erase_App_Area(void)
{
    // 模拟擦除操作：仅修改状态标记
    flash_erased = true;
    // 打印调试信息（QEMU中可通过终端查看）
    printf("[Bootloader] 模拟擦除FLASH成功，地址：0x%08X\n", APPLICATION_BASE);
    return 1; // 固定返回成功，保证流程走通
}

/**
 * @brief 模拟写入固件到FLASH（仅标记状态，无真实写入）
 * @param addr 写入地址
 * @param buf  固件数据缓冲区
 * @param len  写入长度
 * @return 1: 写入成功  0: 写入失败
 */
uint8_t FLASH_Write_Firmware(uint32_t addr, uint8_t *buf, uint32_t len)
{
    // 模拟参数校验
    if (buf == NULL || len == 0 || addr < APPLICATION_BASE)
    {
        printf("[Bootloader] 模拟写入FLASH失败：参数错误\n");
        return 0;
    }

    // 模拟写入操作
    firmware_written = true;
    printf("[Bootloader] 模拟写入固件成功：地址0x%08X，长度%lu字节\n", addr, len);
    return 1;
}

// 供调试用：获取FLASH操作状态
bool get_flash_erase_status(void) { return flash_erased; }
bool get_firmware_write_status(void) { return firmware_written; }
