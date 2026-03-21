// main.c
#include "main.h"
#include "ui.h"
#include "gauges_controller.h"

/* LDO channel handle */
static esp_ldo_channel_handle_t ldo3 = NULL;
static esp_ldo_channel_handle_t ldo4 = NULL;

/**
 • @brief Initialization failure handler (repeatedly prints error information)

 */
static void init_fail_handler(const char *module_name, esp_err_t err) {
    while (1) {  // Infinite loop to help debug which module failed to initialize
        MAIN_ERROR("[%s] init failed: %s", module_name, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
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
    if (err != ESP_OK) init_fail_handler("ldo3", err);

    esp_ldo_channel_config_t ldo4_cof = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };
    err = esp_ldo_acquire_channel(&ldo4_cof, &ldo4);
    if (err != ESP_OK) init_fail_handler("ldo4", err);
    MAIN_INFO("LDO3 and LDO4 init success");

    // 2. Initialize I2C (required for touch chip)
    MAIN_INFO("Initializing I2C...");
    err = i2c_init();
    if (err != ESP_OK) init_fail_handler("I2C", err);
    MAIN_INFO("I2C init success");

    // 3. Initialize touch panel (low-level driver)
    MAIN_INFO("Initializing touch panel...");
    err = touch_init();
    if (err != ESP_OK) init_fail_handler("Touch", err);
    MAIN_INFO("Touch panel init success");

    // 4. Initialize LCD hardware and LVGL (must initialize before turning on backlight)
    err = display_init();
    if (err != ESP_OK) init_fail_handler("LCD", err);
    MAIN_INFO("LCD init success");

    // 5. Turn on LCD backlight (brightness set to 100 = max)
    err = set_lcd_blight(100);
    if (err != ESP_OK) init_fail_handler("LCD Backlight", err);
    MAIN_INFO("LCD backlight opened (brightness: 100)");

    // 6. Initialize LED control GPIO (GPIO48)
    MAIN_INFO("Initializing GPIO48 for LED...");
    err = gpio_extra_init();
    if (err != ESP_OK) init_fail_handler("GPIO48", err);

    gpio_extra_set_level(false);  // Initially turn off LED
    MAIN_INFO("LED initialized to OFF state");

    // 7. Initialize sensor subsystem (ADS1115 for voltage & current)
    MAIN_INFO("Initializing sensor subsystem...");
    err = sensor_init();
    if (err != ESP_OK) init_fail_handler("Sensor", err);
    MAIN_INFO("Sensor subsystem init success");

    // Set default 2-point calibration (adjust these to your hardware)
    // Voltage: e.g. voltage divider maps 0V->0V, 4.0V ADC -> 30.0V actual
    sensor_cal_t voltage_cal = { .raw1 = 0.0f, .actual1 = 0.0f,
                                 .raw2 = 4.0f, .actual2 = 30.0f };
    sensor_set_voltage_cal(&voltage_cal);

    // Ampere: e.g. current sensor maps 0V->0A, 4.0V ADC -> 20.0A actual
    sensor_cal_t ampere_cal = { .raw1 = 0.0f, .actual1 = 0.0f,
                                .raw2 = 4.0f, .actual2 = 20.0f };
    sensor_set_ampere_cal(&ampere_cal);

    // 8. Initialize pulse-based sensors (flow rate + RPM)
    MAIN_INFO("Initializing pulse controller (flow + RPM)...");
    err = pulse_controller_init();
    if (err != ESP_OK) init_fail_handler("PulseCtrl", err);
    MAIN_INFO("Pulse controller init success (flow=GPIO4, rpm=GPIO5)");

    // 9. Initialize DS3231 RTC
    MAIN_INFO("Initializing DS3231 RTC...");
    err = ds3231_init();
    if (err != ESP_OK) init_fail_handler("DS3231", err);
    MAIN_INFO("DS3231 RTC init success");

    // 10. Initialize SD card
    MAIN_INFO("Initializing SD card...");
    err = sd_init();
    if (err != ESP_OK) init_fail_handler("SD Card", err);
    MAIN_INFO("SD card init success");
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

    // Start periodic random gauge updates (every 1 second)
    err = gauges_controller_start();
    if (err != ESP_OK) init_fail_handler("Gauge Controller", err);
    MAIN_INFO("Gauge controller started successfully");

    // Start CSV data logger (every 60 seconds)
    err = data_logger_start();
    if (err != ESP_OK) init_fail_handler("Data Logger", err);
    MAIN_INFO("Data logger started successfully");
}
