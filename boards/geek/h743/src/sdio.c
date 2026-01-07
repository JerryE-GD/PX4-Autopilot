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
 * @file sdio.c
 *
 * SDIO driver for PX4 STM32H743 FMU, with Bootloader SD card detection support
 */

#include "hw_config.h"
#include "init.h"  // 引入init.c中的全局句柄hsd
#include "stm32h7xx_hal.h"

#include <nuttx/config.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>
#include <arch/board/board.h>

/**
 * @brief 检测SD卡是否插入
 * @return 1: SD卡已插入  0: SD卡未插入
 */
uint8_t SD_Card_Detect(void)
{
    HAL_SD_CardInfoTypeDef card_info;
    HAL_StatusTypeDef status;

    // 重新初始化SDIO外设（确保状态正确）
    status = HAL_SD_Init(&hsd);
    if (status != HAL_OK)
    {
        return 0;
    }

    // 获取SD卡信息，判断是否存在
    status = HAL_SD_GetCardInfo(&hsd, &card_info);
    if (status == HAL_OK && card_info.CardType != CARD_NONE)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief 从SD卡读取固件数据（预留接口，供后续扩展）
 * @param src_addr SD卡起始地址
 * @param buf 数据缓冲区
 * @param len 读取长度
 * @return 实际读取长度
 */
uint32_t SD_Read_Firmware(uint32_t src_addr, uint8_t *buf, uint32_t len)
{
    HAL_SD_CardInfoTypeDef card_info;
    uint32_t read_len = 0;

    // 检测SD卡是否存在
    if (SD_Card_Detect() == 0)
    {
        return 0;
    }

    // 读取数据（以块为单位，SD卡块大小默认512字节）
    uint32_t block_count = len / 512;
    uint32_t remain_bytes = len % 512;

    if (block_count > 0)
    {
        if (HAL_SD_ReadBlocks(&hsd, buf, src_addr / 512, block_count, HAL_MAX_DELAY) == HAL_OK)
        {
            read_len += block_count * 512;
            src_addr += block_count * 512;
        }
    }

    // 处理剩余字节（简化：忽略不足一个块的部分，后续可优化）
    UNUSED(remain_bytes);

    return read_len;
}

// PX4原有SDIO驱动函数（保留，不影响应用程序）
int stm32_sdio_initialize(void)
{
    static bool initialized = false;

    if (initialized)
    {
        return OK;
    }

    // 调用HAL库初始化SDIO
    if (HAL_SD_Init(&hsd) != HAL_OK)
    {
        return ERROR;
    }

    initialized = true;
    return OK;
}
