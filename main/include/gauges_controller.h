#ifndef _GAUGES_CONTROLLER_H_
#define _GAUGES_CONTROLLER_H_

#include "esp_err.h"

esp_err_t gauges_controller_start(void);

/**
 * @brief Reset chart ready flag to force re-initialization on next Data screen load.
 */
void gauges_controller_reset_chart(void);

/**
 * @brief Immediately initialize/refresh the Data page chart styling.
 */
void gauges_controller_refresh_chart(void);

#endif
