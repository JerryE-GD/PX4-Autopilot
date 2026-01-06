############################################################################
#
#   Copyright (c) 2021 PX4 Development Team. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name PX4 nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.#
############################################################################
# ====================== 新增：geek/h743 核心配置 ======================
# 1. 定义板名（适配geek_h743，和编译指令make geek_h743_default匹配）
set(PX4_BOARD "geek_h743")
set(PX4_BOARD_LABEL "geek_h743")

# 2. 定义MCU和架构（STM32H743必需）
set(MCU "stm32h743xx")
set(ARCH "cortex-m7")

# 3. Flash/RAM配置（Bootloader和应用层共用）
set(FLASH_BASE 0x08000000)          # Flash起始地址
set(FLASH_SIZE 1024K)               # 总Flash大小（H743标准1MB）
set(BOOTLOADER_FLASH_SIZE 16K)      # Bootloader占用前16KB
set(APP_FLASH_START 0x08004000)     # 应用层起始地址（跳过Bootloader）
set(RAM_BASE 0x20000000)            # RAM起始地址
set(RAM_SIZE 128K)                  # SRAM1大小

# 4. STM32H743编译参数（必须，否则编译报错）
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=${ARCH} -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -D${MCU}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcpu=${ARCH} -mthumb -mfloat-abi=hard -mfpu=fpv5-d16")
# =====================================================================
if("${PX4_BOARD_LABEL}" STREQUAL  "bootloader")
	add_library(drivers_board
		bootloader_main.c
		usb.c
	)
	target_link_libraries(drivers_board
		PRIVATE
			nuttx_arch
			nuttx_drivers
			bootloader
	)
	target_include_directories(drivers_board PRIVATE ${PX4_SOURCE_DIR}/platforms/nuttx/src/bootloader/common)

else()
	add_library(drivers_board
		i2c.cpp
		init.c
		led.c
		sdio.c
		spi.cpp
		timer_config.cpp
		usb.c
	)
	add_dependencies(drivers_board arch_board_hw_info)

	target_link_libraries(drivers_board
		PRIVATE
			arch_io_pins
			arch_spi
			arch_board_hw_info
			drivers__led
			nuttx_arch
			nuttx_drivers
			px4_layer
	)
endif()
# 只在编译Bootloader时关联链接脚本
if("${PX4_BOARD_LABEL}" STREQUAL  "bootloader")
        # 指向你手动创建的support/linker.ld（路径要和你的文件对应）
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_CURRENT_SOURCE_DIR}/../support/linker.ld")
endif()

# 新增：geek/h743配置
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_CURRENT_SOURCE_DIR}/../support/linker.ld")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -DSTM32H743xx")
