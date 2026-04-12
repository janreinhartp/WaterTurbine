# Usage Guide

This guide explains how to build, flash, run, and extend the WaterTurbine firmware.

## 1. Prerequisites

- ESP-IDF v5.4.3 installed and exported in your shell
- Target board connected and recognized by serial port
- VS Code with ESP-IDF extension (recommended)
- FAT32-formatted SD card inserted in the board slot

## 2. Build and Flash

### Option A: VS Code ESP-IDF Extension (Recommended)

Use command palette:

1. `ESP-IDF: Set Espressif Device Target` -> esp32p4
2. `ESP-IDF: Build your project`
3. `ESP-IDF: Flash your project`
4. `ESP-IDF: Monitor your device`

### Option B: Terminal Commands

From project root:

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your COM port (example: `COM5`).

## 3. Runtime Startup Sequence

At boot, the app initializes hardware in this order:

1. Initialize LDO channels (LDO3=2.5V, LDO4=3.3V)
2. Initialize I2C bus (GPIO45 SDA, GPIO46 SCL, 400kHz)
3. Initialize GT911 touch panel
4. Initialize display + LVGL port
5. Turn on LCD backlight (100% brightness)
6. Initialize extra GPIO (GPIO48 LED, off)
7. Initialize sensor subsystem (INA219 + default 2-point calibration)
8. Initialize pulse controller (flow rate GPIO4, RPM GPIO5)
9. Initialize DS3231 RTC
10. Initialize and mount SD card
11. Initialize UI screens and load Main Menu
12. Start gauges controller task (1-second update loop)
13. Start data logger task (1-second CSV logging)

If any step fails, the app halts and logs the failing module name in a loop.

## 4. How To Use the Current UI

### Main Menu

- Tap GAUGES to open gauge dashboard
- Tap DATA to open chart and numeric metrics
- Tap SETTINGS to open settings page

### Gauges

- Shows 5 arc gauges (Voltage, Ampere, Speed, Power, Flow)
- Voltage and Ampere read from INA219 current/power monitor with 2-point calibration
- Flow Rate read from YF-DN50 sensor via hardware pulse counter (GPIO4)
- RPM read from hall sensor via hardware pulse counter (GPIO5)
- Power computed as Voltage x Ampere
- All gauges update every 1 second
- BACK returns to Main Menu
- DATA opens Data page

### Data

- Shows a live line chart with 4 series (Voltage, Ampere, Flow Rate, RPM) in a 20-point rolling window
- Five metric summary labels (Flow Rate, Voltage, Ampere, Power, RPM) and a time label (RTC) update every 1 second
- BACK returns to Main Menu

### Settings

- Flow Rate Calibration label with numeric text box and on-screen numeric keyboard
- SAVE returns to Main Menu with a 500ms fade animation
- NEXT and BACK buttons exist but are currently hidden

## 5. How Values Are Updated at Runtime

The `gauges_controller` module (`main/gauges_controller.c`) runs a FreeRTOS task at priority 2, stack 8192 bytes, that:

1. Reads voltage and ampere from the INA219 via the sensor module (with 2-point calibration applied).
2. Reads flow rate and RPM from pulse counter sensors via the pulse controller.
3. Computes Power = Voltage x Ampere.
4. Updates gauge arcs (percentage) and value labels on the Gauges screen.
5. Pushes new data points to the line chart and updates metric labels on the Data screen.
6. Repeats every 1 second.

All LVGL calls are wrapped in `lvgl_port_lock()` / `lvgl_port_unlock()` for thread safety.

### Data Logger

The `data_logger` module (`main/data_logger.c`) runs a separate FreeRTOS task at priority 5, stack 8192 bytes, pinned to core 1, that:

1. Reads the current time from the DS3231 RTC.
2. Reads all sensor values (voltage, ampere, flow rate, RPM).
3. Computes Power = Voltage x Ampere.
4. Logs the row to serial console.
5. Appends a CSV row to `/sdcard/log.csv` with all values (if SD card is mounted).
6. Repeats every 1 second.

The CSV header (`Timestamp,Voltage,Ampere,Power,RPM,Flow`) is written automatically when the file does not exist. Time format: `d-mmm-yyyy h:mm:ss` (e.g. `21-Mar-2026 14:35:07`).

### Calibration

Voltage and Ampere use 2-point linear calibration defined in `main/sensor.c`.
Default calibration is set in `system_init()` in `main.c`:

```c
// Voltage: INA219 bus voltage maps directly (0V->0V, 32V->32V)
sensor_cal_t voltage_cal = { .raw1 = 0.0f, .actual1 = 0.0f,
                             .raw2 = 32.0f, .actual2 = 32.0f };
sensor_set_voltage_cal(&voltage_cal);

// Ampere: INA219 current register maps directly (0A->0A, 3.2A->3.2A)
sensor_cal_t ampere_cal = { .raw1 = 0.0f, .actual1 = 0.0f,
                            .raw2 = 3.2f, .actual2 = 3.2f };
sensor_set_ampere_cal(&ampere_cal);
```

The INA219 reads bus voltage and current directly (via a 0.1 Ω shunt resistor), so the default calibration is 1:1 (identity). Adjust if your hardware requires external scaling.

### Flow Rate & RPM

Flow rate and RPM use hardware pulse counters (PCNT peripheral) configured in `main/pulse_controller.c`.

- **YF-DN50 flow sensor** (GPIO4): Default factor = 3.5 Hz per L/min. Adjust with `pulse_controller_set_flow_factor()`.
- **Hall sensor** (GPIO5): Default = 1 pulse per revolution. Adjust with `pulse_controller_set_rpm_ppr()`.

### DS3231 RTC

The RTC keeps time with battery backup. To set the initial time:

```c
ds3231_time_t t = {
    .year = 2026, .month = 3, .date = 21,
    .hours = 14, .minutes = 30, .seconds = 0,
    .day = 6  // Saturday
};
ds3231_set_time(&t);
```

The data logger reads time automatically via `ds3231_get_time()`.

## 6. How To Extend the Project

1. **Add a new sensor:** Create a BSP driver in `peripheral/` following the bsp_ina219 or bsp_ds3231 pattern. Add to `main/CMakeLists.txt` REQUIRES.
2. **Add a new application module:** Create `.c` in `main/` and `.h` in `main/include/`. The CMake GLOB picks it up automatically.
3. **Integrate into UI updates:** Modify `update_values()` in `gauges_controller.c` to read your new sensor and update UI widgets.
4. **Integrate into data logging:** Modify `logger_task()` in `data_logger.c` to include new data columns.
5. **Add settings persistence:** Wire the Settings screen buttons to NVS read/write for calibration values.

## 7. Troubleshooting

- **App stuck printing init error:** The module name in the log indicates the failed initialization stage.
- **No display/backlight:** Check LDO init, display init, and backlight GPIO31 PWM path.
- **No touch response:** Check I2C init and GT911 RST/INT pin wiring.
- **SD card mount failed:** Ensure the card is FAT32 formatted and properly inserted. Check GPIO43/44/39 connections.
- **No CSV data logged:** Ensure DS3231 RTC is responding (check I2C address 0x68) and SD card is mounted.
- **Incorrect sensor values:** Adjust 2-point calibration values in `main.c` or INA219 configuration (shunt resistor value, max current) to match your hardware.
