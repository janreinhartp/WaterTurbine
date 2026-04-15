// main.c
#include "main.h"
#include "ui.h"
#include "gauges_controller.h"
#include "nvs_flash.h"
#include <string.h>

/* LDO channel handle */
static esp_ldo_channel_handle_t ldo3 = NULL;
static esp_ldo_channel_handle_t ldo4 = NULL;

/* Startup error message buffer */
#define STARTUP_ERR_BUF_SIZE 256
static char s_startup_errors[STARTUP_ERR_BUF_SIZE];
static size_t s_startup_err_len = 0;

static void append_startup_error(const char *module_name, esp_err_t err) {
    int written = snprintf(s_startup_errors + s_startup_err_len,
                           STARTUP_ERR_BUF_SIZE - s_startup_err_len,
                           "%s%s: %s",
                           s_startup_err_len > 0 ? "\n" : "",
                           module_name, esp_err_to_name(err));
    if (written > 0) {
        s_startup_err_len += (size_t)written;
    }
    MAIN_ERROR("[%s] init failed: %s", module_name, esp_err_to_name(err));
}

/**
 • @brief System initialization (LDO + LCD + backlight + other hardware)

 */
static void system_init(void) {
    esp_err_t err = ESP_OK;

    // 1. Initialize LDO (required for screen)
    esp_ldo_channel_config_t ldo3_cof = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };
    err = esp_ldo_acquire_channel(&ldo3_cof, &ldo3);
    if (err != ESP_OK) { MAIN_ERROR("ldo3 init failed"); return; }

    esp_ldo_channel_config_t ldo4_cof = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };
    err = esp_ldo_acquire_channel(&ldo4_cof, &ldo4);
    if (err != ESP_OK) { MAIN_ERROR("ldo4 init failed"); return; }
    MAIN_INFO("LDO3 and LDO4 init success");

    // 2. Initialize I2C (required for touch chip)
    MAIN_INFO("Initializing I2C...");
    err = i2c_init();
    if (err != ESP_OK) { MAIN_ERROR("I2C init failed"); return; }
    MAIN_INFO("I2C init success");

    // 3. Initialize touch panel (low-level driver)
    MAIN_INFO("Initializing touch panel...");
    err = touch_init();
    if (err != ESP_OK) { MAIN_ERROR("Touch init failed"); return; }
    MAIN_INFO("Touch panel init success");

    // 4. Initialize LCD hardware and LVGL (must initialize before turning on backlight)
    err = display_init();
    if (err != ESP_OK) { MAIN_ERROR("LCD init failed"); return; }
    MAIN_INFO("LCD init success");

    // 5. Turn on LCD backlight (brightness set to 100 = max)
    err = set_lcd_blight(100);
    if (err != ESP_OK) { MAIN_ERROR("Backlight init failed"); return; }
    MAIN_INFO("LCD backlight opened (brightness: 100)");

    // 6. Initialize LED control GPIO (GPIO48)
    MAIN_INFO("Initializing GPIO48 for LED...");
    err = gpio_extra_init();
    if (err != ESP_OK) append_startup_error("GPIO48", err);

    gpio_extra_set_level(false);  // Initially turn off LED
    MAIN_INFO("LED initialized to OFF state");

    // 7. Initialize NVS flash (needed for calibration persistence)
    MAIN_INFO("Initializing NVS flash...");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) append_startup_error("NVS", err);
    MAIN_INFO("NVS flash init success");

    // 8. Initialize sensor subsystem (INA219 for voltage & current)
    MAIN_INFO("Initializing sensor subsystem...");
    err = sensor_init();
    if (err != ESP_OK) append_startup_error("Sensor", err);
    sensor_load_cal_nvs();  // Restore saved calibration (falls back to #define defaults)
    MAIN_INFO("Sensor subsystem init success");

    // 8. Initialize pulse-based sensors (flow rate + RPM)
    MAIN_INFO("Initializing pulse controller (flow + RPM)...");
    err = pulse_controller_init();
    if (err != ESP_OK) append_startup_error("PulseCtrl", err);
    MAIN_INFO("Pulse controller init success (flow=GPIO4, rpm=GPIO5)");

    // 9. Initialize DS3231 RTC
    MAIN_INFO("Initializing DS3231 RTC...");
    err = ds3231_init();
    if (err != ESP_OK) append_startup_error("DS3231", err);
    MAIN_INFO("DS3231 RTC init success");

    // Set RTC to build time
    {
        static const char *months[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        const char *build_date = __DATE__;  // e.g. "Mar 31 2026"
        const char *build_time = __TIME__;  // e.g. "14:30:00"
        ds3231_time_t bt = {0};
        char mon_str[4] = {0};
        int day_val = 0, year_val = 0;
        int hh = 0, mm = 0, ss = 0;
        sscanf(build_date, "%3s %d %d", mon_str, &day_val, &year_val);
        sscanf(build_time, "%d:%d:%d", &hh, &mm, &ss);
        bt.year    = (uint16_t)year_val;
        bt.date    = (uint8_t)day_val;
        bt.hours   = (uint8_t)hh;
        bt.minutes = (uint8_t)mm;
        bt.seconds = (uint8_t)ss;
        bt.day     = 1;  // day-of-week not critical
        for (int i = 0; i < 12; i++) {
            if (strncmp(mon_str, months[i], 3) == 0) {
                bt.month = (uint8_t)(i + 1);
                break;
            }
        }
        err = ds3231_set_time(&bt);
        if (err != ESP_OK) {
            MAIN_ERROR("Failed to set RTC to build time");
        } else {
            MAIN_INFO("RTC set to build time: %04u-%02u-%02u %02u:%02u:%02u",
                       bt.year, bt.month, bt.date, bt.hours, bt.minutes, bt.seconds);
        }
    }

    // 10. Initialize SD card
    MAIN_INFO("Initializing SD card...");
    err = sd_init();
    if (err != ESP_OK) {
        append_startup_error("SD Card", err);
    } else {
        MAIN_INFO("SD card init success");
    }
}

void app_main(void)
{
    esp_err_t err = ESP_OK;

    MAIN_INFO("Starting LED control application...");

    // System initialization (including LDO, LCD, touch, LED and all hardware)
    system_init();
    MAIN_INFO("System initialized successfully");

    // Initialize UI components
    ui_init();
    MAIN_INFO("UI initialized successfully");

    // Show startup errors on MainMenu label (or clear it)
    if (ui_lblErrorStartUp) {
        if (s_startup_err_len > 0) {
            lv_label_set_text(ui_lblErrorStartUp, s_startup_errors);
            lv_obj_set_style_text_color(ui_lblErrorStartUp, lv_color_hex(0xFF0000), LV_PART_MAIN);
        } else {
            lv_label_set_text(ui_lblErrorStartUp, "All systems OK");
            lv_obj_set_style_text_color(ui_lblErrorStartUp, lv_color_hex(0x00FF00), LV_PART_MAIN);
        }
    }

    // Start periodic random gauge updates (every 1 second)
    err = gauges_controller_start();
    if (err != ESP_OK) append_startup_error("Gauge Controller", err);
    MAIN_INFO("Gauge controller started successfully");

    // Start serial data logger (every 5 seconds)
    err = data_logger_start();
    if (err != ESP_OK) append_startup_error("Data Logger", err);
    MAIN_INFO("Data logger started successfully");
}
