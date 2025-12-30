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
 * @file board.h
 *
 * STM32H743 board core definitions
 */

#pragma once

/****************************************************************************************************
 * Included Files
 ****************************************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

/****************************************************************************************************
 * Definitions
 ****************************************************************************************************/

/* Clock Configuration (STM32H743核心时钟，固定值，不用改) */
#define STM32_HSE_FREQUENCY    25000000ul    /* 外部晶振频率 25MHz */
#define STM32_SYSCLK_FREQUENCY 280000000ul   /* 系统时钟频率 280MHz（H743最大主频） */
#define STM32_HCLK_FREQUENCY   STM32_SYSCLK_FREQUENCY /* AHB总线时钟 */
#define STM32_PCLK1_FREQUENCY  (STM32_HCLK_FREQUENCY / 2)  /* APB1时钟 140MHz */
#define STM32_PCLK2_FREQUENCY  (STM32_HCLK_FREQUENCY / 2)  /* APB2时钟 140MHz */
#define STM32_PCLK3_FREQUENCY  (STM32_HCLK_FREQUENCY / 2)  /* APB3时钟 140MHz */
#define STM32_PCLK4_FREQUENCY  (STM32_HCLK_FREQUENCY / 2)  /* APB4时钟 140MHz */

/* 引脚定义基础（和你board_config.h里的引脚匹配） */
#define GPIO_INPUT              (0x00000000u)
#define GPIO_OUTPUT             (0x00000001u)
#define GPIO_PUSHPULL           (0x00000000u)
#define GPIO_PULLUP             (0x00000008u)
#define GPIO_PULLDOWN           (0x00000010u)
#define GPIO_SPEED_50MHz        (0x00000020u)
#define GPIO_SPEED_100MHz       (0x00000030u)
#define GPIO_OUTPUT_SET         (0x00000040u)

/* 外设基础定义（适配H743） */
#define STM32_ADC1_BASE         0x50040000ul
#define STM32_USART4_BASE       0x40004C00ul  /* 对应RC串口ttyS4 */
#define STM32_TIM8_BASE         0x40013400ul  /* 对应HRT高精度定时器 */

/* 板级标识（自定义为geek_h743） */
#define BOARD_NAME              "geek_h743"
#define BOARD_HW_VERSION        1

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

#ifndef __ASSEMBLY__

/* 时钟初始化函数（H743核心，不用改） */
extern void stm32_clockconfig(void);

#endif /* __ASSEMBLY__ */
