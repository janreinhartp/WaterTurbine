
#include "pulse_controller.h"
#include "bsp_pcnt.h"
#include "esp_log.h"

#define PULSE_CTRL_TAG "PULSE_CTRL"
#define PULSE_CTRL_INFO(fmt, ...)  ESP_LOGI(PULSE_CTRL_TAG, fmt, ##__VA_ARGS__)
#define PULSE_CTRL_ERROR(fmt, ...) ESP_LOGE(PULSE_CTRL_TAG, fmt, ##__VA_ARGS__)

/* GPIO assignments */
#define FLOW_GPIO  4
#define RPM_GPIO   5

/* PCNT channel handles */
static bsp_pcnt_handle_t s_flow_ch = NULL;
static bsp_pcnt_handle_t s_rpm_ch  = NULL;

/* Conversion factors (adjustable at runtime) */
static float s_flow_factor   = 3.5f;   /* YF-DN50: Hz per L/min      */
static float s_rpm_ppr       = 1.0f;   /* Hall sensor: pulses per rev */

/* Measurement window — the caller is expected to call read() once per second.
 * If the period changes, adjust this constant.  */
#define SAMPLE_PERIOD_S  1.0f

/* ------------------------------------------------------------------ */
esp_err_t pulse_controller_init(void)
{
    esp_err_t err;

    err = bsp_pcnt_create(FLOW_GPIO, &s_flow_ch);
    if (err != ESP_OK) {
        PULSE_CTRL_ERROR("Flow sensor PCNT init failed (GPIO%d): %s",
                         FLOW_GPIO, esp_err_to_name(err));
        return err;
    }

    err = bsp_pcnt_create(RPM_GPIO, &s_rpm_ch);
    if (err != ESP_OK) {
        PULSE_CTRL_ERROR("RPM sensor PCNT init failed (GPIO%d): %s",
                         RPM_GPIO, esp_err_to_name(err));
        return err;
    }

    PULSE_CTRL_INFO("Pulse controller initialized (flow=GPIO%d, rpm=GPIO%d)",
                    FLOW_GPIO, RPM_GPIO);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t pulse_controller_read(float *flow_lpm, float *rpm)
{
    int flow_count = 0;
    int rpm_count  = 0;

    if (s_flow_ch) {
        bsp_pcnt_read_and_clear(s_flow_ch, &flow_count);
    }
    if (s_rpm_ch) {
        bsp_pcnt_read_and_clear(s_rpm_ch, &rpm_count);
    }

    /* Flow: frequency (Hz) = pulses / sample_period
     *       flow (L/min)   = frequency / factor            */
    if (flow_lpm) {
        float freq = (float)flow_count / SAMPLE_PERIOD_S;
        *flow_lpm = (s_flow_factor > 0.0f) ? (freq / s_flow_factor) : 0.0f;
    }

    /* RPM: revolutions/sec = pulses / sample_period / ppr
     *      RPM             = revolutions/sec * 60           */
    if (rpm) {
        float rps = (s_rpm_ppr > 0.0f)
                  ? ((float)rpm_count / SAMPLE_PERIOD_S / s_rpm_ppr)
                  : 0.0f;
        *rpm = rps * 60.0f;
    }

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
void pulse_controller_set_flow_factor(float factor)
{
    if (factor > 0.0f) {
        s_flow_factor = factor;
        PULSE_CTRL_INFO("Flow factor set to %.2f Hz/(L/min)", factor);
    }
}

void pulse_controller_set_rpm_ppr(float pulses_per_rev)
{
    if (pulses_per_rev > 0.0f) {
        s_rpm_ppr = pulses_per_rev;
        PULSE_CTRL_INFO("RPM pulses-per-rev set to %.1f", pulses_per_rev);
    }
}
