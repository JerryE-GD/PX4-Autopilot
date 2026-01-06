# boards/geek/h743/default.cmake
# 板级编译核心配置（适配STM32H743）
set(CONFIG_BOARD "geek_h743")
set(CONFIG_MCU "stm32h743xx")
set(CONFIG_ARCH "cortex-m7")
set(CONFIG_TOOLCHAIN "arm-none-eabi")
set(CONFIG_BOARD_VARIANT "geek_h743")

# 时钟配置（和board.h保持一致）
set(HSE_VALUE 25000000UL)
set(SYSCLK_FREQ 280000000UL)

# 启用的核心驱动（和default.px4board匹配）
set(CONFIG_DRIVERS_ADC ON)
set(CONFIG_DRIVERS_PWM_OUT ON)
set(CONFIG_DRIVERS_LED ON)
set(CONFIG_DRIVERS_CAN ON)
set(CONFIG_BOOTLOADER ON)

# 板级源文件（你的硬件配置文件）
list(APPEND BOARD_SRCS
    src/board.h
    src/board_config.h
)

# 复用STM32H743的通用编译规则
include(${PX4_BOARD_DIR}/../common/stm32h7/stm32h7.cmake)

# 编译输出目录（自动生成build/geek_h743_default）
set(CMAKE_BUILD_DIR "${PX4_BINARY_DIR}/geek_h743_default")
