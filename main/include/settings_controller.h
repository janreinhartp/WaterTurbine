#ifndef _SETTINGS_CONTROLLER_H_
#define _SETTINGS_CONTROLLER_H_

#include "esp_err.h"

/**
 * @brief Called at the end of ui_Settings_screen_init() to set up
 *        calibration page navigation, event handlers, and initial values.
 */
void settings_controller_on_screen_init(void);

#endif
