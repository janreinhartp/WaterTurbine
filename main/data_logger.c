#include "data_logger.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_sd.h"
#include "bsp_ds3231.h"
#include "sensor.h"
#include "pulse_controller.h"

#include <stdio.h>
#include <string.h>

#define LOGGER_TAG        "DATA_LOG"
#define LOGGER_PERIOD_MS  (60 * 1000)
#define CSV_PATH          SD_MOUNT_POINT "/datalog.csv"
#define CSV_HEADER        "Time,Flow Rate,RPM,Voltage,Ampere,Power\n"
#define LINE_BUF_SIZE     128

static TaskHandle_t s_logger_task = NULL;

static const char *s_month_abbr[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Format: d-mmm-yyyy h-mm  (e.g. "21-Mar-2026 14-35") */
static void format_time(const ds3231_time_t *t, char *buf, size_t len)
{
    uint8_t mi = t->month;
    if (mi < 1)  mi = 1;
    if (mi > 12) mi = 12;

    snprintf(buf, len, "%u-%s-%04u %u-%02u",
             t->date, s_month_abbr[mi - 1], t->year,
             t->hours, t->minutes);
}

static void logger_task(void *arg)
{
    (void)arg;

    /* Write CSV header if the file does not yet exist */
    if (!sd_file_exists(CSV_PATH)) {
        if (sd_append_string(CSV_PATH, CSV_HEADER) != ESP_OK) {
            ESP_LOGE(LOGGER_TAG, "Failed to write CSV header");
        }
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(LOGGER_PERIOD_MS));

        /* ---- Read RTC ---- */
        ds3231_time_t now = {0};
        if (ds3231_get_time(&now) != ESP_OK) {
            ESP_LOGE(LOGGER_TAG, "RTC read failed, skipping row");
            continue;
        }

        /* ---- Read sensors ---- */
        float voltage = 0.0f, ampere = 0.0f;
        sensor_read_voltage(&voltage);
        sensor_read_ampere(&ampere);

        float flow = 0.0f, rpm = 0.0f;
        pulse_controller_read(&flow, &rpm);

        float power = voltage * ampere;

        /* ---- Build CSV line ---- */
        char time_str[32];
        format_time(&now, time_str, sizeof(time_str));

        char line[LINE_BUF_SIZE];
        snprintf(line, sizeof(line),
                 "%s,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                 time_str, flow, rpm, voltage, ampere, power);

        /* ---- Append to file ---- */
        if (sd_append_string(CSV_PATH, line) == ESP_OK) {
            ESP_LOGI(LOGGER_TAG, "Logged: %s", line);
        } else {
            ESP_LOGE(LOGGER_TAG, "Failed to append CSV row");
        }
    }
}

esp_err_t data_logger_start(void)
{
    if (s_logger_task != NULL) {
        return ESP_OK;
    }

    BaseType_t rc = xTaskCreate(
        logger_task,
        "data_logger",
        4096,
        NULL,
        tskIDLE_PRIORITY + 1,
        &s_logger_task);

    if (rc != pdPASS) {
        ESP_LOGE(LOGGER_TAG, "Failed to create data logger task");
        return ESP_FAIL;
    }

    ESP_LOGI(LOGGER_TAG, "Data logger started (period=%d s, file=%s)",
             LOGGER_PERIOD_MS / 1000, CSV_PATH);
    return ESP_OK;
}
