#ifndef _BSP_PCNT_H_
#define _BSP_PCNT_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

#define PCNT_TAG "PCNT"
#define PCNT_INFO(fmt, ...)  ESP_LOGI(PCNT_TAG, fmt, ##__VA_ARGS__)
#define PCNT_DEBUG(fmt, ...) ESP_LOGD(PCNT_TAG, fmt, ##__VA_ARGS__)
#define PCNT_ERROR(fmt, ...) ESP_LOGE(PCNT_TAG, fmt, ##__VA_ARGS__)

/* Opaque handle for a pulse counter channel */
typedef struct bsp_pcnt_ctx *bsp_pcnt_handle_t;

/**
 * @brief Create and start a PCNT-based pulse counter on a GPIO pin.
 *
 * Counts rising edges on the specified pin. The counter runs continuously
 * in hardware with no CPU overhead.
 *
 * @param gpio_num   GPIO pin number (must support input)
 * @param[out] handle  Returns the channel handle
 * @return ESP_OK on success
 */
esp_err_t bsp_pcnt_create(int gpio_num, bsp_pcnt_handle_t *handle);

/**
 * @brief Read the current pulse count and reset it atomically.
 *
 * Returns the number of pulses accumulated since the last call,
 * then clears the counter to zero.
 *
 * @param handle  Channel handle from bsp_pcnt_create()
 * @param[out] count  Number of pulses since last read
 * @return ESP_OK on success
 */
esp_err_t bsp_pcnt_read_and_clear(bsp_pcnt_handle_t handle, int *count);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
