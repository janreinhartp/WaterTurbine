# Currently Implemented Items

This document summarizes what is implemented in the current codebase.

## 1. System Startup and Hardware Initialization

Implemented in `main/main.c`:

- LDO initialization:
  - LDO3 set to 2500 mV
  - LDO4 set to 3300 mV
- I2C master bus initialization (required before touch init)
- GT911 touch initialization
- LCD display and LVGL port initialization
- LCD backlight enable with brightness set to 100 in app startup flow
- Extra GPIO initialization for an LED output on GPIO48, default state OFF
- UI initialization by calling `ui_init()`
- Sensor subsystem initialization (INA219 for voltage and current)
- NVS flash initialization (with erase-on-corruption recovery)
- Default 2-point calibration defined via `#define` constants in `sensor.c` (used on first boot)
- Saved calibration loaded from NVS on boot (falls back to `#define` defaults if not found)
- Pulse controller initialization (flow rate on GPIO4, RPM on GPIO5)
- DS3231 RTC initialization over I2C
- SD card initialization and mount (1-wire SDIO)
- Gauges controller task started via `gauges_controller_start()`
- CSV data logger task started via `data_logger_start()`

Error handling behavior:

- Critical init failures (LDO, I2C, touch, display) return early from `system_init()`.
- Non-critical init failures (GPIO48, sensor, pulse controller, DS3231, SD card) are collected in a startup error buffer.
- After UI init, startup errors are displayed on the Main Menu `ui_lblErrorStartUp` label in red text.
- If all systems pass, the label shows "All systems OK" in green.

## 2. BSP / Peripheral Modules

### I2C (`peripheral/bsp_i2c`)

Implemented:

- I2C bus initialization (`i2c_init`)
- Device registration (`i2c_dev_register`)
- Generic read/write helpers:
  - `i2c_read`
  - `i2c_write`
  - `i2c_write_read`
  - `i2c_read_reg`
  - `i2c_write_reg`

Configured pins:

- SDA: GPIO45
- SCL: GPIO46
- Port: I2C0

### Touch (`peripheral/bsp_display`)

Implemented:

- GT911 touch init over I2C (`touch_init`)
- Primary + backup GT911 address fallback handling
- Read touch data (`touch_read`)
- Cached coordinate API:
  - `set_coor`
  - `get_coor`

Configured touch pins:

- RST: GPIO40
- INT: GPIO42

### Display / LVGL / Backlight (`peripheral/bsp_illuminate`)

Implemented:

- Backlight PWM init (`blight_init`)
- Backlight brightness control (`set_lcd_blight`)
- MIPI DSI + EK79007 panel init
- LVGL port init and display registration
- Touch input registration into LVGL (`lvgl_port_add_touch`)
- Composite init (`display_init`)

Configured display params:

- Resolution: 1024 x 600
- Color depth: RGB565 (16 bpp)
- Backlight pin: GPIO31

### Extra GPIO (`peripheral/bsp_extra`)

Implemented:

- GPIO48 output initialization (`gpio_extra_init`)
- GPIO48 level set helper (`gpio_extra_set_level`)

### INA219 Current/Power Monitor (`peripheral/bsp_ina219`)

Implemented:

- INA219 initialization over I2C with device reset and calibration register programming (`ina219_init`)
- Bus voltage reading (`ina219_read_bus_voltage`) — voltage across the load in volts
- Shunt voltage reading (`ina219_read_shunt_voltage`) — voltage across shunt resistor in millivolts
- Current reading via hardware current register (`ina219_read_current`) — computed by INA219 from shunt voltage and calibration
- Power reading via hardware power register (`ina219_read_power`) — computed by INA219 from bus voltage and current
- Configurable bus voltage range (16V/32V), PGA gain (±40/80/160/320 mV), ADC resolution (9–12 bit), and averaging (2–128 samples)
- Supports continuous and triggered measurement modes

Default configuration: 32V bus range, ±320 mV shunt range, 12-bit ADC, 0.1 Ω shunt resistor, 3.2A max current

Default I2C address: 0x40 (A0=GND, A1=GND)

### Pulse Counter (`peripheral/bsp_pcnt`)

Implemented:

- Hardware pulse counting via ESP32-P4 PCNT peripheral (`bsp_pcnt_create`)
- Atomic read-and-clear of accumulated pulse count (`bsp_pcnt_read_and_clear`)
- 1µs glitch filter for noise rejection
- Internal pull-up enabled on input pins
- Counts rising edges only, no CPU overhead

Used by both flow rate and RPM sensors.

Note:

- These functions currently always return `ESP_OK` and do not propagate lower-level driver errors.

### DS3231 RTC (`peripheral/bsp_ds3231`)

Implemented:

- DS3231 initialization over I2C (`ds3231_init`)
- Read date/time (`ds3231_get_time`) — seconds, minutes, hours, day-of-week, date, month, year
- Set date/time (`ds3231_set_time`)
- Read on-chip temperature sensor (`ds3231_get_temperature`) — 0.25°C resolution
- BCD ↔ decimal conversion for all time registers
- Clears oscillator-stop flag (OSF) on init if power was lost
- Configures control register for battery-backed operation

I2C address: 0x68 (fixed)

Time format: 24-hour mode, year stored as 2000–2099

### SD Card (`peripheral/bsp_sd`)

Implemented:

- SD card initialization and FAT filesystem mount (`sd_init`)
- Append string to file (`sd_append_string`) — creates file if not existing
- File existence check (`sd_file_exists`)
- 1-wire SDIO mode with internal pull-ups

GPIO pins: CLK=GPIO43, CMD=GPIO44, D0=GPIO39

Mount point: `/sdcard`

Requires FAT32 formatted SD card.

## 3. LVGL UI Screens and Navigation

UI is generated with SquareLine Studio and initialized in `main/ui/ui.c`.

### Main Menu Screen

Implemented in `main/ui/ui_MainMenu.c`:

- Buttons:
  - GAUGES
  - DATA
  - SETTINGS
- App branding/logo area
- Navigation events:
  - Main Menu -> Gauges
  - Main Menu -> Data
  - Main Menu -> Settings

### Gauges Screen

Implemented in `main/ui/ui_Gauges.c`:

- Arc widgets for:
  - Voltage
  - Ampere
  - Speed (RPM)
  - Power
  - Flow
- Value labels and titles for each metric
- Buttons:
  - BACK (to Main Menu)
  - DATA (to Data screen)

Runtime data behavior:

- Voltage and Ampere are read from the INA219 current/power monitor with 2-point calibration applied.
- Flow Rate is read from YF-DN50 sensor via PCNT on GPIO4 (default factor: 3.5 Hz per L/min).
- RPM is read from hall sensor via PCNT on GPIO5 (default: 1 pulse per revolution).
- Power is computed as Voltage × Ampere (real calculation).
- All five gauge arcs and value labels are updated every 1 second by `gauges_controller`.
- Arc values are set as percentages (0–100) of each parameter's range.

### Data Screen

Implemented in `main/ui/ui_Data.c`:

- Line chart (`lv_chart`) with 4 series and dual Y-axes
- Metric panels with labels/values:
  - Flow rate
  - Voltage
  - Ampere
  - Power
  - RPM
  - Time (RTC)
  - RPM
- BACK button to Main Menu

Runtime data behavior:

- The SquareLine-generated chart series are removed and recreated at runtime by `init_data_chart()` in `gauges_controller.c` with LVGL-managed arrays.
- Four line chart series are displayed in a 20-point rolling window:
  - Voltage (red, `#F60707`) on primary Y-axis
  - Ampere (blue, `#0D06E9`) on primary Y-axis
  - Flow Rate (yellow, `#F3E913`) on secondary Y-axis
  - RPM (grey, `#808080`) on secondary Y-axis
- Chart uses shift mode (oldest points removed as new data arrives).
- All five metric labels (Flow Rate, Voltage, Ampere, Power, RPM) and a time label (RTC) update every 1 second.

### Settings Screen

Implemented in `main/ui/ui_Settings.c` and `main/settings_controller.c`:

- Settings title and container
- Numeric text area with dark theme styling
- Custom numeric keyboard with integrated BACK / SAVE / NEXT navigation buttons
- 8-page calibration workflow for 2-point reference calibration:
  1. V RAW REF LOW — Raw voltage sensor reading at low reference point
  2. V REF LOW — Actual (known) voltage at low reference point
  3. V RAW REF HIGH — Raw voltage sensor reading at high reference point
  4. V REF HIGH — Actual (known) voltage at high reference point
  5. A RAW REF LOW — Raw current sensor reading at low reference point
  6. A REF LOW — Actual (known) current at low reference point
  7. A RAW REF HIGH — Raw current sensor reading at high reference point
  8. A REF HIGH — Actual (known) current at high reference point
- Live raw sensor reading displayed on-screen (updates every 500ms), showing the current raw voltage or ampere value depending on which channel is being calibrated
- BACK navigates to previous calibration page
- NEXT navigates to next calibration page
- SAVE applies calibration immediately (no reboot required) and returns to Main Menu
- Current calibration values are loaded from the sensor module when entering the Settings screen
- All values displayed with 4 decimal places for precision

Current behavior:

- Calibration is applied at runtime via `sensor_set_voltage_cal()` / `sensor_set_ampere_cal()`
- Calibration is saved to NVS flash on SAVE via `sensor_save_cal_nvs()`
- Saved calibration is loaded on boot via `sensor_load_cal_nvs()` (falls back to `#define` defaults)

## 4. Build Integration

Implemented in:

- `main/CMakeLists.txt`: recursively adds all `main/*.c`
- `main/idf_component.yml`: managed dependencies for display/touch/LVGL

Managed dependencies currently used:

- `espressif/esp_lcd_ek79007`
- `espressif/esp_lvgl_port`
- `espressif/esp_lcd_touch_gt911`
- `lvgl/lvgl` (9.2.2)

## 5. Gauges Controller (`main/gauges_controller.c`)

Implemented:

- `gauges_controller_start()` — Creates a FreeRTOS task (`gauges_task`) at priority 2, stack 8192 bytes.
- `gauges_task()` — Loops every 1000 ms, calling `update_values()`.
- `update_values()` — Reads real voltage/ampere from the INA219 sensor module (with calibration), reads real flow rate/RPM from pulse counter sensors, computes Power as Voltage x Ampere, and pushes all data to both the Gauges screen (arcs + labels) and the Data screen (chart series + labels).
- All voltage, ampere, and flow values displayed with 3 decimal places (e.g. `12.345 V`). Power and RPM displayed as whole numbers.
- `init_data_chart()` — One-time setup that removes all SquareLine demo series (which use small external arrays) and recreates 4 series with LVGL-managed internal arrays:
  - Voltage (red `#F60707`) on primary Y-axis
  - Ampere (blue `#0D06E9`) on primary Y-axis
  - Flow (yellow `#F3E913`) on secondary Y-axis
  - RPM (grey `#808080`) on secondary Y-axis
- `rand_range_u32()` — Utility to generate random values within a range.
- `to_percent_u32()` — Utility to map a value to 0–100 percentage.
- Also updates `ui_lblTimeValue` on the Data screen with the current RTC time (`HH:MM:SS` format).

Voltage and Ampere read from INA219 with 2-point calibration. Flow Rate and RPM read from PCNT-based pulse sensors. Power is computed from Voltage × Ampere.

Arc gauge percentage ranges:
- Voltage: 10.0V–30.0V → 0–100%
- Ampere: 0–20.0A → 0–100%
- RPM: 200–1200 → 0–100%
- Power: 0–500W → 0–100%
- Flow: 0–20.0 L/M → 0–100%

## 6. Sensor Module (`main/sensor.c`)

Implemented:

- `sensor_init()` — Initializes the INA219 at address 0x40 with default configuration.
- `sensor_read_voltage()` — Reads INA219 bus voltage, applies 2-point calibration, returns calibrated voltage in volts. Logs both raw and calibrated values.
- `sensor_read_ampere()` — Reads INA219 current register, applies 2-point calibration, returns calibrated current in amps. Logs both raw and calibrated values.
- `sensor_read_raw_voltage()` — Reads uncalibrated INA219 bus voltage (used by settings page for live display).
- `sensor_read_raw_ampere()` — Reads uncalibrated INA219 current (used by settings page for live display).
- `sensor_set_voltage_cal()` / `sensor_set_ampere_cal()` — Set 2-point calibration data at runtime.
- `sensor_get_voltage_cal()` / `sensor_get_ampere_cal()` — Retrieve current calibration data.

2-point calibration:

- Maps two known (raw sensor reading, actual physical value) pairs to a linear function.
- Formula: `actual = slope * raw + offset` where `slope = (actual2 - actual1) / (raw2 - raw1)`.
- Default calibration defined via `#define` constants at the top of `sensor.c`:
  - `V_RAW_REF_LOW`, `V_REF_LOW`, `V_RAW_REF_HIGH`, `V_REF_HIGH` — Voltage channel
  - `A_RAW_REF_LOW`, `A_REF_LOW`, `A_RAW_REF_HIGH`, `A_REF_HIGH` — Ampere channel
- Calibration can be updated at runtime via the Settings screen (applied immediately, no reboot required).
- Raw sensor readings are logged every read cycle to assist with calibration.

INA219 default config: 32V bus range, ±320 mV shunt, 12-bit ADC, 0.1 Ω shunt, 3.2A max.

## 7. Pulse Controller (`main/pulse_controller.c`)

Implemented:

- `pulse_controller_init()` — Creates two PCNT channels for flow rate (GPIO4) and RPM (GPIO5).
- `pulse_controller_read()` — Reads pulse counts since last call, converts to flow rate (L/min) and RPM.
- `pulse_controller_set_flow_factor()` — Set the YF-DN50 calibration factor (default: 3.5 Hz per L/min).
- `pulse_controller_set_rpm_ppr()` — Set pulses per revolution for the hall sensor (default: 1.0).

Conversion formulas:

- Flow: `frequency (Hz) = pulses / sample_period` then `flow (L/min) = frequency / factor`
- RPM: `revolutions/sec = pulses / sample_period / ppr` then `RPM = revolutions/sec * 60`

Sensors:

- **YF-DN50** flow sensor on GPIO4 (open-collector output, internal pull-up enabled)
- **Hall sensor module** on GPIO5 (digital push-pull output)

## 8. Data Logger (`main/data_logger.c`)

Implemented:

- `data_logger_start()` — Creates a FreeRTOS task (`data_logger`) at priority 5, stack 8192 bytes, pinned to core 1.
- Daily file rotation: logs to `/sdcard/YYYY-MM-DD-log.csv` (e.g. `2026-04-15-log.csv`).
- Automatically creates a new file at midnight based on DS3231 RTC date.
- Writes CSV header automatically when a new day's file is created: `Timestamp,Voltage,Ampere,Power,RPM,Flow`
- Time column format: `d-mmm-yyyy h:mm:ss` (e.g. `21-Mar-2026 14:35:07`), read from DS3231 RTC.
- All numeric values formatted with 3 decimal places; RPM with 1 decimal place.
- Reads voltage/ampere from sensor module, flow/RPM from pulse controller, computes Power = Voltage × Ampere.
- Skips logging entirely if voltage or ampere is negative (prevents invalid data from being recorded).
- Checks `sd_is_mounted()` before writing to SD card.
- Also logs each row to serial console via `ESP_LOGI`.
- Skips row if RTC read fails.

## 9. Settings Controller (`main/settings_controller.c`)

Implemented:

- `settings_controller_on_screen_init()` — Called at end of `ui_Settings_screen_init()` to set up calibration pages, keyboard, and event handlers.
- 8-page calibration workflow editing the 2-point reference values for voltage and ampere channels.
- Custom keyboard map replaces LVGL default: numbers + BACK / SAVE / NEXT buttons.
- Live raw sensor reading label (green text, updates every 500ms via LVGL timer) shows current raw voltage or ampere depending on the active page.
- `apply_calibrations()` — Builds `sensor_cal_t` structs from page values, applies them immediately via `sensor_set_voltage_cal()` / `sensor_set_ampere_cal()`, and saves to NVS via `sensor_save_cal_nvs()`.
- `load_current_cal()` — Reads current calibration from sensor module when entering the screen.
- Timer is cleaned up when leaving the Settings screen (SAVE navigates to Main Menu).

## 10. What Is Not Yet Implemented (Observed Gaps)

- Flow factor / RPM PPR persistence (NVS) — currently hardcoded defaults
- UI-triggered control of GPIO48 LED
- Wi-Fi / Bluetooth connectivity
- OTA update flow (partition table supports it, firmware does not yet)
