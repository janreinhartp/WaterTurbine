#ifndef _PULSE_CONTROLLER_H_
#define _PULSE_CONTROLLER_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
#include <stdbool.h>
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

/**
 * @brief Initialize the pulse-based sensors (flow rate + RPM).
 *
 * Creates two PCNT channels:
 *   - Flow rate sensor (YF-DN50) on GPIO4
 *   - RPM hall sensor on GPIO5
 *
 * Must be called after GPIO subsystem is ready.
 *
 * @return ESP_OK on success
 */
esp_err_t pulse_controller_init(void);

/**
 * @brief Sample and compute current flow rate and RPM.
 *
 * Call this periodically (e.g. every 1 second). It reads the pulse counts
 * accumulated since the last call and converts them to physical units.
 *
 * @param[out] flow_lpm   Flow rate in liters per minute (NULL to skip)
 * @param[out] rpm        Revolutions per minute (NULL to skip)
 * @return ESP_OK on success
 */
esp_err_t pulse_controller_read(float *flow_lpm, float *rpm);

/**
 * @brief Set the flow sensor calibration factor.
 *
 * The YF-DN50 datasheet specifies: frequency (Hz) = factor * flow (L/min).
 * Default: 3.5 (can vary per unit — calibrate to a known volume).
 *
 * @param factor  Pulses-per-second per liter-per-minute
 */
void pulse_controller_set_flow_factor(float factor);

/**
 * @brief Set the number of pulses per revolution for the RPM sensor.
 *
 * Default: 1.0 (single-magnet hall sensor = 1 pulse per revolution).
 *
 * @param pulses_per_rev  Pulses generated per full revolution
 */
void pulse_controller_set_rpm_ppr(float pulses_per_rev);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
