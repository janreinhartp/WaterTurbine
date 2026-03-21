# Usage Guide

This guide explains how to build, flash, run, and extend the current WaterTurbine firmware.

## 1. Prerequisites

- ESP-IDF installed and exported in your shell
- Target board connected and recognized by serial port
- VS Code ESP-IDF extension (recommended)

## 2. Build and Flash

## Option A: VS Code ESP-IDF Extension (Recommended)

Use command palette:

1. `ESP-IDF: Build your project`
2. `ESP-IDF: Flash your project`
3. `ESP-IDF: Monitor your device`

## Option B: Terminal Commands

From project root:

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your COM port (example: `COM5`).

## 3. Runtime Startup Sequence

At boot, the app currently executes this sequence:

1. Initialize LDO channels
2. Initialize I2C bus
3. Initialize GT911 touch
4. Initialize display + LVGL
5. Turn on LCD backlight
6. Initialize extra GPIO (LED pin)
7. Initialize sensor subsystem (ADS1115 + default calibration)
8. Initialize pulse controller (flow rate GPIO4, RPM GPIO5)
9. Initialize UI screens and load Main Menu
10. Start gauges controller task (1-second update loop)

If one stage fails, the app logs the module name in a loop for easy diagnosis.

## 4. How To Use the Current UI

### Main Menu

- Tap GAUGES to open gauge dashboard
- Tap DATA to open chart and numeric metrics
- Tap SETTINGS to open settings page

### Gauges

- Shows 5 arc gauges (Voltage, Ampere, Speed, Power, Flow)
- Voltage and Ampere read from ADS1115 ADC with 2-point calibration
- Flow Rate read from YF-DN50 sensor via hardware pulse counter (GPIO4)
- RPM read from hall sensor via hardware pulse counter (GPIO5)
- Power still uses pseudo-random values
- All gauges update every 1 second
- BACK returns to Main Menu
- DATA opens Data page

### Data

- Shows a live line chart with 3 series (Flow Rate, Voltage, Ampere) in a 20-point rolling window
- Five metric summary labels update every 1 second
- BACK returns to Main Menu

### Settings

- Numeric text box with on-screen numeric keyboard
- SAVE returns to Main Menu
- NEXT and BACK are visual only at the moment (no callbacks yet)

## 5. How Values Are Updated at Runtime

The `gauges_controller` module (`main/gauges_controller.c`) runs a FreeRTOS task that:

1. Reads voltage and ampere from the ADS1115 via the sensor module (with 2-point calibration applied).
2. Reads flow rate and RPM from pulse counter sensors via the pulse controller.
3. Generates pseudo-random value for Power.
4. Updates gauge arcs (percentage) and value labels on the Gauges screen.
5. Pushes new data points to the line chart and updates metric labels on the Data screen.
6. Repeats every 1 second.

All LVGL calls are wrapped in `lvgl_port_lock()` / `lvgl_port_unlock()` for thread safety.

### Calibration

Voltage and Ampere use 2-point linear calibration defined in `main/sensor.c`.
Default calibration is set in `system_init()` in `main.c`:

```c
// Voltage: 0V ADC -> 0V actual, 4.0V ADC -> 30.0V actual
sensor_cal_t voltage_cal = { .raw1 = 0.0f, .actual1 = 0.0f,
                             .raw2 = 4.0f, .actual2 = 30.0f };
sensor_set_voltage_cal(&voltage_cal);

// Ampere: 0V ADC -> 0A actual, 4.0V ADC -> 20.0A actual
sensor_cal_t ampere_cal = { .raw1 = 0.0f, .actual1 = 0.0f,
                            .raw2 = 4.0f, .actual2 = 20.0f };
sensor_set_ampere_cal(&ampere_cal);
```

Adjust these values to match your voltage divider ratio and current sensor output characteristics.

### Flow Rate & RPM

Flow rate and RPM use hardware pulse counters (PCNT peripheral) configured in `main/pulse_controller.c`.

- **YF-DN50 flow sensor** (GPIO4): Default factor = 3.5 Hz per L/min. Adjust with `pulse_controller_set_flow_factor()`.
- **Hall sensor** (GPIO5): Default = 1 pulse per revolution. Adjust with `pulse_controller_set_rpm_ppr()`.

To replace remaining pseudo-random data (Power), add a driver and modify `update_values()` in `gauges_controller.c`.

## 6. How To Extend the Project

1. Add sensor read module in `peripheral/` or `main/`.
2. Parse/scale raw values.
3. Add calibration and integrate into `gauges_controller.c` `update_values()` (see voltage/ampere for pattern).
4. If settings are needed, connect `NEXT/BACK/SAVE` button callbacks.
5. Save settings/calibration to NVS if persistence is required.

## 7. Troubleshooting

- No display/backlight:
  - Check LDO init, display init, and backlight GPIO31 PWM path.
- No touch response:
  - Check I2C init and GT911 RST/INT pin wiring/config.
- App stuck printing init error:
  - The module name in log indicates the failed initialization stage.
