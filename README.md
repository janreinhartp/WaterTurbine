# WaterTurbine

Real-time water turbine monitoring system built on the **ESP32-P4** with a 9" touch display, live sensor data, and SD card logging.

## Features

- **Live Dashboard** — 5 arc gauges (Voltage, Ampere, RPM, Power, Flow Rate) updated every 1 second
- **Data Charts** — Rolling 20-point line chart with Voltage, Ampere, Flow Rate, and RPM series
- **Real Sensors** — INA219 current/power monitor for voltage/current, hardware pulse counters for flow and RPM
- **2-Point Calibration** — Linear calibration for voltage and ampere channels, configurable via Settings screen or `#define` constants
- **3 Decimal Precision** — Voltage, ampere, and flow values displayed with 3 decimal places on all screens and SD card logs
- **RTC Clock** — DS3231 real-time clock with battery backup
- **SD Card Logging** — CSV data logged every 1 second with timestamped rows
- **Touch UI** — 4 screens (Main Menu, Gauges, Data, Settings) with LVGL 9.2.2

## Hardware

| Component | Connection | Details |
|---|---|---|
| **Display** | MIPI DSI | 1024x600 IPS (EK79007), backlight on GPIO31 |
| **Touch** | I2C (0x5D) | GT911 capacitive, RST=GPIO40, INT=GPIO42 |
| **INA219 Monitor** | I2C (0x40) | Bus voltage + shunt current, 0.1Ω shunt, 32V/3.2A default |
| **DS3231 RTC** | I2C (0x68) | Battery-backed clock + temperature sensor |
| **YF-DN50 Flow Sensor** | GPIO4 | Pulse output, 3.5 Hz per L/min (default) |
| **Hall Effect RPM Sensor** | GPIO5 | 1 pulse per revolution (default) |
| **SD Card** | SDIO 1-wire | CLK=GPIO43, CMD=GPIO44, D0=GPIO39 |
| **Status LED** | GPIO48 | General purpose output |

### I2C Bus

- SDA: GPIO45
- SCL: GPIO46
- Speed: 400 kHz
- Shared by: GT911 touch (0x5D), INA219 (0x40), DS3231 (0x68)

## Project Structure

```
WaterTurbine/
├── main/
│   ├── main.c                  # App entry point and hardware init
│   ├── gauges_controller.c     # 1-second UI update task
│   ├── settings_controller.c  # 8-page calibration with live raw display
│   ├── sensor.c                # INA219 voltage/ampere with 2-point calibration + raw reads
│   ├── pulse_controller.c      # Flow rate and RPM from PCNT
│   ├── data_logger.c           # CSV logging to SD card (1s interval, skips negative values)
│   ├── include/
│   │   ├── main.h
│   │   ├── gauges_controller.h
│   │   ├── settings_controller.h
│   │   ├── sensor.h
│   │   ├── pulse_controller.h
│   │   └── data_logger.h
│   └── ui/                     # SquareLine Studio generated LVGL screens
│       ├── ui.c / ui.h
│       ├── ui_MainMenu.c
│       ├── ui_Gauges.c
│       ├── ui_Data.c
│       └── ui_Settings.c
├── peripheral/
│   ├── bsp_i2c/                # I2C master bus driver
│   ├── bsp_display/            # GT911 touch driver
│   ├── bsp_illuminate/         # Display (MIPI DSI + LVGL) and backlight
│   ├── bsp_extra/              # GPIO48 LED control
│   ├── bsp_ina219/             # INA219 current/power monitor driver
│   ├── bsp_pcnt/               # Hardware pulse counter wrapper
│   ├── bsp_ds3231/             # DS3231 RTC driver
│   └── bsp_sd/                 # SD card (FAT32) driver
├── documentation/
│   ├── IMPLEMENTED_ITEMS.md    # Detailed implementation reference
│   ├── USAGE_GUIDE.md          # Build, flash, and usage instructions
│   └── LVGL_ELEMENTS.md        # LVGL widget inventory and examples
├── CMakeLists.txt              # Root build configuration
├── partitions.csv              # Partition table (factory + OTA)
└── sdkconfig                   # ESP-IDF project configuration
```

## Getting Started

### Prerequisites

- [ESP-IDF v5.4.3](https://docs.espressif.com/projects/esp-idf/en/v5.4.3/esp32p4/get-started/index.html) installed and configured
- ESP32-P4 board connected via USB
- FAT32-formatted SD card inserted

### Build and Flash

**VS Code (recommended):**

1. Open project in VS Code with ESP-IDF extension
2. `ESP-IDF: Set Espressif Device Target` → esp32p4
3. `ESP-IDF: Build your project`
4. `ESP-IDF: Flash your project`
5. `ESP-IDF: Monitor your device`

**Terminal:**

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> flash monitor
```

## Boot Sequence

The firmware initializes hardware in this order:

1. LDO regulators (LDO3=2.5V, LDO4=3.3V)
2. I2C master bus (GPIO45/46, 400kHz)
3. GT911 touch panel
4. LCD display + LVGL port
5. LCD backlight (100% brightness)
6. GPIO48 LED (off)
7. INA219 sensor subsystem + default calibration from `#define` constants
8. Pulse controller (flow=GPIO4, RPM=GPIO5)
9. DS3231 RTC
10. SD card mount
11. UI initialization
12. Gauges controller task (1-second loop)
13. Data logger task (1-second CSV logging)

Critical init failures halt startup. Non-critical failures are collected and displayed on the Main Menu.

## CSV Data Logging

The data logger writes to `/sdcard/datalog.csv` every 60 seconds:

```csv
Time,Flow Rate,RPM,Voltage,Ampere,Power
21-Mar-2026 14-35,5.300,850.000,24.500,5.100,124.950
21-Mar-2026 14-36,5.280,848.000,24.480,5.090,124.604
```

- **Time format:** `d-mmm-yyyy h-mm`
- **All values:** 3 decimal places
- **Power:** computed as Voltage × Ampere
- Header row is written automatically when a new file is created

## Calibration

### Voltage and Ampere (2-Point Linear)

Default calibration is defined via `#define` constants at the top of `main/sensor.c`:

```c
#define V_RAW_REF_LOW   0.0f    // Raw sensor reading at low point
#define V_REF_LOW       0.0f    // Actual voltage at low point
#define V_RAW_REF_HIGH  33.0f   // Raw sensor reading at high point
#define V_REF_HIGH      32.0f   // Actual voltage at high point

#define A_RAW_REF_LOW   0.0f
#define A_REF_LOW       0.0f
#define A_RAW_REF_HIGH  3.2f
#define A_REF_HIGH      3.2f
```

Calibration can also be adjusted at runtime via the **Settings screen** (8-page workflow with live raw sensor display). Runtime calibration is applied immediately without reboot but does not persist across power cycles.

Raw sensor readings are logged to the serial console every cycle to assist calibration.

### Flow Rate and RPM

- **YF-DN50 flow factor:** Default 3.5 Hz per L/min — adjust with `pulse_controller_set_flow_factor()`
- **RPM pulses per revolution:** Default 1.0 — adjust with `pulse_controller_set_rpm_ppr()`

## Dependencies

Managed via `idf_component.yml`:

| Component | Version |
|---|---|
| espressif/esp_lcd_ek79007 | ^1.0.2 |
| espressif/esp_lvgl_port | ^2.6.0 |
| espressif/esp_lcd_touch_gt911 | ^1.1.3 |
| lvgl/lvgl | 9.2.2 |

## Not Yet Implemented

- Calibration persistence across reboots (NVS)
- CSV log file rotation / size management
- Wi-Fi / Bluetooth connectivity
- OTA firmware updates (partition table ready)

## Documentation

See the [documentation/](documentation/) folder for detailed reference:

- [IMPLEMENTED_ITEMS.md](documentation/IMPLEMENTED_ITEMS.md) — Complete implementation inventory
- [USAGE_GUIDE.md](documentation/USAGE_GUIDE.md) — Build, flash, and extension guide
- [LVGL_ELEMENTS.md](documentation/LVGL_ELEMENTS.md) — LVGL widget reference and examples
