# geek/h743 虚拟飞控板配置说明
## 1. 硬件基础信息
- 芯片型号：STM32H743（cortex-m7）
- 系统时钟：280MHz
- 编译器：arm-none-eabi

## 2. 外设引脚配置
### 2.1 LED
- 红灯：PE5
- 绿灯：PE6
- 蓝灯：PE4

### 2.2 ADC（电池监控）
- 电压采样：PC0（ADC1_CH10）
- 电流采样：PC1（ADC1_CH11）
- 电压分压比：11.0f，电流采样系数：40.0f

### 2.3 PWM
- 输出通道数：10路
- 输入通道数：10路

### 2.4 串口映射
- RC遥控器：/dev/ttyS4（UART4）
- GPS1：/dev/ttyS2
- 数传1：/dev/ttyS0
- 数传2：/dev/ttyS1
- 数传3：/dev/ttyS3
- 数传4：/dev/ttyS6
- URT6：/dev/ttyS5

### 2.5 其他外设
- CAN1：TX=PB9，RX=PB8
- USB VBUS检测：PA8
- SD卡：SDIO_SLOTNO=0
- 高精度定时器：TIM8

## 3. 启用的核心驱动
- ADC电池监控、PWM输出、RC串口、CAN、SDIO
- 气压计（DPS310）、电源监控（INA220/226/228）
- MSP_OSD、DSHOT、MAVLink

## 4. 编译命令
```bash
make geek_h743_default
