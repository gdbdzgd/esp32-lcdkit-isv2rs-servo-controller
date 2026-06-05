# ESP32-C3 LCDKit iSV2-RS Servo Controller

[![Build Status](https://github.com/gdzhang/esp32-lcdkit-isv2rs-servo-controller/workflows/Build/badge.svg)](https://github.com/gdzhang/esp32-lcdkit-isv2rs-servo-controller/actions)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.1+-orange.svg)](https://docs.espressif.com/projects/esp-idf/)

[中文文档](README-zh_CN.md) | English

---

## Overview

An intelligent servo motor controller based on ESP32-C3-LCDkit development board, equipped with a circular LCD display and rotary encoder for intuitive control and status monitoring of iSV2-RS6040 servo motors.

**Key Features:**

- 🎛️ **Rotary Encoder Control**: Intuitively adjust motor speed and parameters via rotary encoder
- 📊 **Circular LCD Display**: 240x240 circular screen for real-time speed, status, and direction display
- 🚀 **Multiple Control Modes**: Supports speed mode, position mode, and PR mode
- 🔊 **Audio Feedback**: Built-in NS4150 amplifier provides key sound effects
- 💡 **LED Status Indicator**: WS2812 RGB LED shows operation status
- 💾 **Parameter Persistence**: Configuration parameters saved to NVS Flash
- 🔄 **Modbus RTU Communication**: Communicates with servo drive via RS485

## Hardware Requirements

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

## Software Architecture

```
esp32-lcdkit-isv2rs-servo-controller/
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

## Features

### Main Interface

- **Arc Speedometer**: Displays target speed (0-3000 RPM)
- **Status Indicator**:
  - Gray: Stopped
  - Green: Running
  - Red: Alarm
- **Direction Indicator**:
  - Blue: Forward
  - Orange: Backward
- **Alarm Display**: Shows alarm code when triggered

### Settings Menu

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

### Operation Logic

| Operation | Function |
|-----------|----------|
| Encoder Rotation | Adjust target speed (when stopped) or handwheel mode control |
| Short Press | Start/Stop toggle |
| Long Press 2s | Enter settings menu |
| Rotate in Menu | Select menu item / Adjust parameter |
| Short Press in Menu | Enter edit mode |
| Long Press in Menu | Return to main interface |

### Key Sound Effects

- Short press: Single beep
- Long press: Double beep
- Rotation: Single beep

## Build and Flash

### Environment Setup

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

### Build Project

```bash
# Clone project
git clone https://github.com/gdzhang/esp32-lcdkit-isv2rs-servo-controller.git
cd esp32-lcdkit-isv2rs-servo-controller

# Build
idf.py build

# Flash
idf.py -p /dev/ttyACM0 flash

# Monitor
idf.py -p /dev/ttyACM0 monitor
```

### Download Firmware from GitHub Actions

After each code commit, GitHub Actions automatically builds the firmware. You can download the compiled firmware from the Actions page:

1. Go to the Actions page
2. Select the latest successful build
3. Download `esp32-lcdkit-isv2rs-servo-controller-firmware` from Artifacts
4. After extraction, use `esptool` to flash:
   ```bash
   esptool.py -p /dev/ttyACM0 -b 460800 write_flash --flash_mode qio 0x0 esp32-lcdkit-isv2rs-servo-controller.bin
   ```

## Modbus Registers

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

## Troubleshooting

### 1. Program Upload Failure

**Solution:**
1. Ensure the development board is properly connected
2. Enter download mode:
   - Hold the knob button
   - Press the reset button briefly
   - Release the knob button
3. Re-upload the program

### 2. Encoder Not Responding

**Check:**
- Whether encoder GPIO configuration is correct
- Whether NVS configuration is initialized
- Try factory reset (long press knob for 5 seconds)

### 3. Servo Motor Not Responding

**Check:**
- Whether RS485 wiring is correct
- Whether servo drive address matches (default 16)
- Whether baud rate is consistent (default 9600)
- Check Modbus communication logs in output

## Roadmap

- [ ] Handle Modbus communication error retry
- [ ] Acceleration/deceleration time setting
- [ ] Menu lighting effect configuration
- [ ] PR mode position control (Modbus send position command)

## Contributing

Issues and Pull Requests are welcome!

1. Fork this repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Submit a Pull Request

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details

## Acknowledgments

- [ESP-IDF](https://github.com/espressif/esp-idf) - Espressif IoT Development Framework
- [LVGL](https://lvgl.io/) - Light and Versatile Graphics Library
- [ESP32-C3-LCDkit](https://github.com/espressif/esp-dev-kits) - ESP32-C3 LCD Development Board

---

## Support

For technical queries, please visit [esp32.com](https://esp32.com/) forum or create a [GitHub Issue](https://github.com/gdzhang/esp32-lcdkit-isv2rs-servo-controller/issues)

**Author:** gdbdzgd@gmail.com

**Project Link:** [https://github.com/gdzhang/esp32-lcdkit-isv2rs-servo-controller](https://github.com/gdzhang/esp32-lcdkit-isv2rs-servo-controller)