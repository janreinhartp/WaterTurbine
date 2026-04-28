#include "gauges_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_lvgl_port.h"

#include "ui.h"
#include "sensor.h"
#include "pulse_controller.h"
#include "bsp_ds3231.h"

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

/* One-time reconfiguration of the Data page chart for live plotting.
 * The UI creates 4 series with small external demo arrays.
 * We must remove them and recreate with internal LVGL-managed arrays
 * so that CHART_POINT_COUNT works safely. */
static void init_data_chart(void)
{
    if (s_chart_ready || ui_Chart1 == NULL) {
        return;
    }

    /* Remove all SquareLine demo series (they use small external arrays) */
    lv_chart_series_t *ser;
    while ((ser = lv_chart_get_series_next(ui_Chart1, NULL)) != NULL) {
        lv_chart_remove_series(ui_Chart1, ser);
    }

    lv_chart_set_point_count(ui_Chart1, CHART_POINT_COUNT);
    lv_chart_set_update_mode(ui_Chart1, LV_CHART_UPDATE_MODE_SHIFT);

    /* Re-add series for Chart1: Voltage(red/prim), Ampere(blue/prim), Power(purple/sec) */
    lv_chart_add_series(ui_Chart1, lv_color_hex(0xF60707),
                        LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_add_series(ui_Chart1, lv_color_hex(0x0D06E9),
                        LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_add_series(ui_Chart1, lv_color_hex(0x9B59B6),
                        LV_CHART_AXIS_SECONDARY_Y);

    /* Style the series lines */
    lv_obj_set_style_line_width(ui_Chart1, 5, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui_Chart1, true, LV_PART_ITEMS | LV_STATE_DEFAULT);

    /* ---------- Style Chart1 ---------- */
    lv_obj_set_style_bg_color(ui_Chart1, lv_color_hex(0x1A1A2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Chart1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Chart1, lv_color_hex(0x2D2D44), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Chart1, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_Chart1, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_Chart1, lv_color_hex(0x2D2D44), LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ---------- Init Chart2: Flow(yellow/prim), RPM(grey/sec) ---------- */
    if (ui_Chart2 != NULL) {
        lv_chart_series_t *ser2;
        while ((ser2 = lv_chart_get_series_next(ui_Chart2, NULL)) != NULL) {
            lv_chart_remove_series(ui_Chart2, ser2);
        }
        lv_chart_set_point_count(ui_Chart2, CHART_POINT_COUNT);
        lv_chart_set_update_mode(ui_Chart2, LV_CHART_UPDATE_MODE_SHIFT);
        /* Enable horizontal scrolling so the user can pan back through history */
        lv_obj_add_flag(ui_Chart2, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(ui_Chart2, LV_DIR_HOR);
        lv_chart_add_series(ui_Chart2, lv_color_hex(0xF3E913), LV_CHART_AXIS_PRIMARY_Y);
        lv_chart_add_series(ui_Chart2, lv_color_hex(0x808080), LV_CHART_AXIS_SECONDARY_Y);
        lv_obj_set_style_line_width(ui_Chart2, 5, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_line_rounded(ui_Chart2, true, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_Chart2, lv_color_hex(0x1A1A2E), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_Chart2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_Chart2, lv_color_hex(0x2D2D44), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_Chart2, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(ui_Chart2, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_color(ui_Chart2, lv_color_hex(0x2D2D44), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* ---------- Style right-side data cards ---------- */
    /* Helper macro: style a data card container + its labels */
    #define STYLE_DATA_CARD(container, lbl_name, lbl_val, accent_color, y_pos) \
        do { \
            lv_obj_set_width(container, 244); \
            lv_obj_set_height(container, 68); \
            lv_obj_set_x(container, 370); \
            lv_obj_set_y(container, y_pos); \
            lv_obj_set_style_bg_color(container, lv_color_hex(0x1A1A2E), LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_bg_opa(container, 240, LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_radius(container, 10, LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_border_color(container, lv_color_hex(accent_color), LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_border_opa(container, 255, LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_border_width(container, 3, LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_border_side(container, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_pad_left(container, 12, LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_pad_top(container, 6, LV_PART_MAIN | LV_STATE_DEFAULT); \
            /* Label name: small, accent color */ \
            lv_obj_set_x(lbl_name, 0); \
            lv_obj_set_y(lbl_name, -16); \
            lv_obj_set_align(lbl_name, LV_ALIGN_LEFT_MID); \
            lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_text_color(lbl_name, lv_color_hex(accent_color), LV_PART_MAIN | LV_STATE_DEFAULT); \
            /* Value: large, white */ \
            lv_obj_set_x(lbl_val, 0); \
            lv_obj_set_y(lbl_val, 14); \
            lv_obj_set_align(lbl_val, LV_ALIGN_LEFT_MID); \
            lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT); \
            lv_obj_set_style_text_color(lbl_val, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT); \
        } while (0)

    /* Cards: spaced 74px apart, starting at y=-252 from center */
    STYLE_DATA_CARD(ui_Voltage,  ui_lblVoltage,  ui_lblVoltageValue,  0xF60707, -252);
    STYLE_DATA_CARD(ui_Ampere,   ui_lblAmpere,   ui_lblAmpereValue,   0x0D06E9, -178);
    STYLE_DATA_CARD(ui_Power,    ui_lblPower,    ui_lblPowerValue,     0x9B59B6, -104);
    STYLE_DATA_CARD(ui_FlowRate, ui_lblFlowRate, ui_lblFlowRateValue,  0xF3E913,  -30);
    STYLE_DATA_CARD(ui_RPM,      ui_lblRpm,      ui_lblRpmValue,       0x808080,   44);

    #undef STYLE_DATA_CARD

    /* Time card — wider to fit "12-Apr-2026 14:35:07" */
    lv_obj_set_width(ui_RPM1, 244);
    lv_obj_set_height(ui_RPM1, 68);
    lv_obj_set_x(ui_RPM1, 370);
    lv_obj_set_y(ui_RPM1, 118);
    lv_obj_set_style_bg_color(ui_RPM1, lv_color_hex(0x1A1A2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_RPM1, 240, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_RPM1, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_RPM1, lv_color_hex(0x2ECC71), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_RPM1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_RPM1, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_RPM1, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_RPM1, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_RPM1, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_x(ui_lblRpm1, 0);
    lv_obj_set_y(ui_lblRpm1, -16);
    lv_obj_set_align(ui_lblRpm1, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblRpm1, "DATE / TIME");
    lv_obj_set_style_text_font(ui_lblRpm1, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblRpm1, lv_color_hex(0x2ECC71), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_x(ui_lblTimeValue, 0);
    lv_obj_set_y(ui_lblTimeValue, 14);
    lv_obj_set_align(ui_lblTimeValue, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_lblTimeValue, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblTimeValue, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Resize and restyle BACK button */
    lv_obj_set_width(ui_btnDataBack, 244);
    lv_obj_set_height(ui_btnDataBack, 60);
    lv_obj_set_x(ui_btnDataBack, 370);
    lv_obj_set_y(ui_btnDataBack, 210);
    lv_obj_set_style_radius(ui_btnDataBack, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_btnDataBack, lv_color_hex(0x1A1A2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_btnDataBack, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_btnDataBack, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_btnDataBack, lv_color_hex(0xD34D4D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_btnDataBack, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_btnDataBack, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_btnDataBack, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_btnDataBack, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_btnDataBack, &lv_font_montserrat_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_btnDataBack, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_btnDataBack, lv_color_hex(0x2D2D44), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_btnDataBack, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    /* Dark screen background */
    lv_obj_set_style_bg_color(ui_Data, lv_color_hex(0x0E0E1A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Data, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_chart_ready = true;
}

/* Read all sensors and pre-compute display values (called WITHOUT LVGL lock). */
typedef struct {
    uint32_t voltage_milli;
    uint32_t ampere_milli;
    uint32_t power_milli;
    uint32_t flow_milli;
    uint32_t rpm;
    uint8_t  time_h, time_m, time_s;
    uint8_t  date_day, date_month;
    uint16_t date_year;
    bool     time_valid;
} sensor_snapshot_t;

static void read_sensors(sensor_snapshot_t *snap)
{
    /* ---- Read real sensors for voltage and ampere ---- */
    float voltage_f = 0.0f;
    float ampere_f  = 0.0f;

    if (sensor_read_voltage(&voltage_f) != ESP_OK) {
        voltage_f = 0.0f;
    }
    if (sensor_read_ampere(&ampere_f) != ESP_OK) {
        ampere_f = 0.0f;
    }

    /* Clamp to display range */
    if (voltage_f < 0.0f) voltage_f = 0.0f;
    if (ampere_f  < 0.0f) ampere_f  = 0.0f;

    snap->voltage_milli = (uint32_t)(voltage_f * 1000.0f + 0.5f);
    snap->ampere_milli  = (uint32_t)(ampere_f  * 1000.0f + 0.5f);
    snap->power_milli   = (uint32_t)(voltage_f * ampere_f * 1000.0f + 0.5f);

    /* ---- Read real sensors for flow rate and RPM ---- */
    float flow_f = 0.0f;
    float rpm_f  = 0.0f;
    pulse_controller_read(&flow_f, &rpm_f);
    if (flow_f < 0.0f) flow_f = 0.0f;
    if (rpm_f  < 0.0f) rpm_f  = 0.0f;

    snap->flow_milli = (uint32_t)(flow_f * 1000.0f + 0.5f);
    snap->rpm         = (uint32_t)(rpm_f + 0.5f);

    /* ---- Read RTC time ---- */
    ds3231_time_t now = {0};
    if (ds3231_get_time(&now) == ESP_OK) {
        snap->time_h      = now.hours;
        snap->time_m      = now.minutes;
        snap->time_s      = now.seconds;
        snap->date_day    = now.date;
        snap->date_month  = now.month;
        snap->date_year   = now.year;
        snap->time_valid  = true;
    } else {
        snap->time_valid = false;
    }
}

/* Push pre-read sensor data to LVGL widgets (called WITH LVGL lock held). */
static void update_ui(const sensor_snapshot_t *snap)
{
    uint32_t voltage_milli = snap->voltage_milli;
    uint32_t ampere_milli  = snap->ampere_milli;
    uint32_t power_milli   = snap->power_milli;
    uint32_t flow_milli    = snap->flow_milli;
    uint32_t rpm            = snap->rpm;

    /* -------- Gauges screen arcs + labels -------- */
    if (ui_gVoltage && ui_gVoltageValue) {
        lv_arc_set_value(ui_gVoltage, to_percent_u32(voltage_milli, 1000, 30000));
        lv_label_set_text_fmt(ui_gVoltageValue, "%lu.%03lu V",
                              voltage_milli / 1000, voltage_milli % 1000);
    }

    if (ui_gAmpere && ui_gAmpereValue) {
        lv_arc_set_value(ui_gAmpere, to_percent_u32(ampere_milli, 0, 20000));
        lv_label_set_text_fmt(ui_gAmpereValue, "%lu.%03lu A",
                              ampere_milli / 1000, ampere_milli % 1000);
    }

    if (ui_gRPM && ui_gRpmValue) {
        lv_arc_set_value(ui_gRPM, to_percent_u32(rpm, 200, 1200));
        lv_label_set_text_fmt(ui_gRpmValue, "%lu RPM", rpm);
    }

    if (ui_gPower && ui_gPowerValue) {
        lv_arc_set_value(ui_gPower, to_percent_u32(power_milli, 0, 500000));
        lv_label_set_text_fmt(ui_gPowerValue, "%lu.%03lu W",
                              power_milli / 1000, power_milli % 1000);
    }

    if (ui_gWaterFlow && ui_gWaterFlowValue) {
        lv_arc_set_value(ui_gWaterFlow, to_percent_u32(flow_milli, 0, 20000));
        lv_label_set_text_fmt(ui_gWaterFlowValue, "%lu.%03lu L/M",
                              flow_milli / 1000, flow_milli % 1000);
    }

    /* -------- Data screen labels -------- */
    if (ui_lblFlowRateValue) {
        lv_label_set_text_fmt(ui_lblFlowRateValue, "%lu.%03lu L/M",
                              flow_milli / 1000, flow_milli % 1000);
    }
    if (ui_lblVoltageValue) {
        lv_label_set_text_fmt(ui_lblVoltageValue, "%lu.%03lu V",
                              voltage_milli / 1000, voltage_milli % 1000);
    }
    if (ui_lblAmpereValue) {
        lv_label_set_text_fmt(ui_lblAmpereValue, "%lu.%03lu A",
                              ampere_milli / 1000, ampere_milli % 1000);
    }
    if (ui_lblPowerValue) {
        lv_label_set_text_fmt(ui_lblPowerValue, "%lu.%03lu W",
                              power_milli / 1000, power_milli % 1000);
    }
    if (ui_lblRpmValue) {
        lv_label_set_text_fmt(ui_lblRpmValue, "%lu RPM", rpm);
    }

    /* -------- Data screen time label (matches CSV: d-mmm-yyyy h:mm:ss) -------- */
    if (ui_lblTimeValue && snap->time_valid) {
        static const char *mon[] = {
            "Jan","Feb","Mar","Apr","May","Jun",
            "Jul","Aug","Sep","Oct","Nov","Dec"
        };
        uint8_t mi = snap->date_month;
        if (mi < 1)  mi = 1;
        if (mi > 12) mi = 12;
        lv_label_set_text_fmt(ui_lblTimeValue, "%u-%s-%04u %02u:%02u:%02u",
                              snap->date_day, mon[mi - 1], snap->date_year,
                              snap->time_h, snap->time_m, snap->time_s);
    }

    /* -------- Data screen Chart1 (Voltage, Ampere, Power) -------- */
    init_data_chart();
    if (s_chart_ready && ui_Chart1) {
        /* Rolling max: expands immediately on a new peak, decays slowly (~30 updates
         * to halve) so the range stays stable as values fall. */
        static uint32_t s_c1_prim_max = 100;
        static uint32_t s_c1_sec_max  = 10;

        uint32_t prim_val = voltage_milli > ampere_milli ? voltage_milli : ampere_milli;
        if (prim_val > s_c1_prim_max) s_c1_prim_max = prim_val;
        else if (s_c1_prim_max > 100) s_c1_prim_max -= s_c1_prim_max / 30 + 1;
        if (s_c1_prim_max < 100) s_c1_prim_max = 100;

        if (power_milli > s_c1_sec_max) s_c1_sec_max = power_milli;
        else if (s_c1_sec_max > 10) s_c1_sec_max -= s_c1_sec_max / 30 + 1;
        if (s_c1_sec_max < 10) s_c1_sec_max = 10;

        int32_t c1_prim_top = (int32_t)(s_c1_prim_max * 5 / 4); /* 25% headroom */
        int32_t c1_sec_top  = (int32_t)(s_c1_sec_max  * 5 / 4);

        lv_chart_set_range(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y,   0, c1_prim_top);
        lv_chart_set_range(ui_Chart1, LV_CHART_AXIS_SECONDARY_Y, 0, c1_sec_top);
        /* Keep scale label widgets in sync with the actual chart range */
        lv_scale_set_range(ui_Chart1_Yaxis1, 0, c1_prim_top);
        lv_scale_set_range(ui_Chart1_Yaxis2, 0, c1_sec_top);

        lv_chart_series_t *sv = lv_chart_get_series_next(ui_Chart1, NULL);
        lv_chart_series_t *sa = sv ? lv_chart_get_series_next(ui_Chart1, sv) : NULL;
        lv_chart_series_t *sp = sa ? lv_chart_get_series_next(ui_Chart1, sa) : NULL;
        if (sv && sa && sp) {
            lv_chart_set_next_value(ui_Chart1, sv, (int32_t)voltage_milli);
            lv_chart_set_next_value(ui_Chart1, sa, (int32_t)ampere_milli);
            lv_chart_set_next_value(ui_Chart1, sp, (int32_t)power_milli);
        }
    }

    /* -------- Data screen Chart2 (Flow, RPM) -------- */
    if (s_chart_ready && ui_Chart2) {
        static uint32_t s_c2_prim_max = 100;
        static uint32_t s_c2_sec_max  = 10;

        if (flow_milli > s_c2_prim_max) s_c2_prim_max = flow_milli;
        else if (s_c2_prim_max > 100) s_c2_prim_max -= s_c2_prim_max / 30 + 1;
        if (s_c2_prim_max < 100) s_c2_prim_max = 100;

        if (rpm > s_c2_sec_max) s_c2_sec_max = rpm;
        else if (s_c2_sec_max > 10) s_c2_sec_max -= s_c2_sec_max / 30 + 1;
        if (s_c2_sec_max < 10) s_c2_sec_max = 10;

        int32_t c2_prim_top = (int32_t)(s_c2_prim_max * 5 / 4);
        int32_t c2_sec_top  = (int32_t)(s_c2_sec_max  * 5 / 4);

        lv_chart_set_range(ui_Chart2, LV_CHART_AXIS_PRIMARY_Y,   0, c2_prim_top);
        lv_chart_set_range(ui_Chart2, LV_CHART_AXIS_SECONDARY_Y, 0, c2_sec_top);
        lv_scale_set_range(ui_Chart2_Yaxis1, 0, c2_prim_top);
        lv_scale_set_range(ui_Chart2_Yaxis2, 0, c2_sec_top);

        lv_chart_series_t *sf = lv_chart_get_series_next(ui_Chart2, NULL);
        lv_chart_series_t *sr = sf ? lv_chart_get_series_next(ui_Chart2, sf) : NULL;
        if (sf && sr) {
            lv_chart_set_next_value(ui_Chart2, sf, (int32_t)flow_milli);
            lv_chart_set_next_value(ui_Chart2, sr, (int32_t)rpm);
        }
    }
}

static void gauges_task(void *arg)
{
    (void)arg;

    while (1) {
        /* Read sensors OUTSIDE the LVGL lock to avoid starving the LVGL task */
        sensor_snapshot_t snap = {0};
        read_sensors(&snap);

        /* Only hold the lock for fast UI widget updates */
        if (lvgl_port_lock(200)) {
            update_ui(&snap);
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

/**
 * @brief Reset chart ready flag to force re-initialization on next Data screen load.
 * Call this when navigating away from the Data screen to ensure fresh styling
 * on the next visit.
 */
void gauges_controller_reset_chart(void)
{
    s_chart_ready = false;
}

/**
 * @brief Immediately initialize/refresh the Data page chart styling.
 * Can be called from screen load event to ensure chart is styled before display.
 */
void gauges_controller_refresh_chart(void)
{
    if (lvgl_port_lock(200)) {
        init_data_chart();
        lvgl_port_unlock();
    }
}
