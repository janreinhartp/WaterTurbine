#ifndef _SENSOR_H_
#define _SENSOR_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
#include <stdbool.h>
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

/* 2-point calibration data: maps raw ADC voltage to actual physical value.
 * Linear interpolation:  actual = slope * raw + offset
 *   slope  = (actual2 - actual1) / (raw2 - raw1)
 *   offset = actual1 - slope * raw1                                        */
typedef struct {
    float raw1;     /* ADC voltage at calibration point 1 */
    float actual1;  /* Known physical value at point 1    */
    float raw2;     /* ADC voltage at calibration point 2 */
    float actual2;  /* Known physical value at point 2    */
} sensor_cal_t;

/**
 * @brief Initialize the sensor subsystem (ADS1115).
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
 * @return ESP_OK on success, or error from ADS1115 read
 */
esp_err_t sensor_read_voltage(float *voltage);

/**
 * @brief Read calibrated current (in amps).
 *
 * @param[out] ampere  Result in amps
 * @return ESP_OK on success, or error from ADS1115 read
 */
esp_err_t sensor_read_ampere(float *ampere);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
