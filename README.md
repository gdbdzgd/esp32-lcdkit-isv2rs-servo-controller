# Knob Panel - ESP32-C3 伺服电机控制器

[![Build Status](https://github.com/gdzhang/knob_panel/workflows/Build/badge.svg)](https://github.com/gdzhang/knob_panel/actions)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.1+-orange.svg)](https://docs.espressif.com/projects/esp-idf/)

[中文文档](#中文文档) | [English Documentation](#english-documentation)

---

## 中文文档

### 项目简介

基于 ESP32-C3-LCDkit 开发板的智能伺服电机控制器，配备圆形 LCD 显示屏和旋转编码器，实现对 iSV2-RS6040 伺服电机的直观控制和状态监控。

**主要特性：**

- 🎛️ **旋转编码器控制**：通过旋转编码器直观调节电机转速和参数
- 📊 **圆形 LCD 显示**：240x240 圆形屏幕实时显示转速、状态和方向
- 🚀 **多种控制模式**：支持速度模式、位置模式和 PR 模式
- 🔊 **音频反馈**：内置 NS4150 功放提供按键音效
- 💡 **LED 状态指示**：WS2812 RGB LED 显示运行状态
- 💾 **参数持久化**：配置参数保存到 NVS Flash
- 🔄 **Modbus RTU 通信**：通过 RS485 与伺服驱动器通信

### 硬件要求

| 组件 | 规格 |
|------|------|
| **开发板** | ESP32-C3-LCDkit (ESP32-C3-MINI-1, 4MB Flash) |
| **显示屏** | GC9A01 240x240 圆形 LCD (SPI 接口) |
| **编码器** | EC11 旋转编码器 + 按键 |
| **通信** | RS485 (Modbus RTU) |
| **音频** | NS4150 功放 (GPIO3) |
| **LED** | WS2812 RGB LED (GPIO8) |

### 引脚定义

#### 编码器
- GPIO10: 编码器 A 相
- GPIO6: 编码器 B 相  
- GPIO9: 编码器按键

#### Modbus RS485
- GPIO21: TX (DI)
- GPIO20: RX (RO)

### 软件架构

```
knob_panel/
├── main/
│   ├── app_main.c              # 主程序入口
│   ├── servo/
│   │   ├── isv2rs6040.c/h      # Modbus 伺服驱动
│   │   ├── servo_config.c/h    # NVS 配置存储
│   │   ├── servo_audio.c/h     # 音频驱动 (NS4150)
│   │   └── servo_led.c/h       # LED 驱动 (WS2812)
│   └── ui/
│       ├── ui_servo_main.c/h   # 主界面
│       ├── ui_servo_menu.c/h   # 设置菜单
│       └── fonts/              # 字体文件
├── components/
│   └── esp_modbus/             # Modbus 组件
├── CMakeLists.txt
├── sdkconfig.defaults          # ESP-IDF 默认配置
└── partitions.csv              # 分区表
```

### 功能说明

#### 主界面

- **圆弧转速表**：显示目标转速 (0-3000 RPM)
- **状态指示**：
  - 灰色：停止状态
  - 绿色：运行状态
  - 红色：报警状态
- **方向指示**：
  - 蓝色：正转
  - 橙色：反转
- **报警显示**：当有报警时显示报警码

#### 设置菜单

| 菜单项 | 选项 |
|--------|------|
| 速度步进 | 10/50/100/200/500 RPM |
| 旋转方向 | 正转/反转 |
| 默认转速 | 0-3000 RPM |
| 伺服地址 | 1-247 |
| 波特率 | 1200/2400/4800/9600/19200/38400/57600/115200 |
| 控制模式 | 速度/位置/PR模式 |
| 脉冲比例 | 1-100 |
| 位置速度 | 0-1000 RPM (步进10) |
| 音量 | 0-100% |
| 恢复出厂 | 确认后重置 |

#### 操作逻辑

| 操作 | 功能 |
|------|------|
| 编码器旋转 | 调节目标转速（停止时）或手轮模式控制 |
| 短按编码器 | 启动/停止切换 |
| 长按编码器 2s | 进入设置菜单 |
| 菜单中旋转 | 选择菜单项/调节参数 |
| 菜单中短按 | 进入编辑模式 |
| 菜单中长按 | 返回主界面 |

#### 按键提示音

- 短按："滴"
- 长按："滴滴"
- 旋转："滴"

### 构建和烧录

#### 环境准备

1. 安装 ESP-IDF v5.1 或更高版本
   ```bash
   # 安装依赖
   sudo apt-get install git wget flex bison gperf python3 python3-pip \
       python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
       dfu-util libusb-1.0-0
   
   # 克隆 ESP-IDF
   cd ~/esp
   git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32c3
   ```

2. 设置环境变量
   ```bash
   source ~/esp/esp-idf/export.sh
   ```

#### 编译项目

```bash
# 克隆项目
git clone https://github.com/gdzhang/knob_panel.git
cd knob_panel

# 编译
idf.py build

# 烧录
idf.py -p /dev/ttyACM0 flash

# 监控
idf.py -p /dev/ttyACM0 monitor
```

#### 从 GitHub Actions 下载固件

每次提交代码后，GitHub Actions 会自动编译固件。你可以从 Actions 页面下载编译好的固件：

1. 进入 Actions 页面
2. 选择最新的成功构建
3. 下载 Artifacts 中的 `knob_panel-firmware`
4. 解压后使用 `esptool` 烧录：
   ```bash
   esptool.py -p /dev/ttyACM0 -b 460800 write_flash --flash_mode qio 0x0 knob_panel.bin
   ```

### Modbus 寄存器

| 地址 | 功能 | 读/写 |
|------|------|-------|
| 0x0609 | JOG 速度设置 | R/W |
| 0x1001 | 实际转速 | R |
| 0x1801 | 控制字 | R/W |
| 0x0405 | 软件使能 | R/W |
| 0x1901 | 状态字 | R |
| 0x2203 | 报警码 | R |

**控制字定义：**
- `0x4001`: 正转 (JOG+)
- `0x4002`: 反转 (JOG-)
- `0x0000`: 停止
- `0x1111`: 清除报警

### 常见问题

#### 1. 程序下载失败

**解决方法：**
1. 确保开发板已正确连接
2. 进入下载模式：
   - 按住旋钮按键
   - 短按复位按键
   - 松开旋钮按键
3. 重新上传程序

#### 2. 编码器操作无反应

**检查：**
- 编码器 GPIO 配置是否正确
- NVS 配置是否已初始化
- 尝试恢复出厂设置（长按旋钮 5 秒）

#### 3. 伺服电机不响应

**检查：**
- RS485 接线是否正确
- 伺服驱动器地址是否匹配（默认 16）
- 波特率是否一致（默认 9600）
- 查看 Log 输出中的 Modbus 通信日志

### 开发计划

- [ ] 处理 Modbus 通信错误重试
- [ ] 加减速时间设置
- [ ] 菜单灯光效果配置
- [ ] PR 模式位置控制（Modbus 发送位置指令）

### 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 提交 Pull Request

### 许可证

本项目采用 GNU General Public License v3.0 许可证 - 详见 [LICENSE](LICENSE) 文件

### 致谢

- [ESP-IDF](https://github.com/espressif/esp-idf) - Espressif IoT Development Framework
- [LVGL](https://lvgl.io/) - Light and Versatile Graphics Library
- [ESP32-C3-LCDkit](https://github.com/espressif/esp-dev-kits) - ESP32-C3 LCD 开发板

---

## English Documentation

### Project Overview

An intelligent servo motor controller based on ESP32-C3-LCDkit development board, equipped with a circular LCD display and rotary encoder for intuitive control and status monitoring of iSV2-RS6040 servo motors.

**Key Features:**

- 🎛️ **Rotary Encoder Control**: Intuitively adjust motor speed and parameters via rotary encoder
- 📊 **Circular LCD Display**: 240x240 circular screen for real-time speed, status, and direction display
- 🚀 **Multiple Control Modes**: Supports speed mode, position mode, and PR mode
- 🔊 **Audio Feedback**: Built-in NS4150 amplifier provides key sound effects
- 💡 **LED Status Indicator**: WS2812 RGB LED shows operation status
- 💾 **Parameter Persistence**: Configuration parameters saved to NVS Flash
- 🔄 **Modbus RTU Communication**: Communicates with servo drive via RS485

### Hardware Requirements

| Component | Specification |
|-----------|---------------|
| **Dev Board** | ESP32-C3-LCDkit (ESP32-C3-MINI-1, 4MB Flash) |
| **Display** | GC9A01 240x240 Circular LCD (SPI Interface) |
| **Encoder** | EC11 Rotary Encoder + Button |
| **Communication** | RS485 (Modbus RTU) |
| **Audio** | NS4150 Amplifier (GPIO3) |
| **LED** | WS2812 RGB LED (GPIO8) |

### Pin Definition

#### Encoder
- GPIO10: Encoder Phase A
- GPIO6: Encoder Phase B
- GPIO9: Encoder Button

#### Modbus RS485
- GPIO21: TX (DI)
- GPIO20: RX (RO)

### Software Architecture

```
knob_panel/
├── main/
│   ├── app_main.c              # Main program entry
│   ├── servo/
│   │   ├── isv2rs6040.c/h      # Modbus servo driver
│   │   ├── servo_config.c/h    # NVS configuration storage
│   │   ├── servo_audio.c/h     # Audio driver (NS4150)
│   │   └── servo_led.c/h       # LED driver (WS2812)
│   └── ui/
│       ├── ui_servo_main.c/h   # Main interface
│       ├── ui_servo_menu.c/h   # Settings menu
│       └── fonts/              # Font files
├── components/
│   └── esp_modbus/             # Modbus component
├── CMakeLists.txt
├── sdkconfig.defaults          # ESP-IDF default configuration
└── partitions.csv              # Partition table
```

### Features

#### Main Interface

- **Arc Speedometer**: Displays target speed (0-3000 RPM)
- **Status Indicator**:
  - Gray: Stopped
  - Green: Running
  - Red: Alarm
- **Direction Indicator**:
  - Blue: Forward
  - Orange: Backward
- **Alarm Display**: Shows alarm code when triggered

#### Settings Menu

| Menu Item | Options |
|-----------|---------|
| Speed Step | 10/50/100/200/500 RPM |
| Rotation Direction | Forward/Backward |
| Default Speed | 0-3000 RPM |
| Servo Address | 1-247 |
| Baud Rate | 1200/2400/4800/9600/19200/38400/57600/115200 |
| Control Mode | Speed/Position/PR Mode |
| Pulse Ratio | 1-100 |
| Position Speed | 0-1000 RPM (step 10) |
| Volume | 0-100% |
| Factory Reset | Confirm to reset |

#### Operation Logic

| Operation | Function |
|-----------|----------|
| Encoder Rotation | Adjust target speed (when stopped) or handwheel mode control |
| Short Press | Start/Stop toggle |
| Long Press 2s | Enter settings menu |
| Rotate in Menu | Select menu item / Adjust parameter |
| Short Press in Menu | Enter edit mode |
| Long Press in Menu | Return to main interface |

#### Key Sound Effects

- Short press: Single beep
- Long press: Double beep
- Rotation: Single beep

### Build and Flash

#### Environment Setup

1. Install ESP-IDF v5.1 or later
   ```bash
   # Install dependencies
   sudo apt-get install git wget flex bison gperf python3 python3-pip \
       python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
       dfu-util libusb-1.0-0
   
   # Clone ESP-IDF
   cd ~/esp
   git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32c3
   ```

2. Set environment variables
   ```bash
   source ~/esp/esp-idf/export.sh
   ```

#### Build Project

```bash
# Clone project
git clone https://github.com/gdzhang/knob_panel.git
cd knob_panel

# Build
idf.py build

# Flash
idf.py -p /dev/ttyACM0 flash

# Monitor
idf.py -p /dev/ttyACM0 monitor
```

#### Download Firmware from GitHub Actions

After each code commit, GitHub Actions automatically builds the firmware. You can download the compiled firmware from the Actions page:

1. Go to the Actions page
2. Select the latest successful build
3. Download `knob_panel-firmware` from Artifacts
4. After extraction, use `esptool` to flash:
   ```bash
   esptool.py -p /dev/ttyACM0 -b 460800 write_flash --flash_mode qio 0x0 knob_panel.bin
   ```

### Modbus Registers

| Address | Function | R/W |
|---------|----------|-----|
| 0x0609 | JOG Speed Setting | R/W |
| 0x1001 | Actual Speed | R |
| 0x1801 | Control Word | R/W |
| 0x0405 | Software Enable | R/W |
| 0x1901 | Status Word | R |
| 0x2203 | Alarm Code | R |

**Control Word Definitions:**
- `0x4001`: Forward (JOG+)
- `0x4002`: Backward (JOG-)
- `0x0000`: Stop
- `0x1111`: Clear Alarm

### Troubleshooting

#### 1. Program Upload Failure

**Solution:**
1. Ensure the development board is properly connected
2. Enter download mode:
   - Hold the knob button
   - Press the reset button briefly
   - Release the knob button
3. Re-upload the program

#### 2. Encoder Not Responding

**Check:**
- Whether encoder GPIO configuration is correct
- Whether NVS configuration is initialized
- Try factory reset (long press knob for 5 seconds)

#### 3. Servo Motor Not Responding

**Check:**
- Whether RS485 wiring is correct
- Whether servo drive address matches (default 16)
- Whether baud rate is consistent (default 9600)
- Check Modbus communication logs in output

### Roadmap

- [ ] Handle Modbus communication error retry
- [ ] Acceleration/deceleration time setting
- [ ] Menu lighting effect configuration
- [ ] PR mode position control (Modbus send position command)

### Contributing

Issues and Pull Requests are welcome!

1. Fork this repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Submit a Pull Request

### License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details

### Acknowledgments

- [ESP-IDF](https://github.com/espressif/esp-idf) - Espressif IoT Development Framework
- [LVGL](https://lvgl.io/) - Light and Versatile Graphics Library
- [ESP32-C3-LCDkit](https://github.com/espressif/esp-dev-kits) - ESP32-C3 LCD Development Board

---

## Support

For technical queries, please visit [esp32.com](https://esp32.com/) forum or create a [GitHub Issue](https://github.com/gdzhang/knob_panel/issues)

**Author:** gdbdzgd@gmail.com

**Project Link:** [https://github.com/gdzhang/knob_panel](https://github.com/gdzhang/knob_panel)