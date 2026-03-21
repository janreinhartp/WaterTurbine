#include "bsp_ads1115.h"
#include "bsp_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* -------- ADS1115 Register Addresses -------- */
#define ADS1115_REG_CONVERSION 0x00
#define ADS1115_REG_CONFIG     0x01

/* -------- Config register bit positions -------- */
#define ADS1115_CFG_OS_SINGLE  (1 << 15)  /* Start single conversion */

/* MUX: single-ended channels (AINx vs GND) are codes 0b100..0b111 */
#define ADS1115_CFG_MUX_SHIFT  12
#define ADS1115_CFG_MUX_AIN(ch) (((ch) + 4) << ADS1115_CFG_MUX_SHIFT)

/* PGA gain codes occupy bits 11-9 */
#define ADS1115_CFG_PGA_SHIFT  9

/* Mode: 1 = single-shot */
#define ADS1115_CFG_MODE_SINGLE (1 << 8)

/* Data rate: 128 SPS (default, code 0b100) */
#define ADS1115_CFG_DR_128     (0x04 << 5)

/* Comparator off (default) */
#define ADS1115_CFG_COMP_OFF   0x0003

/* Full-scale voltage for each PGA setting (in microvolts) */
static const float s_gain_fs_uv[] = {
    6144000.0f,  /* GAIN_6144 */
    4096000.0f,  /* GAIN_4096 */
    2048000.0f,  /* GAIN_2048 */
    1024000.0f,  /* GAIN_1024 */
     512000.0f,  /* GAIN_0512 */
     256000.0f,  /* GAIN_0256 */
};

static i2c_master_dev_handle_t s_dev = NULL;

/* ------------------------------------------------------------------ */
esp_err_t ads1115_init(uint8_t i2c_addr)
{
    s_dev = i2c_dev_register(i2c_addr);
    if (s_dev == NULL) {
        ADS1115_ERROR("Failed to register I2C device at 0x%02X", i2c_addr);
        return ESP_FAIL;
    }

    /* Verify communication by reading the config register */
    uint8_t buf[2] = {0};
    esp_err_t err = i2c_read_reg(s_dev, ADS1115_REG_CONFIG, buf, 2);
    if (err != ESP_OK) {
        ADS1115_ERROR("Failed to read config register: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t cfg = ((uint16_t)buf[0] << 8) | buf[1];
    ADS1115_INFO("Initialized (addr=0x%02X, default config=0x%04X)", i2c_addr, cfg);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ads1115_read_raw(ads1115_channel_t channel, ads1115_gain_t gain, int16_t *raw)
{
    if (s_dev == NULL || raw == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Build config word: start single-shot conversion on requested channel */
    uint16_t config = ADS1115_CFG_OS_SINGLE
                    | ADS1115_CFG_MUX_AIN(channel)
                    | ((uint16_t)gain << ADS1115_CFG_PGA_SHIFT)
                    | ADS1115_CFG_MODE_SINGLE
                    | ADS1115_CFG_DR_128
                    | ADS1115_CFG_COMP_OFF;

    /* Write config register (3 bytes: reg addr + 2 data bytes) */
    uint8_t wr_buf[3] = {
        ADS1115_REG_CONFIG,
        (uint8_t)(config >> 8),
        (uint8_t)(config & 0xFF),
    };
    esp_err_t err = i2c_write(s_dev, wr_buf, sizeof(wr_buf));
    if (err != ESP_OK) {
        ADS1115_ERROR("Config write failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Wait for conversion to complete (~8 ms at 128 SPS) */
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Read conversion register */
    uint8_t rd_buf[2] = {0};
    err = i2c_read_reg(s_dev, ADS1115_REG_CONVERSION, rd_buf, 2);
    if (err != ESP_OK) {
        ADS1115_ERROR("Conversion read failed: %s", esp_err_to_name(err));
        return err;
    }

    *raw = (int16_t)(((uint16_t)rd_buf[0] << 8) | rd_buf[1]);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
float ads1115_raw_to_voltage(int16_t raw, ads1115_gain_t gain)
{
    if (gain > ADS1115_GAIN_0256) {
        gain = ADS1115_GAIN_2048; /* fallback */
    }

    /* LSB = full_scale_uV / 32768 ; result in volts */
    return (float)raw * s_gain_fs_uv[gain] / 32768.0f / 1000000.0f;
}
