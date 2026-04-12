#include "sensor.h"
#include "bsp_ina219.h"
#include "esp_log.h"

#define SENSOR_TAG "SENSOR"
#define SENSOR_INFO(fmt, ...)  ESP_LOGI(SENSOR_TAG, fmt, ##__VA_ARGS__)
#define SENSOR_ERROR(fmt, ...) ESP_LOGE(SENSOR_TAG, fmt, ##__VA_ARGS__)

/* Default calibration: 1:1 (raw value = physical value, i.e. no scaling).
 * Two points on the identity line y = x.                                    */
static sensor_cal_t s_voltage_cal = { .raw1 = 0.0f, .actual1 = 0.0f,
                                      .raw2 = 1.0f, .actual2 = 1.0f };
static sensor_cal_t s_ampere_cal  = { .raw1 = 0.0f, .actual1 = 0.0f,
                                      .raw2 = 1.0f, .actual2 = 1.0f };

/* ------------------------------------------------------------------ */
/*  2-point linear calibration: actual = slope * raw + offset          */
/* ------------------------------------------------------------------ */
static float calibrate(const sensor_cal_t *cal, float raw)
{
    float denom = cal->raw2 - cal->raw1;
    if (denom == 0.0f) {
        /* Degenerate calibration – return raw unchanged */
        return raw;
    }
    float slope  = (cal->actual2 - cal->actual1) / denom;
    float offset = cal->actual1 - slope * cal->raw1;
    return slope * raw + offset;
}

/* ------------------------------------------------------------------ */
esp_err_t sensor_init(void)
{
    esp_err_t err = ina219_init(INA219_ADDR_GND_GND, NULL);
    if (err != ESP_OK) {
        SENSOR_ERROR("INA219 init failed: %s", esp_err_to_name(err));
        return err;
    }
    SENSOR_INFO("Sensor subsystem initialized (INA219 @ 0x%02X)", INA219_ADDR_GND_GND);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
void sensor_set_voltage_cal(const sensor_cal_t *cal)
{
    if (cal) {
        s_voltage_cal = *cal;
        SENSOR_INFO("Voltage cal set: raw(%.3f,%.3f) -> actual(%.3f,%.3f)",
                    cal->raw1, cal->raw2, cal->actual1, cal->actual2);
    }
}

void sensor_set_ampere_cal(const sensor_cal_t *cal)
{
    if (cal) {
        s_ampere_cal = *cal;
        SENSOR_INFO("Ampere cal set: raw(%.3f,%.3f) -> actual(%.3f,%.3f)",
                    cal->raw1, cal->raw2, cal->actual1, cal->actual2);
    }
}

/* ------------------------------------------------------------------ */
esp_err_t sensor_read_voltage(float *voltage)
{
    if (voltage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    float bus_v;
    esp_err_t err = ina219_read_bus_voltage(&bus_v);
    if (err != ESP_OK) {
        return err;
    }

    *voltage = calibrate(&s_voltage_cal, bus_v);
    return ESP_OK;
}

esp_err_t sensor_read_ampere(float *ampere)
{
    if (ampere == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    float current;
    esp_err_t err = ina219_read_current(&current);
    if (err != ESP_OK) {
        return err;
    }

    *ampere = calibrate(&s_ampere_cal, current);
    return ESP_OK;
}

void sensor_get_voltage_cal(sensor_cal_t *cal)
{
    if (cal) *cal = s_voltage_cal;
}

void sensor_get_ampere_cal(sensor_cal_t *cal)
{
    if (cal) *cal = s_ampere_cal;
}
