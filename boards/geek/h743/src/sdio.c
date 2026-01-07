/****************************************************************************
 *
 *   Copyright (C) 2014, 2016 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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
 * 3. Neither the name NuttX nor the names of its contributors may be
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <board_config.h>
#include "hw_config.h"  // 新增：Bootloader硬件配置

#include <stdbool.h>
#include <stdio.h>
#include <debug.h>
#include <errno.h>

#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>

#include "chip.h"
#include "board_config.h"
#include "stm32_gpio.h"
#include "stm32_sdmmc.h"

#ifdef CONFIG_MMCSD

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Card detections requires card support and a card detection GPIO */

#define HAVE_NCD   1
#if !defined(GPIO_SDMMC1_NCD)
#  undef HAVE_NCD
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct sdio_dev_s *sdio_dev;
#ifdef HAVE_NCD
static bool g_sd_inserted = 0xff; /* Impossible value */
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_ncd_interrupt
 *
 * Description:
 *   Card detect interrupt handler.
 *
 ****************************************************************************/

#ifdef HAVE_NCD
static int stm32_ncd_interrupt(int irq, FAR void *context)
{
	bool present;

	present = !stm32_gpioread(GPIO_SDMMC1_NCD);

	if (sdio_dev && present != g_sd_inserted) {
		sdio_mediachange(sdio_dev, present);
		g_sd_inserted = present;
	}

	return OK;
}
#endif

/****************************************************************************
 * 新增：Bootloader SD卡检测&固件读取函数
 ****************************************************************************/

/**
 * @brief Bootloader专用：检测SD卡是否插入
 * @return 1: SD卡已插入  0: SD卡未插入
 */
uint8_t SD_Card_Detect(void)
{
#ifdef HAVE_NCD
    // 使用PX4原生的卡检测GPIO判断（最可靠）
    return !stm32_gpioread(GPIO_SDMMC1_NCD) ? 1 : 0;
#else
    // 无卡检测GPIO时，默认认为卡已插入（适配无检测引脚的硬件）
    return 1;
#endif
}

/**
 * @brief Bootloader专用：从SD卡读取固件数据
 * @param src_addr SD卡起始地址（块地址，512字节/块）
 * @param buf 数据缓冲区
 * @param len 读取长度（字节）
 * @return 实际读取长度
 */
uint32_t SD_Read_Firmware(uint32_t src_addr, uint8_t *buf, uint32_t len)
{
    // 1. 先检测SD卡是否插入
    if (SD_Card_Detect() == 0) {
        return 0;
    }

    // 2. 确保SDIO驱动已初始化
    if (sdio_dev == NULL) {
        stm32_sdio_initialize(); // 调用原生初始化函数
        if (sdio_dev == NULL) {
            return 0;
        }
    }

    // 3. 按SD卡块大小（512字节）读取数据
    uint32_t block_addr = src_addr / 512;
    uint32_t block_count = len / 512;
    uint32_t remain_bytes = len % 512;
    uint32_t read_len = 0;

    // 读取整块数据
    if (block_count > 0) {
        // 核心修复1：补充sdio_readblocks函数声明（解决隐式声明错误）
        extern int sdio_readblocks(FAR struct sdio_dev_s *dev, uint32_t startblock,
                                   FAR uint8_t *buffer, unsigned int nblocks);
        
        // 核心修复2：调用sdio_readblocks时参数匹配（dev + 块地址 + 缓冲区 + 块数）
        int ret = sdio_readblocks(sdio_dev, block_addr, buf, block_count);
        if (ret == OK) {
            read_len = block_count * 512;
        }
    }

    // 忽略不足1块的剩余字节（简化版，后续可扩展）
    UNUSED(remain_bytes);

    return read_len;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_sdio_initialize
 *
 * Description:
 *   Initialize SDIO-based MMC/SD card support
 *
 ****************************************************************************/

int stm32_sdio_initialize(void)
{
	int ret;

#ifdef HAVE_NCD
	/* Card detect */

	bool cd_status;

	/* Configure the card detect GPIO */

	stm32_configgpio(GPIO_SDMMC1_NCD);

	/* Register an interrupt handler for the card detect pin */

	stm32_gpiosetevent(GPIO_SDMMC1_NCD, true, true, true, stm32_ncd_interrupt);
#endif

	/* Mount the SDIO-based MMC/SD block driver */
	/* First, get an instance of the SDIO interface */

	finfo("Initializing SDIO slot %d\n", SDIO_SLOTNO);

	sdio_dev = sdio_initialize(SDIO_SLOTNO);

	if (!sdio_dev) {
		syslog(LOG_ERR, "[boot] Failed to initialize SDIO slot %d\n", SDIO_SLOTNO);
		return -ENODEV;
	}

	/* Now bind the SDIO interface to the MMC/SD driver */

	finfo("Bind SDIO to the MMC/SD driver, minor=%d\n", SDIO_MINOR);

	ret = mmcsd_slotinitialize(SDIO_MINOR, sdio_dev);

	if (ret != OK) {
		syslog(LOG_ERR, "[boot] Failed to bind SDIO to the MMC/SD driver: %d\n", ret);
		return ret;
	}

	finfo("Successfully bound SDIO to the MMC/SD driver\n");

#ifdef HAVE_NCD
	/* Use SD card detect pin to check if a card is g_sd_inserted */

	cd_status = !stm32_gpioread(GPIO_SDMMC1_NCD);
	finfo("Card detect : %d\n", cd_status);

	sdio_mediachange(sdio_dev, cd_status);
#else
	/* Assume that the SD card is inserted.  What choice do we have? */

	sdio_mediachange(sdio_dev, true);
#endif

	return OK;
}

#endif /* CONFIG_MMCSD */
