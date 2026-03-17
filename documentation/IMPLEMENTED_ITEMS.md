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

Note:

- These functions currently always return `ESP_OK` and do not propagate lower-level driver errors.

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

Current data behavior:

- Gauge values and text labels are initialized with static values.
- No runtime sensor update loop is currently connected.

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

Current data behavior:

- Chart and labels are initialized with static demo/sample values.
- No live data feed is currently connected.

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

## 5. What Is Not Yet Implemented (Observed Gaps)

- Sensor acquisition pipeline connected to UI updates
- Live refresh logic for arcs/chart/labels
- Settings workflow logic for NEXT/BACK and multi-setting pages
- Settings value validation and persistence
- UI-triggered control of GPIO48 LED
- Explicit runtime task architecture documentation in source
