#ifndef _SENSOR_H_
#define _SENSOR_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
#include <stdbool.h>
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

/* 2-point calibration data: maps raw sensor reading to actual physical value.
 * Linear interpolation:  actual = slope * raw + offset
 *   slope  = (actual2 - actual1) / (raw2 - raw1)
 *   offset = actual1 - slope * raw1                                        */
typedef struct {
    float raw1;     /* Sensor reading at calibration point 1 */
    float actual1;  /* Known physical value at point 1       */
    float raw2;     /* Sensor reading at calibration point 2 */
    float actual2;  /* Known physical value at point 2       */
} sensor_cal_t;

/**
 * @brief Initialize the sensor subsystem (INA219).
 *
 * Must be called after i2c_init().
 *
 * @return ESP_OK on success
 */
esp_err_t sensor_init(void);

/**
 * @brief Set 2-point calibration for the voltage channel.
 *
 * @param cal  Pointer to calibration data
 */
void sensor_set_voltage_cal(const sensor_cal_t *cal);

/**
 * @brief Set 2-point calibration for the ampere channel.
 *
 * @param cal  Pointer to calibration data
 */
void sensor_set_ampere_cal(const sensor_cal_t *cal);

/**
 * @brief Read calibrated voltage (in volts).
 *
 * @param[out] voltage  Result in volts
 * @return ESP_OK on success, or error from INA219 read
 */
esp_err_t sensor_read_voltage(float *voltage);

/**
 * @brief Read calibrated current (in amps).
 *
 * @param[out] ampere  Result in amps
 * @return ESP_OK on success, or error from INA219 read
 */
esp_err_t sensor_read_ampere(float *ampere);

void sensor_get_voltage_cal(sensor_cal_t *cal);
void sensor_get_ampere_cal(sensor_cal_t *cal);

/**
 * @brief Read raw (uncalibrated) voltage from INA219.
 */
esp_err_t sensor_read_raw_voltage(float *raw_voltage);

/**
 * @brief Read raw (uncalibrated) current from INA219.
 */
esp_err_t sensor_read_raw_ampere(float *raw_ampere);

/**
 * @brief Save current calibration to NVS flash.
 */
esp_err_t sensor_save_cal_nvs(void);

/**
 * @brief Load calibration from NVS flash (overwrites #define defaults).
 */
esp_err_t sensor_load_cal_nvs(void);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
