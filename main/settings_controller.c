#include "settings_controller.h"
#include "sensor.h"
#include "ui.h"
#include "ui_helpers.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SETTINGS_TAG "SETTINGS"

/* ------------------------------------------------------------------ */
/*  Custom number keyboard with BACK / NEXT in the bottom row          */
/* ------------------------------------------------------------------ */
#define KB_BTN_BACK "BACK"
#define KB_BTN_NEXT "NEXT"
#define KB_BTN_SAVE "SAVE"

static const char * const s_kb_map_cal[] = {
    "1",  "2",  "3",  LV_SYMBOL_BACKSPACE, "\n",
    "4",  "5",  "6",  "0",                 "\n",
    "7",  "8",  "9",  ".",                 "\n",
    KB_BTN_BACK, KB_BTN_SAVE, KB_BTN_NEXT,   ""
};

static const lv_buttonmatrix_ctrl_t s_kb_ctrl_cal[] = {
    2, 2, 2, 1,
    2, 2, 2, 2,
    2, 2, 2, 2,
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2, LV_KEYBOARD_CTRL_BUTTON_FLAGS | 3, LV_KEYBOARD_CTRL_BUTTON_FLAGS | 2
};

/* ------------------------------------------------------------------ */
/*  Page definitions                                                   */
/* ------------------------------------------------------------------ */
/*  Each page edits one float value.  offset + gain per channel.       */
/*  actual = (raw - offset) * gain                                     */

typedef enum {
    PAGE_VOLT_OFFSET = 0,
    PAGE_VOLT_GAIN,
    PAGE_AMP_OFFSET,
    PAGE_AMP_GAIN,
    PAGE_COUNT
} settings_page_id_t;

static const char *s_page_labels[PAGE_COUNT] = {
    "VOLTAGE OFFSET (V)",
    "VOLTAGE GAIN",
    "AMPERE OFFSET (A)",
    "AMPERE GAIN",
};

static float  s_page_values[PAGE_COUNT];
static int    s_current_page = 0;

/* ------------------------------------------------------------------ */
/*  Helpers: convert between offset/gain and sensor_cal_t              */
/* ------------------------------------------------------------------ */
/* sensor_cal_t → offset,gain:
 *   gain   = (actual2 − actual1) / (raw2 − raw1)
 *   offset = raw1 − actual1 / gain          (when actual1 == 0, offset == raw1)
 */
static void cal_to_offset_gain(const sensor_cal_t *cal, float *offset, float *gain)
{
    float denom = cal->raw2 - cal->raw1;
    if (fabsf(denom) < 1e-9f) {
        *gain   = 1.0f;
        *offset = cal->raw1;
        return;
    }
    *gain   = (cal->actual2 - cal->actual1) / denom;
    *offset = cal->raw1 - cal->actual1 / (*gain);
}

/* offset,gain → sensor_cal_t:
 *   raw1 = offset,  actual1 = 0
 *   raw2 = offset + 1,  actual2 = gain
 * Verification: slope = gain/1 = gain, intercept = 0 - gain*offset = −gain*offset
 *               actual = gain*(raw − offset) ✓
 */
static void offset_gain_to_cal(float offset, float gain, sensor_cal_t *cal)
{
    cal->raw1    = offset;
    cal->actual1 = 0.0f;
    cal->raw2    = offset + 1.0f;
    cal->actual2 = gain;
}

/* ------------------------------------------------------------------ */
/*  Load current sensor calibration into page values                   */
/* ------------------------------------------------------------------ */
static void load_current_cal(void)
{
    sensor_cal_t vcal, acal;
    sensor_get_voltage_cal(&vcal);
    sensor_get_ampere_cal(&acal);

    cal_to_offset_gain(&vcal, &s_page_values[PAGE_VOLT_OFFSET],
                              &s_page_values[PAGE_VOLT_GAIN]);
    cal_to_offset_gain(&acal, &s_page_values[PAGE_AMP_OFFSET],
                              &s_page_values[PAGE_AMP_GAIN]);
}

/* ------------------------------------------------------------------ */
/*  UI helpers                                                         */
/* ------------------------------------------------------------------ */
static void show_page(int page)
{
    if (page < 0 || page >= PAGE_COUNT) return;
    s_current_page = page;

    /* Update setting name label */
    char header[48];
    snprintf(header, sizeof(header), "%s  (%d/%d)",
             s_page_labels[page], page + 1, PAGE_COUNT);
    lv_label_set_text(ui_lblCurrentSettings, header);

    /* Pre-fill current value */
    char buf[16];
    snprintf(buf, sizeof(buf), "%.3f", s_page_values[page]);
    lv_textarea_set_text(ui_txtbSettingsCurrentValue, buf);
}

/* Read the text-area value and store in current page slot */
static void store_current_page_value(void)
{
    const char *txt = lv_textarea_get_text(ui_txtbSettingsCurrentValue);
    if (txt && txt[0] != '\0') {
        s_page_values[s_current_page] = strtof(txt, NULL);
    }
}

/* Apply offset/gain values to the sensor calibration module */
static void apply_calibrations(void)
{
    sensor_cal_t vcal, acal;
    offset_gain_to_cal(s_page_values[PAGE_VOLT_OFFSET],
                       s_page_values[PAGE_VOLT_GAIN], &vcal);
    offset_gain_to_cal(s_page_values[PAGE_AMP_OFFSET],
                       s_page_values[PAGE_AMP_GAIN], &acal);

    sensor_set_voltage_cal(&vcal);
    sensor_set_ampere_cal(&acal);

    ESP_LOGI(SETTINGS_TAG, "Calibration applied: V(off=%.3f gain=%.3f) A(off=%.3f gain=%.3f)",
             s_page_values[PAGE_VOLT_OFFSET], s_page_values[PAGE_VOLT_GAIN],
             s_page_values[PAGE_AMP_OFFSET],  s_page_values[PAGE_AMP_GAIN]);
}

/* ------------------------------------------------------------------ */
/*  Event callbacks                                                    */
/* ------------------------------------------------------------------ */

/* Intercept BACK / NEXT from the custom keyboard map.
 * We replace the default keyboard VALUE_CHANGED handler so that
 * "BACK"/"NEXT" are not typed into the text area.  All standard keys
 * are forwarded to lv_keyboard_def_event_cb(). */
static void on_keyboard_value(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_current_target(e);
    uint32_t btn_id = lv_buttonmatrix_get_selected_button(kb);
    if (btn_id == LV_BUTTONMATRIX_BUTTON_NONE) return;

    const char *txt = lv_buttonmatrix_get_button_text(kb, btn_id);
    if (txt == NULL) return;

    if (strcmp(txt, KB_BTN_BACK) == 0) {
        store_current_page_value();
        if (s_current_page > 0) {
            show_page(s_current_page - 1);
        }
        return;
    }
    if (strcmp(txt, KB_BTN_NEXT) == 0) {
        store_current_page_value();
        if (s_current_page < PAGE_COUNT - 1) {
            show_page(s_current_page + 1);
        }
        return;
    }
    if (strcmp(txt, KB_BTN_SAVE) == 0) {
        store_current_page_value();
        apply_calibrations();
        /* Trigger the existing Save button to navigate to MainMenu */
        lv_obj_send_event(ui_btnSettingsSave, LV_EVENT_CLICKED, NULL);
        return;
    }

    /* Forward all other keys to the default LVGL keyboard handler */
    lv_keyboard_def_event_cb(e);
}

/* ------------------------------------------------------------------ */
/*  Public: called from ui_Settings_screen_init()                      */
/* ------------------------------------------------------------------ */
void settings_controller_on_screen_init(void)
{
    /* Read current cal into page values */
    load_current_cal();
    s_current_page = 0;

    /* Keep BACK/NEXT buttons hidden – navigation is now on the keyboard */
    /* (They were created by SquareLine with HIDDEN flag; leave them.) */

    /* Apply custom number keyboard map with BACK / NEXT keys.
     * Remove the default VALUE_CHANGED handler so our replacement
     * can intercept BACK/NEXT without them being typed as text. */
    lv_keyboard_set_map(ui_Keyboard2, LV_KEYBOARD_MODE_NUMBER,
                        s_kb_map_cal, s_kb_ctrl_cal);
    lv_keyboard_set_mode(ui_Keyboard2, LV_KEYBOARD_MODE_NUMBER);

    lv_obj_remove_event_cb(ui_Keyboard2, lv_keyboard_def_event_cb);
    lv_obj_add_event_cb(ui_Keyboard2, on_keyboard_value,
                        LV_EVENT_VALUE_CHANGED, NULL);

    /* Hide the standalone SAVE button — it's now on the keyboard */
    lv_obj_add_flag(ui_btnSettingsSave, LV_OBJ_FLAG_HIDDEN);

    /* ---- Reposition label + container + textbox above keyboard ---- */
    /* Title */
    lv_obj_set_y(ui_Label9, -260);

    /* Container holding label + textarea */
    lv_obj_set_width(ui_SettingsContainer, 500);
    lv_obj_set_height(ui_SettingsContainer, 100);
    lv_obj_set_x(ui_SettingsContainer, 0);
    lv_obj_set_y(ui_SettingsContainer, -170);

    /* Textarea: wider, larger font, dark theme */
    lv_obj_set_width(ui_txtbSettingsCurrentValue, 400);
    lv_obj_set_style_text_font(ui_txtbSettingsCurrentValue, &lv_font_montserrat_30,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_txtbSettingsCurrentValue, &lv_font_montserrat_30,
                               LV_PART_TEXTAREA_PLACEHOLDER | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_txtbSettingsCurrentValue, lv_color_hex(0x1A1A2E),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_txtbSettingsCurrentValue, 255,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_txtbSettingsCurrentValue, lv_color_hex(0xFFFFFF),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_txtbSettingsCurrentValue, lv_color_hex(0x2D2D44),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_txtbSettingsCurrentValue, 1,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_txtbSettingsCurrentValue, 8,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    /* Cursor color */
    lv_obj_set_style_bg_color(ui_txtbSettingsCurrentValue, lv_color_hex(0x4D7DD3),
                              LV_PART_CURSOR | LV_STATE_DEFAULT);

    /* ---- Center the keyboard below the textbox ---- */
    lv_obj_set_width(ui_Keyboard2, 700);
    lv_obj_set_height(ui_Keyboard2, 290);
    lv_obj_set_x(ui_Keyboard2, 0);
    lv_obj_set_y(ui_Keyboard2, 140);
    lv_obj_set_align(ui_Keyboard2, LV_ALIGN_CENTER);

    /* General keyboard styling */
    lv_obj_set_style_bg_color(ui_Keyboard2, lv_color_hex(0x1A1A2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Keyboard2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_Keyboard2, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(ui_Keyboard2, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Number key buttons */
    lv_obj_set_style_bg_color(ui_Keyboard2, lv_color_hex(0x2D2D44), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Keyboard2, 255, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Keyboard2, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Keyboard2, &lv_font_montserrat_30, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_Keyboard2, 8, LV_PART_ITEMS | LV_STATE_DEFAULT);

    /* Pressed state */
    lv_obj_set_style_bg_color(ui_Keyboard2, lv_color_hex(0x4D7DD3), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_Keyboard2, 255, LV_PART_ITEMS | LV_STATE_PRESSED);

    /* Control buttons (BACK/SAVE/NEXT) — uses CHECKED state from CTRL_BUTTON_FLAGS */
    lv_obj_set_style_bg_color(ui_Keyboard2, lv_color_hex(0xD34D4D), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(ui_Keyboard2, 255, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui_Keyboard2, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(ui_Keyboard2, &lv_font_montserrat_22, LV_PART_ITEMS | LV_STATE_CHECKED);

    /* Show first page */
    show_page(0);
}
