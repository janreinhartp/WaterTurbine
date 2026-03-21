#ifndef _BSP_ADS1115_H_
#define _BSP_ADS1115_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

#define ADS1115_TAG "ADS1115"
#define ADS1115_INFO(fmt, ...)  ESP_LOGI(ADS1115_TAG, fmt, ##__VA_ARGS__)
#define ADS1115_DEBUG(fmt, ...) ESP_LOGD(ADS1115_TAG, fmt, ##__VA_ARGS__)
#define ADS1115_ERROR(fmt, ...) ESP_LOGE(ADS1115_TAG, fmt, ##__VA_ARGS__)

/* Default I2C address (ADDR pin tied to GND) */
#define ADS1115_ADDR_GND  0x48
#define ADS1115_ADDR_VDD  0x49
#define ADS1115_ADDR_SDA  0x4A
#define ADS1115_ADDR_SCL  0x4B

/* Input channel selection (single-ended) */
typedef enum {
    ADS1115_CHANNEL_0 = 0,  /* AIN0 vs GND */
    ADS1115_CHANNEL_1,      /* AIN1 vs GND */
    ADS1115_CHANNEL_2,      /* AIN2 vs GND */
    ADS1115_CHANNEL_3,      /* AIN3 vs GND */
} ads1115_channel_t;

/* Programmable gain amplifier setting */
typedef enum {
    ADS1115_GAIN_6144 = 0,  /* ±6.144 V  (LSB = 187.5 µV) */
    ADS1115_GAIN_4096,      /* ±4.096 V  (LSB = 125.0 µV) */
    ADS1115_GAIN_2048,      /* ±2.048 V  (LSB = 62.5 µV)  */
    ADS1115_GAIN_1024,      /* ±1.024 V  (LSB = 31.25 µV) */
    ADS1115_GAIN_0512,      /* ±0.512 V  (LSB = 15.625 µV)*/
    ADS1115_GAIN_0256,      /* ±0.256 V  (LSB = 7.8125 µV)*/
} ads1115_gain_t;

/**
 * @brief Initialize the ADS1115 on the I2C bus.
 *
 * @param i2c_addr  7-bit I2C address (e.g. ADS1115_ADDR_GND = 0x48)
 * @return ESP_OK on success
 */
esp_err_t ads1115_init(uint8_t i2c_addr);

/**
 * @brief Perform a single-shot read on the specified channel.
 *
 * @param channel  Input channel (0–3, single-ended)
 * @param gain     PGA gain setting
 * @param[out] raw Signed 16-bit conversion result
 * @return ESP_OK on success
 */
esp_err_t ads1115_read_raw(ads1115_channel_t channel, ads1115_gain_t gain, int16_t *raw);

/**
 * @brief Convert a raw ADS1115 reading to voltage (in volts).
 *
 * @param raw   Raw 16-bit signed value from ads1115_read_raw()
 * @param gain  PGA gain that was used during the read
 * @return Voltage in volts
 */
float ads1115_raw_to_voltage(int16_t raw, ads1115_gain_t gain);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
