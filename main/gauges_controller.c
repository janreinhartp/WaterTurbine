#include "gauges_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_lvgl_port.h"

#include "ui.h"

#define GAUGES_CTRL_TAG   "GAUGE_CTRL"
#define GAUGES_CTRL_PERIOD_MS 1000
#define CHART_POINT_COUNT 20

static TaskHandle_t s_gauges_task = NULL;

static bool s_chart_ready = false;

static uint32_t rand_range_u32(uint32_t min, uint32_t max)
{
    return min + (esp_random() % (max - min + 1));
}

static uint32_t to_percent_u32(uint32_t value, uint32_t min, uint32_t max)
{
    if (value <= min) {
        return 0;
    }
    if (value >= max) {
        return 100;
    }
    return ((value - min) * 100) / (max - min);
}

/* One-time reconfiguration of the Data page chart for live plotting */
static void init_data_chart(void)
{
    if (s_chart_ready || ui_Chart1 == NULL) {
        return;
    }

    /* Switch from BAR to LINE for time-series display */
    lv_chart_set_type(ui_Chart1, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_Chart1, CHART_POINT_COUNT);
    lv_chart_set_update_mode(ui_Chart1, LV_CHART_UPDATE_MODE_SHIFT);

    /* Adjust Y-axis ranges:
     *  Primary   Y: 0-30  (Voltage 10-30 V)
     *  Secondary Y: 0-20  (Ampere 0-20 A, FlowRate 0-20 L/M) */
    lv_chart_set_range(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y, 0, 30);
    lv_chart_set_range(ui_Chart1, LV_CHART_AXIS_SECONDARY_Y, 0, 20);

    /* Remove the demo series created by SquareLine Studio */
    lv_chart_series_t *ser;
    while ((ser = lv_chart_get_series_next(ui_Chart1, NULL)) != NULL) {
        lv_chart_remove_series(ui_Chart1, ser);
    }

    /* Add live series: FlowRate (green), Voltage (red), Ampere (yellow) */
    lv_chart_add_series(ui_Chart1, lv_color_hex(0x13C56D),
                        LV_CHART_AXIS_SECONDARY_Y);
    lv_chart_add_series(ui_Chart1, lv_color_hex(0xD62323),
                        LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_add_series(ui_Chart1, lv_color_hex(0xF3E913),
                        LV_CHART_AXIS_SECONDARY_Y);

    s_chart_ready = true;
}

static void update_values(void)
{
    /* Generate pseudo sensor values (shared by Gauges + Data pages) */
    uint32_t voltage_tenths = rand_range_u32(100, 300);  /* 10.0 V .. 30.0 V */
    uint32_t ampere_tenths  = rand_range_u32(0, 200);    /*  0.0 A .. 20.0 A */
    uint32_t rpm            = rand_range_u32(200, 1200);  /* 200 .. 1200 RPM  */
    uint32_t power_w        = rand_range_u32(0, 500);     /*   0 .. 500 W     */
    uint32_t flow_tenths    = rand_range_u32(0, 200);    /*  0.0 .. 20.0 L/M */

    /* -------- Gauges screen arcs + labels -------- */
    if (ui_gVoltage && ui_gVoltageValue) {
        lv_arc_set_value(ui_gVoltage, to_percent_u32(voltage_tenths, 100, 300));
        lv_label_set_text_fmt(ui_gVoltageValue, "%lu.%lu V",
                              voltage_tenths / 10, voltage_tenths % 10);
    }

    if (ui_gAmpere && ui_gAmpereValue) {
        lv_arc_set_value(ui_gAmpere, to_percent_u32(ampere_tenths, 0, 200));
        lv_label_set_text_fmt(ui_gAmpereValue, "%lu.%lu A",
                              ampere_tenths / 10, ampere_tenths % 10);
    }

    if (ui_gRPM && ui_gRpmValue) {
        lv_arc_set_value(ui_gRPM, to_percent_u32(rpm, 200, 1200));
        lv_label_set_text_fmt(ui_gRpmValue, "%lu RPM", rpm);
    }

    if (ui_gPower && ui_gPowerValue) {
        lv_arc_set_value(ui_gPower, to_percent_u32(power_w, 0, 500));
        lv_label_set_text_fmt(ui_gPowerValue, "%lu W", power_w);
    }

    if (ui_gWaterFlow && ui_gWaterFlowValue) {
        lv_arc_set_value(ui_gWaterFlow, to_percent_u32(flow_tenths, 0, 200));
        lv_label_set_text_fmt(ui_gWaterFlowValue, "%lu.%lu L/M",
                              flow_tenths / 10, flow_tenths % 10);
    }

    /* -------- Data screen labels -------- */
    if (ui_lblFlowRateValue) {
        lv_label_set_text_fmt(ui_lblFlowRateValue, "%lu.%lu L/M",
                              flow_tenths / 10, flow_tenths % 10);
    }
    if (ui_lblVoltageValue) {
        lv_label_set_text_fmt(ui_lblVoltageValue, "%lu.%lu V",
                              voltage_tenths / 10, voltage_tenths % 10);
    }
    if (ui_lblAmpereValue) {
        lv_label_set_text_fmt(ui_lblAmpereValue, "%lu.%lu A",
                              ampere_tenths / 10, ampere_tenths % 10);
    }
    if (ui_lblPowerValue) {
        lv_label_set_text_fmt(ui_lblPowerValue, "%lu W", power_w);
    }
    if (ui_lblRpmValue) {
        lv_label_set_text_fmt(ui_lblRpmValue, "%lu RPM", rpm);
    }

    /* -------- Data screen chart (FlowRate, Voltage, Ampere only) -------- */
    init_data_chart();
    if (s_chart_ready && ui_Chart1) {
        /* Re-acquire series pointers each cycle to avoid stale references */
        lv_chart_series_t *sf = lv_chart_get_series_next(ui_Chart1, NULL);
        lv_chart_series_t *sv = sf ? lv_chart_get_series_next(ui_Chart1, sf) : NULL;
        lv_chart_series_t *sa = sv ? lv_chart_get_series_next(ui_Chart1, sv) : NULL;
        if (sf && sv && sa) {
            lv_chart_set_next_value(ui_Chart1, sf, (int32_t)(flow_tenths / 10));
            lv_chart_set_next_value(ui_Chart1, sv, (int32_t)(voltage_tenths / 10));
            lv_chart_set_next_value(ui_Chart1, sa, (int32_t)(ampere_tenths / 10));
        }
    }
}

static void gauges_task(void *arg)
{
    (void)arg;

    while (1) {
        if (lvgl_port_lock(200)) {
            update_values();
            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(GAUGES_CTRL_PERIOD_MS));
    }
}

esp_err_t gauges_controller_start(void)
{
    if (s_gauges_task != NULL) {
        return ESP_OK;
    }

    BaseType_t rc = xTaskCreate(
        gauges_task,
        "gauges_ctrl",
        8192,
        NULL,
        tskIDLE_PRIORITY + 2,
        &s_gauges_task);

    if (rc != pdPASS) {
        ESP_LOGE(GAUGES_CTRL_TAG, "Failed to create gauges controller task");
        return ESP_FAIL;
    }

    ESP_LOGI(GAUGES_CTRL_TAG, "Gauges controller started (period=%d ms)", GAUGES_CTRL_PERIOD_MS);
    return ESP_OK;
}
