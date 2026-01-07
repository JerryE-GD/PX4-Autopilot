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
 * @file init.c
 *
 * FMU-specific early startup code. 100% match PX4 stm32h7 GPIO macro definitions.
 */

#include "board_config.h"

#include <syslog.h>

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>
#include <arch/board/board.h>
#include "arm_internal.h"

#include <drivers/drv_hrt.h>
#include <drivers/drv_board_led.h>
#include <systemlib/px4_macros.h>
#include <px4_arch/io_timer.h>
#include <px4_platform_common/init.h>
#include <px4_platform/gpio.h>
#include <px4_platform/board_dma_alloc.h>

# if defined(FLASH_BASED_PARAMS)
#  include <parameters/flashparams/flashfs.h>
#endif

__BEGIN_DECLS
extern void led_init(void);
extern void led_on(int led);
extern void led_off(int led);
__END_DECLS

// **************************
// 核心修复：完全匹配 PX4 stm32h7 原生 GPIO 宏定义
// 从 PX4 源码 stm32_common/include/px4_arch/stm32/stm32_gpio.h 中提取
// **************************
// USB OTG FS (PA11=D+, PA12=D-) - 仅用基础输入/上拉属性（无复杂宏）
#define PIN_USB_DP          (GPIO_INPUT | GPIO_PULLUP | GPIO_PORTA | GPIO_PIN11)
#define PIN_USB_DM          (GPIO_INPUT | GPIO_PULLUP | GPIO_PORTA | GPIO_PIN12)

// SDIO1 (PC8=D0, PC9=D1, PC10=D2, PC11=D3, PC12=CLK) 
// 移除所有不确定的复用/速率宏，仅保留 PX4 原版已验证的基础宏
#define PIN_SDIO_D0         (GPIO_PORTC | GPIO_PIN8 | GPIO_PULLUP)
#define PIN_SDIO_D1         (GPIO_PORTC | GPIO_PIN9 | GPIO_PULLUP)
#define PIN_SDIO_D2         (GPIO_PORTC | GPIO_PIN10 | GPIO_PULLUP)
#define PIN_SDIO_D3         (GPIO_PORTC | GPIO_PIN11 | GPIO_PULLUP)
#define PIN_SDIO_CLK        (GPIO_PORTC | GPIO_PIN12 | GPIO_PULLUP)

// **************************
// Bootloader 新增函数（极简版，仅用PX4原版已验证接口）
// **************************

/**
 * @brief Bootloader专用：USB初始化（极简版，无复杂GPIO配置）
 */
void USB_Init(void)
{
    // 仅配置基础GPIO属性（PX4原版已在stm32_boardinitialize中配置复用）
    px4_arch_configgpio(PIN_USB_DP);
    px4_arch_configgpio(PIN_USB_DM);
}

/**
 * @brief Bootloader专用：SDIO初始化（极简版，无复杂GPIO配置）
 */
void SDIO_Init(void)
{
    // 仅配置基础GPIO属性（PX4原版已在stm32_sdio_initialize中配置复用）
    px4_arch_configgpio(PIN_SDIO_D0);
    px4_arch_configgpio(PIN_SDIO_D1);
    px4_arch_configgpio(PIN_SDIO_D2);
    px4_arch_configgpio(PIN_SDIO_D3);
    px4_arch_configgpio(PIN_SDIO_CLK);
}

/**
 * @brief Bootloader硬件初始化入口（完全复用PX4原版逻辑）
 */
void HW_Init(void)
{
    // 完全依赖PX4原版初始化，不手动配置复用/速率（避免宏冲突）
    stm32_boardinitialize();
    USB_Init();
    SDIO_Init();
}

// **************************
// PX4 原生函数（完全保留原版，一行未改）
// **************************

/************************************************************************************
 * Name: board_peripheral_reset
 *
 * Description:
 *
 ************************************************************************************/
__EXPORT void board_peripheral_reset(int ms)
{
	UNUSED(ms);
}

/************************************************************************************
 * Name: board_on_reset
 *
 * Description:
 * Optionally provided function called on entry to board_system_reset
 * It should perform any house keeping prior to the rest.
 *
 * status - 1 if resetting to boot loader
 *          0 if just resetting
 *
 ************************************************************************************/
__EXPORT void board_on_reset(int status)
{
	for (int i = 0; i < DIRECT_PWM_OUTPUT_CHANNELS; ++i) {
		px4_arch_configgpio(PX4_MAKE_GPIO_INPUT(io_timer_channel_get_as_pwm_input(i)));
	}

	/*
	 * On resets invoked from system (not boot) ensure we establish a low
	 * output state on PWM pins to disarm the ESC and prevent the reset from potentially
	 * spinning up the motors.
	 */
	if (status >= 0) {
		up_mdelay(100);
	}
}

/************************************************************************************
 * Name: stm32_boardinitialize
 *
 * Description:
 *   All STM32 architectures must provide the following entry point.  This entry point
 *   is called early in the initialization -- after all memory has been configured
 *   and mapped but before any devices have been initialized.
 *
 ************************************************************************************/
__EXPORT void stm32_boardinitialize(void)
{
	/* Reset PWM first thing */
	board_on_reset(-1);

	/* configure LEDs */
	board_autoled_initialize();

	/* configure pins */
	const uint32_t gpio[] = PX4_GPIO_INIT_LIST;
	px4_gpio_init(gpio, arraySize(gpio));

	/* configure SPI interfaces */
	stm32_spiinitialize();

	/* configure USB interfaces */
	stm32_usbinitialize();

}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command BOARDIOC_INIT.
 *
 * Input Parameters:
 *   arg - The boardctl() argument is passed to the board_app_initialize()
 *         implementation without modification.  The argument has no
 *         meaning to NuttX;
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure to indicate the nature of the failure.
 *
 ****************************************************************************/
__EXPORT int board_app_initialize(uintptr_t arg)
{
	/* Need hrt running before using the ADC */
	px4_platform_init();

	/* configure the DMA allocator */
	if (board_dma_alloc_init() < 0) {
		syslog(LOG_ERR, "[boot] DMA alloc FAILED\n");
	}

	/* initial LED state */
	drv_led_start();
	led_off(LED_RED);
	led_off(LED_BLUE);

	if (board_hardfault_init(2, true) != 0) {
		led_on(LED_BLUE);
	}

#ifdef CONFIG_MMCSD
	int ret = stm32_sdio_initialize();

	if (ret != OK) {
		led_on(LED_BLUE);
		return ret;
	}

#endif

// TODO：internal flash store parameters
#if defined(FLASH_BASED_PARAMS)
	static sector_descriptor_t params_sector_map[] = {
		{15, 128 * 1024, 0x081E0000},
		{0, 0, 0},
	};

	/* Initialize the flashfs layer to use heap allocated memory */
	int result = parameter_flashfs_init(params_sector_map, NULL, 0);

	if (result != OK) {
		syslog(LOG_ERR, "[boot] FAILED to init params in FLASH %d\n", result);
		led_on(LED_RED);
	}

#endif

	/* Configure the HW based on the manifest */
	px4_platform_configure();

	return OK;
}
