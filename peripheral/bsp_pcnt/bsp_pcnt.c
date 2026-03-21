#include "bsp_pcnt.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include <stdlib.h>

/* Internal structure backing the opaque handle */
struct bsp_pcnt_ctx {
    pcnt_unit_handle_t unit;
    pcnt_channel_handle_t ch;
};

/* ------------------------------------------------------------------ */
esp_err_t bsp_pcnt_create(int gpio_num, bsp_pcnt_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    struct bsp_pcnt_ctx *ctx = calloc(1, sizeof(struct bsp_pcnt_ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* Create a PCNT unit with a large enough range that we won't overflow
     * between 1-second reads.  Max for ESP32-P4: ±32767. */
    pcnt_unit_config_t unit_cfg = {
        .low_limit  = -1,       /* not used, but required to be < 0 */
        .high_limit = 30000,    /* comfortably above any 1-sec pulse count */
    };
    esp_err_t err = pcnt_new_unit(&unit_cfg, &ctx->unit);
    if (err != ESP_OK) {
        PCNT_ERROR("Failed to create PCNT unit for GPIO%d: %s", gpio_num, esp_err_to_name(err));
        free(ctx);
        return err;
    }

    /* Configure a glitch filter (1 µs) to reject switch bounce / noise */
    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = 1000,
    };
    err = pcnt_unit_set_glitch_filter(ctx->unit, &filter_cfg);
    if (err != ESP_OK) {
        PCNT_ERROR("Failed to set glitch filter: %s", esp_err_to_name(err));
        pcnt_del_unit(ctx->unit);
        free(ctx);
        return err;
    }

    /* Create a channel: count rising edges on the selected GPIO pin */
    pcnt_chan_config_t chan_cfg = {
        .edge_gpio_num  = gpio_num,
        .level_gpio_num = -1,   /* no level gate */
    };
    pcnt_channel_handle_t ch = NULL;
    err = pcnt_new_channel(ctx->unit, &chan_cfg, &ch);
    if (err != ESP_OK) {
        PCNT_ERROR("Failed to create PCNT channel for GPIO%d: %s", gpio_num, esp_err_to_name(err));
        pcnt_del_unit(ctx->unit);
        free(ctx);
        return err;
    }
    ctx->ch = ch;

    /* Count +1 on rising edge, ignore falling edge */
    pcnt_channel_set_edge_action(ch, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                      PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action(ch, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                       PCNT_CHANNEL_LEVEL_ACTION_KEEP);

    /* Enable internal pull-up (YF-DN50 open-collector, hall sensor safety) */
    gpio_set_pull_mode((gpio_num_t)gpio_num, GPIO_PULLUP_ONLY);

    /* Enable and start the unit */
    pcnt_unit_enable(ctx->unit);
    pcnt_unit_clear_count(ctx->unit);
    pcnt_unit_start(ctx->unit);

    *handle = ctx;
    PCNT_INFO("Pulse counter started on GPIO%d", gpio_num);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t bsp_pcnt_read_and_clear(bsp_pcnt_handle_t handle, int *count)
{
    if (handle == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = pcnt_unit_get_count(handle->unit, count);
    if (err != ESP_OK) {
        return err;
    }

    /* Reset counter for the next measurement window */
    pcnt_unit_clear_count(handle->unit);
    return ESP_OK;
}
