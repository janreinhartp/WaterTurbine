#ifndef _BSP_INA219_H_
#define _BSP_INA219_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

#define INA219_TAG "INA219"
#define INA219_INFO(fmt, ...)  ESP_LOGI(INA219_TAG, fmt, ##__VA_ARGS__)
#define INA219_DEBUG(fmt, ...) ESP_LOGD(INA219_TAG, fmt, ##__VA_ARGS__)
#define INA219_ERROR(fmt, ...) ESP_LOGE(INA219_TAG, fmt, ##__VA_ARGS__)

/* I2C addresses (selected by A0/A1 pins) */
#define INA219_ADDR_GND_GND  0x40  /* A0=GND, A1=GND */
#define INA219_ADDR_VS_GND   0x41  /* A0=VS,  A1=GND */
#define INA219_ADDR_GND_VS   0x44  /* A0=GND, A1=VS  */
#define INA219_ADDR_VS_VS    0x45  /* A0=VS,  A1=VS  */

/* Bus voltage range */
typedef enum {
    INA219_BRNG_16V = 0,  /* 16 V FSR */
    INA219_BRNG_32V = 1,  /* 32 V FSR (default) */
} ina219_brng_t;

/* PGA gain / shunt voltage range */
typedef enum {
    INA219_PGA_40MV  = 0,  /* Gain /1, ±40 mV  */
    INA219_PGA_80MV  = 1,  /* Gain /2, ±80 mV  */
    INA219_PGA_160MV = 2,  /* Gain /4, ±160 mV */
    INA219_PGA_320MV = 3,  /* Gain /8, ±320 mV (default) */
} ina219_pga_t;

/* ADC resolution / averaging */
typedef enum {
    INA219_ADC_9BIT    = 0x00,  /*  9-bit,  84 µs */
    INA219_ADC_10BIT   = 0x01,  /* 10-bit, 148 µs */
    INA219_ADC_11BIT   = 0x02,  /* 11-bit, 276 µs */
    INA219_ADC_12BIT   = 0x03,  /* 12-bit, 532 µs (default) */
    INA219_ADC_2SAMP   = 0x09,  /* 2 samples,  1.06 ms */
    INA219_ADC_4SAMP   = 0x0A,  /* 4 samples,  2.13 ms */
    INA219_ADC_8SAMP   = 0x0B,  /* 8 samples,  4.26 ms */
    INA219_ADC_16SAMP  = 0x0C,  /* 16 samples, 8.51 ms */
    INA219_ADC_32SAMP  = 0x0D,  /* 32 samples, 17.02 ms */
    INA219_ADC_64SAMP  = 0x0E,  /* 64 samples, 34.05 ms */
    INA219_ADC_128SAMP = 0x0F,  /* 128 samples, 68.10 ms */
} ina219_adc_t;

/* Operating mode */
typedef enum {
    INA219_MODE_POWER_DOWN       = 0,
    INA219_MODE_SHUNT_TRIG       = 1,
    INA219_MODE_BUS_TRIG         = 2,
    INA219_MODE_SHUNT_BUS_TRIG   = 3,
    INA219_MODE_ADC_OFF          = 4,
    INA219_MODE_SHUNT_CONT       = 5,
    INA219_MODE_BUS_CONT         = 6,
    INA219_MODE_SHUNT_BUS_CONT   = 7,  /* default */
} ina219_mode_t;

/* Configuration structure */
typedef struct {
    ina219_brng_t brng;       /* Bus voltage range */
    ina219_pga_t  pga;        /* Shunt voltage PGA gain */
    ina219_adc_t  badc;       /* Bus ADC resolution/averaging */
    ina219_adc_t  sadc;       /* Shunt ADC resolution/averaging */
    ina219_mode_t mode;       /* Operating mode */
    float         shunt_ohms; /* Shunt resistor value in ohms */
    float         max_current_a; /* Expected maximum current in amps */
} ina219_config_t;

/**
 * @brief Initialize the INA219 on the I2C bus.
 *
 * @param i2c_addr  7-bit I2C address (e.g. INA219_ADDR_GND_GND = 0x40)
 * @param config    Pointer to configuration (NULL for defaults: 32V, ±320mV, 12-bit, 0.1Ω, 3.2A)
 * @return ESP_OK on success
 */
esp_err_t ina219_init(uint8_t i2c_addr, const ina219_config_t *config);

/**
 * @brief Read the bus voltage (in volts).
 *
 * @param[out] voltage  Bus voltage in volts
 * @return ESP_OK on success
 */
esp_err_t ina219_read_bus_voltage(float *voltage);

/**
 * @brief Read the shunt voltage (in millivolts).
 *
 * @param[out] shunt_mv  Shunt voltage in millivolts
 * @return ESP_OK on success
 */
esp_err_t ina219_read_shunt_voltage(float *shunt_mv);

/**
 * @brief Read the current (in amps) using the hardware current register.
 *
 * @param[out] current  Current in amps
 * @return ESP_OK on success
 */
esp_err_t ina219_read_current(float *current);

/**
 * @brief Read the power (in watts) using the hardware power register.
 *
 * @param[out] power  Power in watts
 * @return ESP_OK on success
 */
esp_err_t ina219_read_power(float *power);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
