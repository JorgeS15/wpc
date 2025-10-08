# Water Pressure Controller (WPC)

An intelligent water pressure monitoring and control system for well pumps, built on ESP32 and designed for seamless Home Assistant integration.

## Installation

See the [Installation Guide](INSTALLATION.md) for complete setup instructions.

## Overview

The Water Pressure Controller automatically regulates water pressure from well systems, providing real-time monitoring, safety features, and smart home integration. Built with ESP32 and programmed using Arduino IDE, this project offers a reliable and upgradable solution for managing well water systems.

![image](https://github.com/user-attachments/assets/4f81e516-7991-4bd9-b226-5a1823c93b5f)

## Features

- **Real-time Monitoring**
  - Water pressure tracking
  - Temperature measurement
  - Flow rate monitoring

- **Home Automation**
  - Home Assistant integration via MQTT
  - Remote monitoring and control
  - Status notifications

- **Safety & Protection**
  - Automatic error detection
  - Leak detection with automatic motor shutoff
  - Prevents pump damage and water waste

- **Easy Maintenance**
  - Over-The-Air (OTA) updates
  - No need for physical access to update firmware
  - Organized filesystem structure

- **Developer Friendly**
  - Clean, organized codebase
  - Modular filesystem architecture
  - Easily upgradable and customizable

<img width="834" height="762" alt="mqtt" src="https://github.com/user-attachments/assets/6f5ee53e-3310-4821-934c-1fdc29e39601" />

## Hardware Requirements

- ESP32 development board
- Pressure sensor (compatible with ESP32)
- Temperature sensor
- Flow sensor
- Relay module (for motor control)
- Well pump/motor
- Power supply

## Software Requirements

- Arduino IDE
- ESP32 board support for Arduino
- Required libraries (see `platformio.ini` or source files for dependencies)
- MQTT broker (for Home Assistant integration)

## Usage

Once installed and configured:

1. The system will automatically monitor water pressure, temperature, and flow
2. The pump motor will be controlled based on pressure thresholds
3. If a leak or error is detected, the motor will automatically shut off
4. Monitor all readings through Home Assistant dashboard
5. Update firmware remotely using OTA when updates are available

## Safety Features

- **Automatic Shutoff**: Motor stops immediately when leaks are detected
- **Pressure Monitoring**: Prevents over-pressurization
- **Error Detection**: Identifies and responds to system anomalies
- **Fail-safe Operation**: Default behavior ensures system safety

## License

This project is open source. Please check the repository for license information.


## Support

For issues, questions, or suggestions, please open an issue on the GitHub repository.

---

**Project Status**: Not in active development

**Maintainer**: [JorgeS15](https://github.com/JorgeS15)
