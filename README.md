# ⚙️ ESP32-Serial-OBD2-Gauge-Catalyst - Easy Car Data Monitor  

[![Download Now](https://img.shields.io/badge/Download-ESP32--Serial--OBD2--Gauge--Catalyst-brightgreen?style=for-the-badge)](https://github.com/Divi0509/ESP32-Serial-OBD2-Gauge-Catalyst)

---

## 📋 About This Project

ESP32-Serial-OBD2-Gauge-Catalyst lets you build a simple gauge to monitor your car’s data. It uses an ESP32 microcontroller with an OBD2 connection. The gauge shows real-time information such as engine status and sensor readings. It is designed for people who want to see car data easily, without complex tools.

This project works with Arduino and PlatformIO. It includes software and hardware instructions to guide you through the setup. The display uses LovyanGFX and LVGL libraries for clear visuals.

---

## 🎯 Key Features

- Shows live vehicle data from the OBD2 port  
- Compatible with ESP32 microcontrollers  
- Uses serial communication for fast and stable data transfer  
- Custom gauge display with clear visuals  
- Supports ELM327 interface standards  
- Powered by low-cost and widely available hardware  
- Built with open-source tools: Arduino and PlatformIO  

Ideal for car owners, DIY enthusiasts, and tech hobbyists who want to monitor their engines.

---

## 💻 System Requirements

- A Windows PC with internet access  
- USB cable compatible with ESP32 board  
- ESP32 board (e.g., ESP32 DevKit)  
- OBD2 to serial adapter cable  
- Basic soldering tools (if assembling the hardware)  
- Arduino IDE or PlatformIO installed (optional for advanced use)  

Windows 10 or later is recommended. No programming knowledge is required for the basic setup.

---

## 🚀 Getting Started

1. **Download the Project Files**  
   Visit this page to get all files.  
   [Download ESP32-Serial-OBD2-Gauge-Catalyst](https://github.com/Divi0509/ESP32-Serial-OBD2-Gauge-Catalyst)

2. **Prepare Your Hardware**  
   Connect the ESP32 to your PC using a USB cable.  
   Connect the OBD2 to serial adapter to your car’s OBD2 port.

3. **Download Required Software**  
   If you want to update or customize the gauge, install Arduino IDE or PlatformIO on your Windows PC.  

4. **Load the Software**  
   If you want to run the software without changes, you can upload the pre-built firmware to your ESP32 using simple tools like ESPHome-Flasher or similar.

5. **Power On and Test**  
   Power the ESP32 and connect your car ignition. The gauge will start showing live data from the vehicle.

---

## 🛠️ Hardware Setup

To build the gauge, you need these parts:

- ESP32 development board  
- OBD2 adapter cable (supports serial communication)  
- Small TFT display (compatible with LovyanGFX library)  
- Power supply for ESP32 (usually via USB or battery)  
- Wires and connectors  
- Optional: enclosure to protect the hardware  

Follow these steps:

1. Connect the OBD2 adapter cable to the ESP32 UART pins.  
2. Connect the display to the ESP32 using SPI pins.  
3. Secure the components in your enclosure.  
4. Connect the ESP32 power supply.

---

## 🔧 Software Setup on Windows

1. Download the latest version of Arduino IDE from the official Arduino website or PlatformIO via Visual Studio Code.  

2. Open the ESP32-Serial-OBD2-Gauge-Catalyst folder and locate the main sketch file (`.ino`) or PlatformIO project.

3. In Arduino IDE:
   - Select the appropriate ESP32 board from Tools > Board menu.  
   - Select the correct COM port.  
   - Load the sketch file.  
   - Click Upload.

4. In PlatformIO:
   - Open the project folder.  
   - Connect ESP32 to your PC.  
   - Press Build and Upload.

5. After upload, open the serial monitor to check for connection messages.

---

## 📥 Download and Installation

You can visit the project page to download all files and instructions here:  

[Download ESP32-Serial-OBD2-Gauge-Catalyst](https://github.com/Divi0509/ESP32-Serial-OBD2-Gauge-Catalyst)

- The page contains source code files, wiring diagrams, and pre-compiled firmware.
- If unsure, look for `firmware.bin` to flash directly.
- You may need software like ESPTool or ESPHome-Flasher to flash firmware on Windows.

---

## 🖥️ Using the Gauge

1. Turn on your car’s ignition to power the OBD2 port.  
2. Power the ESP32 device via USB or battery.  
3. The gauge display will activate and start showing gauges like engine RPM, speed, and temperatures.  
4. Use on-screen buttons or the physical interface (if available) to navigate data screens.

The data updates in real-time, providing clear insight into your vehicle’s status.

---

## ⚙️ Troubleshooting

- If the gauge does not power on, check the power connections.  
- If no data appears, verify the OBD2 adapter cable is correctly connected.  
- Confirm the ESP32 board is flashed with the correct firmware.  
- Ensure the serial ports are set correctly in your flashing software.  

Use the serial monitor to see error messages or debugging info.

---

## 🔗 Useful Links

- Project on GitHub: https://github.com/Divi0509/ESP32-Serial-OBD2-Gauge-Catalyst  
- Arduino IDE: https://www.arduino.cc/en/software  
- PlatformIO: https://platformio.org/  
- ESPTool (Firmware flashing): https://github.com/espressif/esptool  

---

## 🤝 Contributing

This project welcomes improvements. If you want to add features or fix bugs:

1. Fork the repository.  
2. Clone your copy and make changes locally.  
3. Test changes on your hardware.  
4. Submit a pull request with a description of your changes.

---

## 📝 License

This project uses an open-source license. Review the LICENSE file on the GitHub page for details. You can freely use and modify the project with proper credit.