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
 * FMU-specific early startup code. This file implements the
 * board_app_initialize() function that is called early by nsh during startup.
 *
 * Code here is run before the rcS script is invoked; it should start required
 * subsystems and perform board-specific initialisation.
 */

#include "board_config.h"
#include "hw_config.h"

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

// 替换HAL库：PX4原生系统时钟配置（适配STM32H743）
static void SystemClock_Config(void)
{
    // PX4原生RCC配置（替代HAL_RCC_OscConfig/HAL_RCC_ClockConfig）
    // 直接操作寄存器配置480MHz系统时钟（STM32H743）
    uint32_t reg;

    // 1. 启用电源时钟，配置电压缩放
    setbits_reg32(RCC->APB1LENR, RCC_APB1LENR_PWREN);
    modifyreg32(PWR->CR1, PWR_CR1_VOS_MASK, PWR_CR1_VOS_0); // 电压缩放1

    // 2. 启用HSE（外部25MHz晶振）
    setbits_reg32(RCC->CR, RCC_CR_HSEON);
    while ((RCC->CR & RCC_CR_HSERDY) == 0); // 等待HSE就绪

    // 3. 配置PLL（HSE→PLL→480MHz）
    modifyreg32(RCC->PLLCKSELR, RCC_PLLCKSELR_PLLSRC_MASK, 0x01 << RCC_PLLCKSELR_PLLSRC_SHIFT); // PLL源=HSE
    modifyreg32(RCC->PLLCFGR, RCC_PLLCFGR_PLLM_MASK, 5 << RCC_PLLCFGR_PLLM_SHIFT); // PLLM=5
    modifyreg32(RCC->PLL1DIVR, RCC_PLL1DIVR_N1_MASK, 192 << RCC_PLL1DIVR_N1_SHIFT); // PLLN=192
    modifyreg32(RCC->PLL1DIVR, RCC_PLL1DIVR_P1_MASK, 2 << RCC_PLL1DIVR_P1_SHIFT);   // PLLP=2
    modifyreg32(RCC->PLL1DIVR, RCC_PLL1DIVR_Q1_MASK, 8 << RCC_PLL1DIVR_Q1_SHIFT);   // PLLQ=8
    modifyreg32(RCC->PLL1DIVR, RCC_PLL1DIVR_R1_MASK, 2 << RCC_PLL1DIVR_R1_SHIFT);   // PLLR=2

    setbits_reg32(RCC->CR, RCC_CR_PLL1ON); // 启用PLL1
    while ((RCC->CR & RCC_CR_PLL1RDY) == 0); // 等待PLL1就绪

    // 4. 配置系统时钟总线分频
    reg = RCC->CFGR;
    reg &= ~(RCC_CFGR_SW_MASK | RCC_CFGR_HPRE_MASK | RCC_CFGR_PPRE1_MASK | RCC_CFGR_PPRE2_MASK);
    reg |= (0x03 << RCC_CFGR_SW_SHIFT) | // SYSCLK=PLL1
           (0x00 << RCC_CFGR_HPRE_SHIFT) | // AHB=1分频
           (APB1_PRESCALER << RCC_CFGR_PPRE1_SHIFT) | // APB1分频（来自hw_config.h）
           (APB2_PRESCALER << RCC_CFGR_PPRE2_SHIFT); // APB2分频（来自hw_config.h）
    RCC->CFGR = reg;

    // 5. 等待系统时钟切换完成
    while ((RCC->CFGR & RCC_CFGR_SWS_MASK) != (0x03 << RCC_CFGR_SWS_SHIFT));

    // 6. 配置FLASH延迟
    modifyreg32(FLASH->ACR, FLASH_ACR_LATENCY_MASK, 5 << FLASH_ACR_LATENCY_SHIFT);
}

// 替换HAL库：PX4原生USB初始化（替代HAL_PCD_Init）
void USB_Init(void)
{
    // 1. 启用GPIOA和USB OTG FS时钟
    setbits_reg32(RCC->AHB4ENR, RCC_AHB4ENR_GPIOAEN);
    setbits_reg32(RCC->AHB2ENR, RCC_AHB2ENR_OTGFSEN);

    // 2. 配置USB D+/D-引脚（PA11/PA12）为AF10
    px4_gpio_configure(GPIOA, GPIO_PIN_11, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF10_OTG1_FS);
    px4_gpio_configure(GPIOA, GPIO_PIN_12, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF10_OTG1_FS);

    // 3. PX4原生USB初始化（替代HAL_PCD_Init）
    stm32_usbinitialize();
}

// 替换HAL库：PX4原生SDIO初始化（替代HAL_SD_Init）
void SDIO_Init(void)
{
    // 1. 启用GPIOC和SDIO时钟
    setbits_reg32(RCC->AHB4ENR, RCC_AHB4ENR_GPIOCEN);
    setbits_reg32(RCC->AHB2ENR, RCC_AHB2ENR_SDMMC1EN);

    // 2. 配置SDIO引脚（PC8-PC11）为AF12
    px4_gpio_configure(GPIOC, GPIO_PIN_8,  GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);
    px4_gpio_configure(GPIOC, GPIO_PIN_9,  GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);
    px4_gpio_configure(GPIOC, GPIO_PIN_10, GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);
    px4_gpio_configure(GPIOC, GPIO_PIN_11, GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);
    px4_gpio_configure(GPIOC, GPIO_PIN_12, GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);

    // 3. PX4原生SDIO初始化（替代HAL_SD_Init）
    stm32_sdio_initialize();
}

// Bootloader硬件初始化入口（纯PX4原生API）
void HW_Init(void)
{
    SystemClock_Config(); // 配置系统时钟
    USB_Init();           // 初始化USB
    SDIO_Init();          // 初始化SDIO
}

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
