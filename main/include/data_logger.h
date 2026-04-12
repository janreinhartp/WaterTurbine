#ifndef _DATA_LOGGER_H_
#define _DATA_LOGGER_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

/**
 * @brief Start the CSV data-logger task.
 *
 * Logs Timestamp, Voltage, Ampere, Power, RPM, Flow to /sdcard/log.csv
 * every 1 second. Also outputs each row to serial console.
 *
 * Requires all sensors to be initialised before calling.
 * SD card mount is checked at runtime before each write.
 *
 * @return ESP_OK on success
 */
esp_err_t data_logger_start(void);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
