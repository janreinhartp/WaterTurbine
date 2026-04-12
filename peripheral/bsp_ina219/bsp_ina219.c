#include "bsp_ina219.h"
#include "bsp_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* -------- INA219 Register Addresses -------- */
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNT_VOLT   0x01
#define INA219_REG_BUS_VOLT     0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

/* -------- Config register bit positions -------- */
#define INA219_CFG_RST          (1 << 15)
#define INA219_CFG_BRNG_SHIFT   13
#define INA219_CFG_PGA_SHIFT    11
#define INA219_CFG_BADC_SHIFT   7
#define INA219_CFG_SADC_SHIFT   3
#define INA219_CFG_MODE_SHIFT   0

/* Bus voltage register: voltage is in bits [15:3], LSB = 4 mV */
#define INA219_BUS_VOLT_LSB_MV  4.0f

/* Shunt voltage register: LSB = 10 µV */
#define INA219_SHUNT_VOLT_LSB_UV 10.0f

static i2c_master_dev_handle_t s_dev = NULL;
static float s_current_lsb = 0.0f;  /* Amps per LSB of the current register */
static float s_power_lsb   = 0.0f;  /* Watts per LSB of the power register  */

/* ------------------------------------------------------------------ */
static esp_err_t ina219_write_reg16(uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = {
        reg,
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF),
    };
    return i2c_write(s_dev, buf, sizeof(buf));
}

static esp_err_t ina219_read_reg16(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2] = {0};
    esp_err_t err = i2c_read_reg(s_dev, reg, buf, 2);
    if (err != ESP_OK) {
        return err;
    }
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ina219_init(uint8_t i2c_addr, const ina219_config_t *config)
{
    s_dev = i2c_dev_register(i2c_addr);
    if (s_dev == NULL) {
        INA219_ERROR("Failed to register I2C device at 0x%02X", i2c_addr);
        return ESP_FAIL;
    }

    /* Reset the device */
    esp_err_t err = ina219_write_reg16(INA219_REG_CONFIG, INA219_CFG_RST);
    if (err != ESP_OK) {
        INA219_ERROR("Reset failed: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Apply defaults if no config provided */
    ina219_brng_t brng = INA219_BRNG_32V;
    ina219_pga_t  pga  = INA219_PGA_320MV;
    ina219_adc_t  badc = INA219_ADC_12BIT;
    ina219_adc_t  sadc = INA219_ADC_12BIT;
    ina219_mode_t mode = INA219_MODE_SHUNT_BUS_CONT;
    float shunt_ohms   = 0.1f;
    float max_current   = 3.2f;

    if (config) {
        brng = config->brng;
        pga  = config->pga;
        badc = config->badc;
        sadc = config->sadc;
        mode = config->mode;
        if (config->shunt_ohms > 0.0f) shunt_ohms = config->shunt_ohms;
        if (config->max_current_a > 0.0f) max_current = config->max_current_a;
    }

    /* Calculate calibration value per INA219 datasheet:
     *   Current_LSB = Max_Current / 2^15
     *   Cal = trunc(0.04096 / (Current_LSB * R_shunt))
     *   Power_LSB = 20 * Current_LSB                     */
    s_current_lsb = max_current / 32768.0f;
    s_power_lsb   = 20.0f * s_current_lsb;

    uint16_t cal = (uint16_t)(0.04096f / (s_current_lsb * shunt_ohms));

    err = ina219_write_reg16(INA219_REG_CALIBRATION, cal);
    if (err != ESP_OK) {
        INA219_ERROR("Calibration write failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Build and write config register */
    uint16_t cfg = ((uint16_t)brng << INA219_CFG_BRNG_SHIFT)
                 | ((uint16_t)pga  << INA219_CFG_PGA_SHIFT)
                 | ((uint16_t)badc << INA219_CFG_BADC_SHIFT)
                 | ((uint16_t)sadc << INA219_CFG_SADC_SHIFT)
                 | ((uint16_t)mode << INA219_CFG_MODE_SHIFT);

    err = ina219_write_reg16(INA219_REG_CONFIG, cfg);
    if (err != ESP_OK) {
        INA219_ERROR("Config write failed: %s", esp_err_to_name(err));
        return err;
    }

    INA219_INFO("Initialized (addr=0x%02X, config=0x%04X, cal=%u, current_lsb=%.6f A)",
                i2c_addr, cfg, cal, s_current_lsb);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ina219_read_bus_voltage(float *voltage)
{
    if (s_dev == NULL || voltage == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw;
    esp_err_t err = ina219_read_reg16(INA219_REG_BUS_VOLT, &raw);
    if (err != ESP_OK) {
        return err;
    }

    /* Bits [15:3] contain the voltage value, LSB = 4 mV */
    int16_t value = (int16_t)(raw >> 3);
    *voltage = (float)value * INA219_BUS_VOLT_LSB_MV / 1000.0f;
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ina219_read_shunt_voltage(float *shunt_mv)
{
    if (s_dev == NULL || shunt_mv == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw;
    esp_err_t err = ina219_read_reg16(INA219_REG_SHUNT_VOLT, &raw);
    if (err != ESP_OK) {
        return err;
    }

    /* Signed 16-bit value, LSB = 10 µV → convert to mV */
    int16_t value = (int16_t)raw;
    *shunt_mv = (float)value * INA219_SHUNT_VOLT_LSB_UV / 1000.0f;
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ina219_read_current(float *current)
{
    if (s_dev == NULL || current == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw;
    esp_err_t err = ina219_read_reg16(INA219_REG_CURRENT, &raw);
    if (err != ESP_OK) {
        return err;
    }

    int16_t value = (int16_t)raw;
    *current = (float)value * s_current_lsb;
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ina219_read_power(float *power)
{
    if (s_dev == NULL || power == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw;
    esp_err_t err = ina219_read_reg16(INA219_REG_POWER, &raw);
    if (err != ESP_OK) {
        return err;
    }

    /* Power register is unsigned 16-bit */
    *power = (float)raw * s_power_lsb;
    return ESP_OK;
}
