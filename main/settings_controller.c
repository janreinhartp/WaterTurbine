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
/*  Page definitions — 2-point reference calibration                   */
/* ------------------------------------------------------------------ */
/*  8 pages: for each channel (V / A) we edit four values:             */
/*    RAW_REF_LOW, REF_LOW, RAW_REF_HIGH, REF_HIGH                    */
/*  These map directly to sensor_cal_t:                                */
/*    raw1 = RAW_REF_LOW,   actual1 = REF_LOW                         */
/*    raw2 = RAW_REF_HIGH,  actual2 = REF_HIGH                        */

typedef enum {
    PAGE_V_RAW_REF_LOW = 0,
    PAGE_V_REF_LOW,
    PAGE_V_RAW_REF_HIGH,
    PAGE_V_REF_HIGH,
    PAGE_A_RAW_REF_LOW,
    PAGE_A_REF_LOW,
    PAGE_A_RAW_REF_HIGH,
    PAGE_A_REF_HIGH,
    PAGE_COUNT
} settings_page_id_t;

/* Whether the page relates to voltage (true) or ampere (false) */
static const bool s_page_is_voltage[PAGE_COUNT] = {
    true, true, true, true,
    false, false, false, false,
};

static const char *s_page_labels[PAGE_COUNT] = {
    "V RAW REF LOW",
    "V REF LOW (actual V)",
    "V RAW REF HIGH",
    "V REF HIGH (actual V)",
    "A RAW REF LOW",
    "A REF LOW (actual A)",
    "A RAW REF HIGH",
    "A REF HIGH (actual A)",
};

static float  s_page_values[PAGE_COUNT];
static int    s_current_page = 0;

/* Dynamically created label for live raw readings */
static lv_obj_t *s_lbl_raw_reading = NULL;
static lv_timer_t *s_raw_timer = NULL;

/* ------------------------------------------------------------------ */
/*  Load current sensor calibration into page values                   */
/* ------------------------------------------------------------------ */
static void load_current_cal(void)
{
    sensor_cal_t vcal, acal;
    sensor_get_voltage_cal(&vcal);
    sensor_get_ampere_cal(&acal);

    s_page_values[PAGE_V_RAW_REF_LOW]  = vcal.raw1;
    s_page_values[PAGE_V_REF_LOW]      = vcal.actual1;
    s_page_values[PAGE_V_RAW_REF_HIGH] = vcal.raw2;
    s_page_values[PAGE_V_REF_HIGH]     = vcal.actual2;

    s_page_values[PAGE_A_RAW_REF_LOW]  = acal.raw1;
    s_page_values[PAGE_A_REF_LOW]      = acal.actual1;
    s_page_values[PAGE_A_RAW_REF_HIGH] = acal.raw2;
    s_page_values[PAGE_A_REF_HIGH]     = acal.actual2;
}

/* ------------------------------------------------------------------ */
/*  Update the live raw reading label                                  */
/* ------------------------------------------------------------------ */
static void update_raw_reading_label(void)
{
    if (s_lbl_raw_reading == NULL) return;

    float raw_v = 0.0f, raw_a = 0.0f;
    sensor_read_raw_voltage(&raw_v);
    sensor_read_raw_ampere(&raw_a);

    char buf[64];
    if (s_page_is_voltage[s_current_page]) {
        snprintf(buf, sizeof(buf), "Raw V: %.4f", raw_v);
    } else {
        snprintf(buf, sizeof(buf), "Raw A: %.4f", raw_a);
    }
    lv_label_set_text(s_lbl_raw_reading, buf);
}

static void raw_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_raw_reading_label();
}

/* ------------------------------------------------------------------ */
/*  UI helpers                                                         */
/* ------------------------------------------------------------------ */
static void show_page(int page)
{
    if (page < 0 || page >= PAGE_COUNT) return;
    s_current_page = page;

    /* Update setting name label */
    char header[64];
    snprintf(header, sizeof(header), "%s  (%d/%d)",
             s_page_labels[page], page + 1, PAGE_COUNT);
    lv_label_set_text(ui_lblCurrentSettings, header);

    /* Pre-fill current value */
    char buf[16];
    snprintf(buf, sizeof(buf), "%.4f", s_page_values[page]);
    lv_textarea_set_text(ui_txtbSettingsCurrentValue, buf);

    /* Immediately refresh raw reading for the new page's channel */
    update_raw_reading_label();
}

/* Read the text-area value and store in current page slot */
static void store_current_page_value(void)
{
    const char *txt = lv_textarea_get_text(ui_txtbSettingsCurrentValue);
    if (txt && txt[0] != '\0') {
        s_page_values[s_current_page] = strtof(txt, NULL);
    }
}

/* Apply 2-point reference values to the sensor calibration module */
static void apply_calibrations(void)
{
    sensor_cal_t vcal = {
        .raw1    = s_page_values[PAGE_V_RAW_REF_LOW],
        .actual1 = s_page_values[PAGE_V_REF_LOW],
        .raw2    = s_page_values[PAGE_V_RAW_REF_HIGH],
        .actual2 = s_page_values[PAGE_V_REF_HIGH],
    };
    sensor_cal_t acal = {
        .raw1    = s_page_values[PAGE_A_RAW_REF_LOW],
        .actual1 = s_page_values[PAGE_A_REF_LOW],
        .raw2    = s_page_values[PAGE_A_RAW_REF_HIGH],
        .actual2 = s_page_values[PAGE_A_REF_HIGH],
    };

    sensor_set_voltage_cal(&vcal);
    sensor_set_ampere_cal(&acal);
    sensor_save_cal_nvs();

    ESP_LOGI(SETTINGS_TAG, "Calibration applied and saved to NVS:");
    ESP_LOGI(SETTINGS_TAG, "  V: raw(%.4f..%.4f) -> actual(%.4f..%.4f)",
             vcal.raw1, vcal.raw2, vcal.actual1, vcal.actual2);
    ESP_LOGI(SETTINGS_TAG, "  A: raw(%.4f..%.4f) -> actual(%.4f..%.4f)",
             acal.raw1, acal.raw2, acal.actual1, acal.actual2);
}

/* ------------------------------------------------------------------ */
/*  Event callbacks                                                    */
/* ------------------------------------------------------------------ */
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
        /* Stop raw reading timer before leaving */
        if (s_raw_timer) {
            lv_timer_delete(s_raw_timer);
            s_raw_timer = NULL;
        }
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

    /* Apply custom number keyboard map with BACK / NEXT keys. */
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

    /* ---- Create live raw reading label ---- */
    s_lbl_raw_reading = lv_label_create(ui_Settings);
    lv_obj_set_width(s_lbl_raw_reading, LV_SIZE_CONTENT);
    lv_obj_set_height(s_lbl_raw_reading, LV_SIZE_CONTENT);
    lv_obj_set_x(s_lbl_raw_reading, 0);
    lv_obj_set_y(s_lbl_raw_reading, -220);
    lv_obj_set_align(s_lbl_raw_reading, LV_ALIGN_CENTER);
    lv_label_set_text(s_lbl_raw_reading, "Raw: --");
    lv_obj_set_style_text_font(s_lbl_raw_reading, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_lbl_raw_reading, lv_color_hex(0x2ECC71), LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Container holding label + textarea */
    lv_obj_set_width(ui_SettingsContainer, 500);
    lv_obj_set_height(ui_SettingsContainer, 100);
    lv_obj_set_x(ui_SettingsContainer, 0);
    lv_obj_set_y(ui_SettingsContainer, -150);

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
    lv_obj_set_height(ui_Keyboard2, 280);
    lv_obj_set_x(ui_Keyboard2, 0);
    lv_obj_set_y(ui_Keyboard2, 150);
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

    /* Start periodic timer to update raw reading (every 500 ms) */
    s_raw_timer = lv_timer_create(raw_timer_cb, 500, NULL);

    /* Show first page */
    show_page(0);
}
