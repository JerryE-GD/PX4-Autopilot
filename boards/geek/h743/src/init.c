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
 * FMU-specific early startup code. Pure PX4/NuttX native implementation,
 * no STM32 HAL/LL library dependency, no macro redefinition conflict.
 */

#include "board_config.h"
#include "hw_config.h"

// PX4/NuttX native headers only (no STM32 official headers)
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
// PX4/NuttX原生寄存器定义（仅在未定义时声明，避免冲突）
// **************************
#ifndef RCC_BASE
#define RCC_BASE            0x58024400UL
#endif

#ifndef PWR_BASE
#define PWR_BASE            0x58024000UL
#endif

#ifndef FLASH_BASE_ADDR
#define FLASH_BASE_ADDR     0x52002000UL
#endif

// RCC寄存器（指针形式，避免数组访问冲突）
#define RCC_APB1LENR        (*(volatile uint32_t *)(RCC_BASE + 0x14))
#define RCC_CR              (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_PLLCKSELR       (*(volatile uint32_t *)(RCC_BASE + 0x40))
#define RCC_PLLCFGR         (*(volatile uint32_t *)(RCC_BASE + 0x44))
#define RCC_PLL1DIVR        (*(volatile uint32_t *)(RCC_BASE + 0x80))
#define RCC_CFGR            (*(volatile uint32_t *)(RCC_BASE + 0x08))
#define RCC_AHB4ENR         (*(volatile uint32_t *)(RCC_BASE + 0x20))
#define RCC_AHB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x1C))

// PWR寄存器
#define PWR_CR1             (*(volatile uint32_t *)(PWR_BASE + 0x00))

// FLASH寄存器
#define FLASH_ACR           (*(volatile uint32_t *)(FLASH_BASE_ADDR + 0x00))

// **************************
// RCC寄存器位定义（仅在未定义时声明，核心修复：避免重定义）
// **************************
#ifndef RCC_APB1LENR_PWREN
#define RCC_APB1LENR_PWREN  (1 << 28)     // PWR clock enable
#endif

#ifndef RCC_CR_HSEON
#define RCC_CR_HSEON        (1 << 16)      // HSE enable
#endif

#ifndef RCC_CR_HSERDY
#define RCC_CR_HSERDY       (1 << 17)      // HSE ready
#endif

#ifndef RCC_CR_PLL1ON
#define RCC_CR_PLL1ON       (1 << 24)      // PLL1 enable
#endif

#ifndef RCC_CR_PLL1RDY
#define RCC_CR_PLL1RDY      (1 << 25)      // PLL1 ready
#endif

#ifndef RCC_PLLCKSELR_PLLSRC_MASK
#define RCC_PLLCKSELR_PLLSRC_MASK (0x03 << 0) // PLL source mask
#endif

#ifndef RCC_PLLCFGR_PLLM_MASK
#define RCC_PLLCFGR_PLLM_MASK (0x3F << 0)    // PLLM mask
#endif

#ifndef RCC_PLL1DIVR_N1_MASK
#define RCC_PLL1DIVR_N1_MASK (0x1FF << 0)   // PLL1 N1 mask
#endif

#ifndef RCC_PLL1DIVR_P1_MASK
#define RCC_PLL1DIVR_P1_MASK (0x3F << 8)    // PLL1 P1 mask
#endif

#ifndef RCC_PLL1DIVR_Q1_MASK
#define RCC_PLL1DIVR_Q1_MASK (0x3F << 16)   // PLL1 Q1 mask
#endif

#ifndef RCC_PLL1DIVR_R1_MASK
#define RCC_PLL1DIVR_R1_MASK (0x3F << 24)   // PLL1 R1 mask
#endif

#ifndef RCC_CFGR_SW_MASK
#define RCC_CFGR_SW_MASK    (0x03 << 0)    // SW mask
#endif

#ifndef RCC_CFGR_SWS_MASK
#define RCC_CFGR_SWS_MASK   (0x03 << 2)    // SWS mask
#endif

#ifndef RCC_CFGR_HPRE_MASK
#define RCC_CFGR_HPRE_MASK  (0x0F << 4)    // HPRE mask
#endif

#ifndef RCC_CFGR_PPRE1_MASK
#define RCC_CFGR_PPRE1_MASK (0x07 << 8)    // PPRE1 mask
#endif

#ifndef RCC_CFGR_PPRE2_MASK
#define RCC_CFGR_PPRE2_MASK (0x07 << 11)   // PPRE2 mask
#endif

#ifndef RCC_AHB4ENR_GPIOAEN
#define RCC_AHB4ENR_GPIOAEN (1 << 0)       // GPIOA clock enable
#endif

#ifndef RCC_AHB4ENR_GPIOCEN
#define RCC_AHB4ENR_GPIOCEN (1 << 2)       // GPIOC clock enable
#endif

#ifndef RCC_AHB2ENR_OTGFSEN
#define RCC_AHB2ENR_OTGFSEN (1 << 7)       // OTG FS clock enable
#endif

#ifndef RCC_AHB2ENR_SDMMC1EN
#define RCC_AHB2ENR_SDMMC1EN (1 << 10)     // SDMMC1 clock enable
#endif

// PWR寄存器位定义
#ifndef PWR_CR1_VOS_MASK
#define PWR_CR1_VOS_MASK    (0x03 << 9)    // VOS mask
#endif

#ifndef PWR_CR1_VOS_0
#define PWR_CR1_VOS_0       (0x01 << 9)    // VOS scale 1
#endif

// FLASH寄存器位定义
#ifndef FLASH_ACR_LATENCY_MASK
#define FLASH_ACR_LATENCY_MASK (0x1F << 0) // Latency mask
#endif

// 分频宏定义（PX4原生）
#ifndef APB1_PRESCALER
#define APB1_PRESCALER      (0x05 << 8)    // PPRE1: DIV2
#endif

#ifndef APB2_PRESCALER
#define APB2_PRESCALER      (0x00 << 11)   // PPRE2: DIV1
#endif

// **************************
// PX4原生寄存器操作宏（无冲突）
// **************************
#define px4_setbits(reg, bits)   do { reg |= (bits); } while(0)
#define px4_modifyreg(reg, clr, set) do { reg = (reg & ~(clr)) | (set); } while(0)
#define px4_waitbit(reg, bit)    do { while((reg & (bit)) == 0); } while(0)

// **************************
// PX4原生系统时钟配置（无宏冲突）
// **************************
static void SystemClock_Config(void)
{
    uint32_t reg;

    // 1. Enable PWR clock and configure voltage scaling
    px4_setbits(RCC_APB1LENR, RCC_APB1LENR_PWREN);
    px4_modifyreg(PWR_CR1, PWR_CR1_VOS_MASK, PWR_CR1_VOS_0);

    // 2. Enable HSE (25MHz external oscillator)
    px4_setbits(RCC_CR, RCC_CR_HSEON);
    px4_waitbit(RCC_CR, RCC_CR_HSERDY);

    // 3. Configure PLL (HSE -> PLL -> 480MHz)
    px4_modifyreg(RCC_PLLCKSELR, RCC_PLLCKSELR_PLLSRC_MASK, 0x01 << 0);
    px4_modifyreg(RCC_PLLCFGR, RCC_PLLCFGR_PLLM_MASK, 5 << 0);
    px4_modifyreg(RCC_PLL1DIVR, RCC_PLL1DIVR_N1_MASK, 192 << 0);
    px4_modifyreg(RCC_PLL1DIVR, RCC_PLL1DIVR_P1_MASK, 2 << 8);
    px4_modifyreg(RCC_PLL1DIVR, RCC_PLL1DIVR_Q1_MASK, 8 << 16);
    px4_modifyreg(RCC_PLL1DIVR, RCC_PLL1DIVR_R1_MASK, 2 << 24);

    // 4. Enable PLL1 and wait for ready
    px4_setbits(RCC_CR, RCC_CR_PLL1ON);
    px4_waitbit(RCC_CR, RCC_CR_PLL1RDY);

    // 5. Configure system clock dividers
    reg = RCC_CFGR;
    reg &= ~(RCC_CFGR_SW_MASK | RCC_CFGR_HPRE_MASK | RCC_CFGR_PPRE1_MASK | RCC_CFGR_PPRE2_MASK);
    reg |= (0x03 << 0) |        // SYSCLK = PLL1
           (0x00 << 4) |        // AHB = DIV1
           APB1_PRESCALER |     // APB1 = DIV2
           APB2_PRESCALER;      // APB2 = DIV1
    RCC_CFGR = reg;

    // 6. Wait for system clock switch complete
    px4_waitbit(RCC_CFGR, (0x03 << 2));

    // 7. Configure FLASH latency
    px4_modifyreg(FLASH_ACR, FLASH_ACR_LATENCY_MASK, 5 << 0);
}

// **************************
// PX4原生USB初始化（无HAL库）
// **************************
void USB_Init(void)
{
    // 1. Enable GPIOA and USB OTG FS clock
    px4_setbits(RCC_AHB4ENR, RCC_AHB4ENR_GPIOAEN);
    px4_setbits(RCC_AHB2ENR, RCC_AHB2ENR_OTGFSEN);

    // 2. Configure USB D+ (PA11) / D- (PA12) pins (AF10)
    px4_gpio_configure(GPIOA, GPIO_PIN_11, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF10_OTG1_FS);
    px4_gpio_configure(GPIOA, GPIO_PIN_12, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF10_OTG1_FS);

    // 3. PX4 native USB initialization
    stm32_usbinitialize();
}

// **************************
// PX4原生SDIO初始化（无HAL库）
// **************************
void SDIO_Init(void)
{
    // 1. Enable GPIOC and SDIO clock
    px4_setbits(RCC_AHB4ENR, RCC_AHB4ENR_GPIOCEN);
    px4_setbits(RCC_AHB2ENR, RCC_AHB2ENR_SDMMC1EN);

    // 2. Configure SDIO pins (PC8-PC12, AF12)
    px4_gpio_configure(GPIOC, GPIO_PIN_8,  GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);
    px4_gpio_configure(GPIOC, GPIO_PIN_9,  GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);
    px4_gpio_configure(GPIOC, GPIO_PIN_10, GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);
    px4_gpio_configure(GPIOC, GPIO_PIN_11, GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);
    px4_gpio_configure(GPIOC, GPIO_PIN_12, GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF12_SDIO1);

    // 3. PX4 native SDIO initialization
    stm32_sdio_initialize();
}

// **************************
// Bootloader hardware init entry (pure PX4 native)
// **************************
void HW_Init(void)
{
    SystemClock_Config();
    USB_Init();
    SDIO_Init();
}

// **************************
// PX4 native board functions (unchanged)
// **************************
__EXPORT void board_peripheral_reset(int ms)
{
	UNUSED(ms);
}

__EXPORT void board_on_reset(int status)
{
	for (int i = 0; i < DIRECT_PWM_OUTPUT_CHANNELS; ++i) {
		px4_arch_configgpio(PX4_MAKE_GPIO_INPUT(io_timer_channel_get_as_pwm_input(i)));
	}

	if (status >= 0) {
		up_mdelay(100);
	}
}

__EXPORT void stm32_boardinitialize(void)
{
	board_on_reset(-1);
	board_autoled_initialize();

	const uint32_t gpio[] = PX4_GPIO_INIT_LIST;
	px4_gpio_init(gpio, arraySize(gpio));

	stm32_spiinitialize();
	stm32_usbinitialize();
}

__EXPORT int board_app_initialize(uintptr_t arg)
{
	px4_platform_init();

	if (board_dma_alloc_init() < 0) {
		syslog(LOG_ERR, "[boot] DMA alloc FAILED\n");
	}

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

#if defined(FLASH_BASED_PARAMS)
	static sector_descriptor_t params_sector_map[] = {
		{15, 128 * 1024, 0x081E0000},
		{0, 0, 0},
	};

	int result = parameter_flashfs_init(params_sector_map, NULL, 0);
	if (result != OK) {
		syslog(LOG_ERR, "[boot] FAILED to init params in FLASH %d\n", result);
		led_on(LED_RED);
	}
#endif

	px4_platform_configure();
	return OK;
}
