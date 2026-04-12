#include "data_logger.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_ds3231.h"
#include "bsp_sd.h"
#include "sensor.h"
#include "pulse_controller.h"

#include <stdio.h>
#include <string.h>

#define LOGGER_TAG        "DATA_LOG"
#define LOGGER_PERIOD_MS  (1 * 1000)
#define LOGGER_STACK_SIZE 8192
#define LOG_FILE_PATH     SD_MOUNT_POINT "/log.csv"
#define LOG_CSV_HEADER    "Timestamp,Voltage,Ampere,Power,RPM,Flow\n"

static TaskHandle_t s_logger_task = NULL;

static const char *s_month_abbr[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Format: d-mmm-yyyy h:mm:ss  (e.g. "21-Mar-2026 14:35:07") */
static void format_time(const ds3231_time_t *t, char *buf, size_t len)
{
    uint8_t mi = t->month;
    if (mi < 1)  mi = 1;
    if (mi > 12) mi = 12;

    snprintf(buf, len, "%u-%s-%04u %02u:%02u:%02u",
             t->date, s_month_abbr[mi - 1], t->year,
             t->hours, t->minutes, t->seconds);
}

static void logger_task(void *arg)
{
    (void)arg;

    /* Verify the SD card is accessible by writing the CSV header */
    if (sd_is_mounted() && !sd_file_exists(LOG_FILE_PATH)) {
        esp_err_t hdr_err = sd_append_string(LOG_FILE_PATH, LOG_CSV_HEADER);
        if (hdr_err != ESP_OK) {
            ESP_LOGE(LOGGER_TAG, "Failed to create %s – is the SD card mounted?", LOG_FILE_PATH);
        } else {
            ESP_LOGI(LOGGER_TAG, "Created %s with header", LOG_FILE_PATH);
        }
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(LOGGER_PERIOD_MS));

        /* ---- Read RTC ---- */
        ds3231_time_t now = {0};
        if (ds3231_get_time(&now) != ESP_OK) {
            ESP_LOGE(LOGGER_TAG, "RTC read failed, skipping cycle");
            continue;
        }

        char time_str[32];
        format_time(&now, time_str, sizeof(time_str));

        /* ---- Read calibrated voltage & ampere ---- */
        float voltage = 0.0f, ampere = 0.0f;
        sensor_read_voltage(&voltage);
        sensor_read_ampere(&ampere);
        float power = voltage * ampere;

        /* ---- Read flow rate & RPM ---- */
        float flow = 0.0f, rpm = 0.0f;
        pulse_controller_read(&flow, &rpm);

        /* ---- Log to serial ---- */
        ESP_LOGI(LOGGER_TAG, "[%s] V=%.3f A=%.3f P=%.3f RPM=%.1f Flow=%.3f",
                 time_str, voltage, ampere, power, rpm, flow);

        /* ---- Save to SD card ---- */
        if (sd_is_mounted()) {
            char csv_line[128];
            snprintf(csv_line, sizeof(csv_line),
                     "%s,%.3f,%.3f,%.3f,%.1f,%.3f\n",
                     time_str, voltage, ampere, power, rpm, flow);
            if (sd_append_string(LOG_FILE_PATH, csv_line) != ESP_OK) {
                ESP_LOGE(LOGGER_TAG, "SD write failed");
            }
        }
    }
}

esp_err_t data_logger_start(void)
{
    if (s_logger_task != NULL) {
        return ESP_OK;
    }

    BaseType_t rc = xTaskCreatePinnedToCore(
        logger_task,
        "data_logger",
        LOGGER_STACK_SIZE,
        NULL,
        5,
        &s_logger_task,
        1);

    if (rc != pdPASS) {
        ESP_LOGE(LOGGER_TAG, "Failed to create data logger task");
        return ESP_FAIL;
    }

    ESP_LOGI(LOGGER_TAG, "Data logger started (period=%d s, serial output)",
             LOGGER_PERIOD_MS / 1000);
    return ESP_OK;
}
