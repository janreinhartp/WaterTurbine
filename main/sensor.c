#include "sensor.h"
#include "bsp_ina219.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define SENSOR_TAG "SENSOR"
#define SENSOR_INFO(fmt, ...)  ESP_LOGI(SENSOR_TAG, fmt, ##__VA_ARGS__)
#define SENSOR_ERROR(fmt, ...) ESP_LOGE(SENSOR_TAG, fmt, ##__VA_ARGS__)

/* ===================================================================
 *  MANUAL CALIBRATION REFERENCE POINTS
 *  Adjust these values based on raw readings from the log output.
 *
 *  V_RAW_REF_HIGH  — Raw voltage reading at the HIGH reference point
 *  V_REF_HIGH      — Actual (known) voltage at the HIGH reference point
 *  V_RAW_REF_LOW   — Raw voltage reading at the LOW reference point
 *  V_REF_LOW       — Actual (known) voltage at the LOW reference point
 *
 *  A_RAW_REF_HIGH  — Raw current reading at the HIGH reference point
 *  A_REF_HIGH      — Actual (known) current at the HIGH reference point
 *  A_RAW_REF_LOW   — Raw current reading at the LOW reference point
 *  A_REF_LOW       — Actual (known) current at the LOW reference point
 * =================================================================== */
#define V_RAW_REF_LOW   0.0f
#define V_REF_LOW       0.0f
#define V_RAW_REF_HIGH  33.0f
#define V_REF_HIGH      32.0f

#define A_RAW_REF_LOW   0.0f
#define A_REF_LOW       0.0f
#define A_RAW_REF_HIGH  3.2f
#define A_REF_HIGH      3.2f

static sensor_cal_t s_voltage_cal = { .raw1 = V_RAW_REF_LOW,  .actual1 = V_REF_LOW,
                                      .raw2 = V_RAW_REF_HIGH, .actual2 = V_REF_HIGH };
static sensor_cal_t s_ampere_cal  = { .raw1 = A_RAW_REF_LOW,  .actual1 = A_REF_LOW,
                                      .raw2 = A_RAW_REF_HIGH, .actual2 = A_REF_HIGH };

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
    SENSOR_INFO("VOLTAGE  raw=%.4f  calibrated=%.4f", bus_v, *voltage);
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
    SENSOR_INFO("AMPERE   raw=%.4f  calibrated=%.4f", current, *ampere);
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

esp_err_t sensor_read_raw_voltage(float *raw_voltage)
{
    if (raw_voltage == NULL) return ESP_ERR_INVALID_ARG;
    return ina219_read_bus_voltage(raw_voltage);
}

esp_err_t sensor_read_raw_ampere(float *raw_ampere)
{
    if (raw_ampere == NULL) return ESP_ERR_INVALID_ARG;
    return ina219_read_current(raw_ampere);
}

/* ------------------------------------------------------------------ */
/*  NVS persistence for calibration                                    */
/* ------------------------------------------------------------------ */
#define NVS_CAL_NAMESPACE "sensor_cal"

esp_err_t sensor_save_cal_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_CAL_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        SENSOR_ERROR("NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    err  = nvs_set_blob(h, "vcal", &s_voltage_cal, sizeof(s_voltage_cal));
    err |= nvs_set_blob(h, "acal", &s_ampere_cal,  sizeof(s_ampere_cal));
    err |= nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) {
        SENSOR_ERROR("NVS save failed: %s", esp_err_to_name(err));
    } else {
        SENSOR_INFO("Calibration saved to NVS");
    }
    return err;
}

esp_err_t sensor_load_cal_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_CAL_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        SENSOR_INFO("No saved calibration in NVS, using #define defaults");
        return err;
    }

    sensor_cal_t vcal, acal;
    size_t len = sizeof(vcal);
    esp_err_t v_err = nvs_get_blob(h, "vcal", &vcal, &len);
    len = sizeof(acal);
    esp_err_t a_err = nvs_get_blob(h, "acal", &acal, &len);
    nvs_close(h);

    if (v_err == ESP_OK) {
        s_voltage_cal = vcal;
        SENSOR_INFO("Loaded voltage cal from NVS: raw(%.3f,%.3f) -> actual(%.3f,%.3f)",
                    vcal.raw1, vcal.raw2, vcal.actual1, vcal.actual2);
    }
    if (a_err == ESP_OK) {
        s_ampere_cal = acal;
        SENSOR_INFO("Loaded ampere cal from NVS: raw(%.3f,%.3f) -> actual(%.3f,%.3f)",
                    acal.raw1, acal.raw2, acal.actual1, acal.actual2);
    }
    return (v_err == ESP_OK && a_err == ESP_OK) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
