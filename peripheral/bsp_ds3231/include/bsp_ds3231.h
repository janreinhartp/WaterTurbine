#ifndef _BSP_DS3231_H_
#define _BSP_DS3231_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdbool.h>
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

#define DS3231_TAG "DS3231"
#define DS3231_INFO(fmt, ...)  ESP_LOGI(DS3231_TAG, fmt, ##__VA_ARGS__)
#define DS3231_DEBUG(fmt, ...) ESP_LOGD(DS3231_TAG, fmt, ##__VA_ARGS__)
#define DS3231_ERROR(fmt, ...) ESP_LOGE(DS3231_TAG, fmt, ##__VA_ARGS__)

/* DS3231 I2C address (fixed) */
#define DS3231_ADDR  0x68

/* Date/time structure */
typedef struct {
    uint8_t  seconds;   /* 0–59  */
    uint8_t  minutes;   /* 0–59  */
    uint8_t  hours;     /* 0–23  */
    uint8_t  day;       /* 1–7 (day of week) */
    uint8_t  date;      /* 1–31  */
    uint8_t  month;     /* 1–12  */
    uint16_t year;      /* 2000–2099 */
} ds3231_time_t;

/**
 * @brief Initialize the DS3231 RTC on the I2C bus.
 *
 * Registers the device and verifies communication.
 *
 * @return ESP_OK on success
 */
esp_err_t ds3231_init(void);

/**
 * @brief Read the current date and time from the RTC.
 *
 * @param[out] time  Pointer to receive the current time
 * @return ESP_OK on success
 */
esp_err_t ds3231_get_time(ds3231_time_t *time);

/**
 * @brief Set the date and time on the RTC.
 *
 * @param time  Pointer to the time to set
 * @return ESP_OK on success
 */
esp_err_t ds3231_set_time(const ds3231_time_t *time);

/**
 * @brief Read the DS3231 on-chip temperature sensor.
 *
 * Resolution: 0.25°C.
 *
 * @param[out] temp_c  Temperature in degrees Celsius
 * @return ESP_OK on success
 */
esp_err_t ds3231_get_temperature(float *temp_c);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
