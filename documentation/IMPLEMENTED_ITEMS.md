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
- Sensor subsystem initialization (ADS1115 for voltage and current)
- Default 2-point calibration applied for voltage and ampere channels
- Pulse controller initialization (flow rate on GPIO4, RPM on GPIO5)
- DS3231 RTC initialization over I2C
- SD card initialization and mount (1-wire SDIO)
- Gauges controller task started via `gauges_controller_start()`
- CSV data logger task started via `data_logger_start()`

Error handling behavior:

- Any failed init stage enters a loop and logs an error once per second.

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

### ADS1115 ADC (`peripheral/bsp_ads1115`)

Implemented:

- ADS1115 16-bit ADC initialization over I2C (`ads1115_init`)
- Single-shot read on any channel with configurable PGA gain (`ads1115_read_raw`)
- Raw-to-voltage conversion (`ads1115_raw_to_voltage`)
- Supports all 4 single-ended channels and 6 PGA gain settings

Default I2C address: 0x48 (ADDR pin to GND)

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

- Voltage and Ampere are read from the ADS1115 ADC with 2-point calibration applied.
- Flow Rate is read from YF-DN50 sensor via PCNT on GPIO4 (default factor: 3.5 Hz per L/min).
- RPM is read from hall sensor via PCNT on GPIO5 (default: 1 pulse per revolution).
- Power is computed as Voltage × Ampere (real calculation).
- All five gauge arcs and value labels are updated every 1 second by `gauges_controller`.
- Arc values are set as percentages (0–100) of each parameter's range.

### Data Screen

Implemented in `main/ui/ui_Data.c`:

- Bar chart (`lv_chart`) with 3 series and axis scales
- Metric panels with labels/values:
  - Flow rate
  - Voltage
  - Ampere
  - Power
  - RPM
- BACK button to Main Menu

Runtime data behavior:

- The chart is converted from BAR to LINE type at runtime by `init_data_chart()` in `gauges_controller.c`.
- Three line chart series are displayed in a 20-point rolling window:
  - Flow Rate (green, `#13C56D`) on primary Y-axis (0–30)
  - Voltage (red, `#D62323`) on secondary Y-axis (0–20)
  - Ampere (yellow, `#F3E913`) on secondary Y-axis (0–20)
- Chart uses shift mode (oldest points removed as new data arrives).
- All five metric labels (Flow Rate, Voltage, Ampere, Power, RPM) update every 1 second.

### Settings Screen

Implemented in `main/ui/ui_Settings.c`:

- Settings title and container
- Numeric text area
- Numeric keyboard
- Buttons:
  - NEXT
  - SAVE
  - BACK

Current behavior:

- `SAVE` button has an event handler and returns to Main Menu.
- `NEXT` and `BACK` buttons are created but have no event callback attached.
- No parameter persistence (NVS/file) is implemented yet.

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

- `gauges_controller_start()` — Creates a FreeRTOS task (`gauges_task`) at priority 4, stack 4096 bytes.
- `gauges_task()` — Loops every 1000 ms, calling `update_values()`.
- `update_values()` — Reads real voltage/ampere from the ADS1115 sensor module (with calibration), reads real flow rate/RPM from pulse counter sensors, generates pseudo-random value for Power, and pushes all data to both the Gauges screen (arcs + labels) and the Data screen (chart series + labels).
- `init_data_chart()` — One-time setup that converts the SquareLine-generated bar chart to a line chart with 3 series, configures colors, Y-axis assignments, and shift mode.
- `rand_range_u32()` — Utility to generate random values within a range.
- `to_percent_u32()` — Utility to map a value to 0–100 percentage.

Voltage and Ampere now read from ADS1115 with 2-point calibration. Flow Rate and RPM read from PCNT-based pulse sensors. Power is computed from Voltage × Ampere.

## 6. Sensor Module (`main/sensor.c`)

Implemented:

- `sensor_init()` — Initializes the ADS1115 at address 0x48.
- `sensor_read_voltage()` — Reads ADS1115 channel 0 (AIN0), applies 2-point calibration, returns calibrated voltage in volts.
- `sensor_read_ampere()` — Reads ADS1115 channel 1 (AIN1), applies 2-point calibration, returns calibrated current in amps.
- `sensor_set_voltage_cal()` / `sensor_set_ampere_cal()` — Set 2-point calibration data.

2-point calibration:

- Maps two known (raw ADC voltage, actual physical value) pairs to a linear function.
- Formula: `actual = slope * raw + offset` where `slope = (actual2 - actual1) / (raw2 - raw1)`.
- Default calibration set in `main.c` during startup:
  - Voltage: 0V ADC → 0V, 4.0V ADC → 30.0V
  - Ampere: 0V ADC → 0A, 4.0V ADC → 20.0A
- Calibration values should be adjusted to match actual voltage divider / current sensor hardware.

PGA gain: ±4.096V for both channels.

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

- `data_logger_start()` — Creates a FreeRTOS task (`data_logger`) at priority 1, stack 4096 bytes.
- Logs to `/sdcard/datalog.csv` every 60 seconds.
- Writes CSV header automatically on first run: `Time,Flow Rate,RPM,Voltage,Ampere,Power`
- Time column format: `d-mmm-yyyy h-mm` (e.g. `21-Mar-2026 14-35`), read from DS3231 RTC.
- All numeric values formatted with 3 decimal places (e.g. `12.340,850.000,24.500,5.100,124.950`).
- Reads voltage/ampere from sensor module, flow/RPM from pulse controller, computes Power = Voltage × Ampere.
- Skips row if RTC read fails.

## 9. What Is Not Yet Implemented (Observed Gaps)

- RTC time display on UI (DS3231 driver is ready but no UI label wired yet)
- CSV log file management (rotation, size limits, deletion)
- Flow factor / RPM PPR persistence (NVS) — currently hardcoded defaults
- Calibration persistence (NVS) — calibration is currently hardcoded at startup
- Settings UI wired to calibration values
- Settings workflow logic for NEXT/BACK and multi-setting pages
- Settings value validation and persistence (NVS)
- UI-triggered control of GPIO48 LED
- Wi-Fi / Bluetooth connectivity
- Data logging or export
- OTA update flow (partition table supports it, firmware does not yet)
