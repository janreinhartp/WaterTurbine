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
7. Initialize UI screens and load Main Menu

If one stage fails, the app logs the module name in a loop for easy diagnosis.

## 4. How To Use the Current UI

### Main Menu

- Tap GAUGES to open gauge dashboard
- Tap DATA to open chart and numeric metrics
- Tap SETTINGS to open settings page

### Gauges

- Shows 5 arc gauges (Voltage, Ampere, Speed, Power, Flow)
- BACK returns to Main Menu
- DATA opens Data page

### Data

- Shows chart + metric summary labels
- BACK returns to Main Menu

### Settings

- Numeric text box with on-screen numeric keyboard
- SAVE returns to Main Menu
- NEXT and BACK are visual only at the moment (no callbacks yet)

## 5. How To Update LVGL Values in Code

Use object handles exposed by generated headers (examples: `ui_gVoltage`, `ui_lblVoltageValue`, `ui_Chart1`).

Typical update pattern:

```c
// Example only: call from a safe LVGL context/task.
lv_arc_set_value(ui_gVoltage, 55);
lv_label_set_text(ui_gVoltageValue, "24.3 V");

lv_chart_set_next_value(ui_Chart1, series_handle, 37);
```

Practical recommendation:

- Keep data acquisition in a dedicated task.
- Push parsed values to a UI update function.
- Apply LVGL updates in the LVGL-safe context (through your LVGL port locking strategy/task model).

## 6. How To Add New App Logic

1. Add sensor read module in `peripheral/` or `main/`.
2. Parse/scale raw values.
3. Update LVGL widgets in a periodic UI update routine.
4. If settings are needed, connect `NEXT/BACK/SAVE` button callbacks.
5. Save settings to NVS if persistence is required.

## 7. Troubleshooting

- No display/backlight:
  - Check LDO init, display init, and backlight GPIO31 PWM path.
- No touch response:
  - Check I2C init and GT911 RST/INT pin wiring/config.
- App stuck printing init error:
  - The module name in log indicates the failed initialization stage.
