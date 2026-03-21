#include "bsp_ds3231.h"
#include "bsp_i2c.h"

/* -------- DS3231 Register Addresses -------- */
#define DS3231_REG_SECONDS  0x00
#define DS3231_REG_MINUTES  0x01
#define DS3231_REG_HOURS    0x02
#define DS3231_REG_DAY      0x03
#define DS3231_REG_DATE     0x04
#define DS3231_REG_MONTH    0x05
#define DS3231_REG_YEAR     0x06
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_STATUS   0x0F
#define DS3231_REG_TEMP_MSB 0x11
#define DS3231_REG_TEMP_LSB 0x12

static i2c_master_dev_handle_t s_dev = NULL;

/* -------- BCD helpers -------- */
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

static uint8_t dec_to_bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10) << 4) | (dec % 10));
}

/* ------------------------------------------------------------------ */
esp_err_t ds3231_init(void)
{
    s_dev = i2c_dev_register(DS3231_ADDR);
    if (s_dev == NULL) {
        DS3231_ERROR("Failed to register I2C device at 0x%02X", DS3231_ADDR);
        return ESP_FAIL;
    }

    /* Verify communication by reading the status register */
    uint8_t status = 0;
    esp_err_t err = i2c_read_reg(s_dev, DS3231_REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        DS3231_ERROR("Failed to read status register: %s", esp_err_to_name(err));
        return err;
    }

    /* Clear OSF (oscillator stop flag) if set, meaning the clock was stopped */
    if (status & 0x80) {
        DS3231_INFO("OSF flag set – clearing");
        err = i2c_write_reg(s_dev, DS3231_REG_STATUS, status & (uint8_t)~0x80);
        if (err != ESP_OK) {
            DS3231_ERROR("Failed to clear OSF: %s", esp_err_to_name(err));
            return err;
        }
    }

    /* Disable square-wave output, enable battery-backed oscillator */
    err = i2c_write_reg(s_dev, DS3231_REG_CONTROL, 0x04);
    if (err != ESP_OK) {
        DS3231_ERROR("Failed to write control register: %s", esp_err_to_name(err));
        return err;
    }

    DS3231_INFO("Initialized (addr=0x%02X, status=0x%02X)", DS3231_ADDR, status);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ds3231_get_time(ds3231_time_t *time)
{
    if (s_dev == NULL || time == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[7] = {0};
    esp_err_t err = i2c_read_reg(s_dev, DS3231_REG_SECONDS, buf, 7);
    if (err != ESP_OK) {
        DS3231_ERROR("Failed to read time registers: %s", esp_err_to_name(err));
        return err;
    }

    time->seconds = bcd_to_dec(buf[0] & 0x7F);
    time->minutes = bcd_to_dec(buf[1] & 0x7F);
    time->hours   = bcd_to_dec(buf[2] & 0x3F);  /* 24-hour mode */
    time->day     = bcd_to_dec(buf[3] & 0x07);
    time->date    = bcd_to_dec(buf[4] & 0x3F);
    time->month   = bcd_to_dec(buf[5] & 0x1F);
    time->year    = 2000 + bcd_to_dec(buf[6]);

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ds3231_set_time(const ds3231_time_t *time)
{
    if (s_dev == NULL || time == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[8];
    buf[0] = DS3231_REG_SECONDS;           /* register start address */
    buf[1] = dec_to_bcd(time->seconds);
    buf[2] = dec_to_bcd(time->minutes);
    buf[3] = dec_to_bcd(time->hours);      /* 24-hour mode (bit 6 = 0) */
    buf[4] = dec_to_bcd(time->day);
    buf[5] = dec_to_bcd(time->date);
    buf[6] = dec_to_bcd(time->month);
    buf[7] = dec_to_bcd((uint8_t)(time->year - 2000));

    esp_err_t err = i2c_write(s_dev, buf, sizeof(buf));
    if (err != ESP_OK) {
        DS3231_ERROR("Failed to set time: %s", esp_err_to_name(err));
        return err;
    }

    DS3231_INFO("Time set to %04u-%02u-%02u %02u:%02u:%02u",
                time->year, time->month, time->date,
                time->hours, time->minutes, time->seconds);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t ds3231_get_temperature(float *temp_c)
{
    if (s_dev == NULL || temp_c == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[2] = {0};
    esp_err_t err = i2c_read_reg(s_dev, DS3231_REG_TEMP_MSB, buf, 2);
    if (err != ESP_OK) {
        DS3231_ERROR("Failed to read temperature: %s", esp_err_to_name(err));
        return err;
    }

    /* MSB = integer part (signed), upper 2 bits of LSB = fraction (0.25°C steps) */
    int8_t  msb = (int8_t)buf[0];
    uint8_t lsb = buf[1] >> 6;
    *temp_c = (float)msb + (float)lsb * 0.25f;

    return ESP_OK;
}
