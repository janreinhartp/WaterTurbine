#ifndef _DATA_LOGGER_H_
#define _DATA_LOGGER_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

/**
 * @brief Start the CSV data-logger task.
 *
 * Logs Time, Flow Rate, RPM, Voltage, Ampere, Power to /sdcard/datalog.csv
 * every 60 seconds. All numeric values use 3 decimal places.
 *
 * Requires SD card and all sensors to be initialised before calling.
 *
 * @return ESP_OK on success
 */
esp_err_t data_logger_start(void);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
