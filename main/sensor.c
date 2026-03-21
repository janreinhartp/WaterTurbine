#include "sensor.h"
#include "bsp_ads1115.h"
#include "esp_log.h"

#define SENSOR_TAG "SENSOR"
#define SENSOR_INFO(fmt, ...)  ESP_LOGI(SENSOR_TAG, fmt, ##__VA_ARGS__)
#define SENSOR_ERROR(fmt, ...) ESP_LOGE(SENSOR_TAG, fmt, ##__VA_ARGS__)

/* ADS1115 channel assignments */
#define VOLTAGE_CHANNEL  ADS1115_CHANNEL_0
#define AMPERE_CHANNEL   ADS1115_CHANNEL_1

/* PGA gain – ±4.096 V gives good range for typical voltage divider / current sensor output */
#define VOLTAGE_GAIN     ADS1115_GAIN_4096
#define AMPERE_GAIN      ADS1115_GAIN_4096

/* Default calibration: 1:1 (raw voltage = physical value, i.e. no scaling).
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
    esp_err_t err = ads1115_init(ADS1115_ADDR_GND);
    if (err != ESP_OK) {
        SENSOR_ERROR("ADS1115 init failed: %s", esp_err_to_name(err));
        return err;
    }
    SENSOR_INFO("Sensor subsystem initialized (ADS1115 @ 0x%02X)", ADS1115_ADDR_GND);
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

    int16_t raw;
    esp_err_t err = ads1115_read_raw(VOLTAGE_CHANNEL, VOLTAGE_GAIN, &raw);
    if (err != ESP_OK) {
        return err;
    }

    float adc_v = ads1115_raw_to_voltage(raw, VOLTAGE_GAIN);
    *voltage = calibrate(&s_voltage_cal, adc_v);
    return ESP_OK;
}

esp_err_t sensor_read_ampere(float *ampere)
{
    if (ampere == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t raw;
    esp_err_t err = ads1115_read_raw(AMPERE_CHANNEL, AMPERE_GAIN, &raw);
    if (err != ESP_OK) {
        return err;
    }

    float adc_v = ads1115_raw_to_voltage(raw, AMPERE_GAIN);
    *ampere = calibrate(&s_ampere_cal, adc_v);
    return ESP_OK;
}
