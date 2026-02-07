#include "ui_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- GLOBAL OBJECTS --- */
static lv_obj_t *scr_status;
static lv_obj_t *scr_list;
static lv_obj_t *scr_chain; 
static lv_obj_t *scr_editor; 
static lv_obj_t *scr_add_select; 
static lv_obj_t *scr_name_select; // New global object for name selection screen

/* Widgets */
static lv_obj_t *status_bg; 
static lv_obj_t *status_label;
static lv_obj_t *library_list;
static lv_obj_t *chain_list;
static lv_obj_t *editor_container;
static lv_obj_t *editor_label;

#define CLICK_DRAG_THRESHOLD 5 // Pixels for detecting if a "click" was actually a drag

static const char *preset_name_options[] = {
    "Rock", "Blues", "Jazz", "Funk", "Pop", "Metal", "Ambient",
    "Clean", "Drive", "Distortion",
    NULL // Sentinel value
};

/* --- STYLES --- */
static lv_style_t style_active;
static lv_style_t style_inactive;
static lv_style_t style_knob_arc;
static lv_style_t style_list_btn;

/* --- DATA MODELS --- */
typedef enum {
    FX_DISTORTION,
    FX_OVERDRIVE,
    FX_FUZZ,
    FX_GAIN,
    FX_COMPRESSOR,
    FX_CHORUS,
    FX_FLANGER,
    FX_DELAY,
    FX_ECHO,
    FX_REVERB,
    FX_EQ_3BAND,
    FX_EQ_8BAND,
    FX_NONE
} effect_type_t;

typedef struct {
    effect_type_t type;
    char name[32];
} effect_item_t;

#define MAX_PRESETS 30
#define MAX_EFFECTS 8 

typedef struct {
    char name[32];
    effect_item_t effects[MAX_EFFECTS];
    int effect_count;
    bool active;
} preset_t;

static preset_t presets[MAX_PRESETS];
static int preset_count = 0;
static int current_preset_idx = 0;
static int previous_active_preset_idx = 0; // New variable
static int current_effect_idx = -1; 

/* --- PROTOTYPES --- */
static void create_scr_status(void);
static void create_scr_list(void);
static void create_scr_chain(void);
static void create_scr_editor(void);
static void create_scr_add_select(void);
static void build_knobs_for_effect(effect_type_t type);
static void rebuild_chain_list_ui(void);
static void init_dummy_data(void);
static void chain_gesture_cb(lv_event_t *e); // New prototype
static void create_scr_name_select(void); // New prototype
static void btn_select_name_cb(lv_event_t *e);
static void cancel_name_select_cb(lv_event_t *e);
static void btn_del_preset_cb(lv_event_t *e);

/* ================= HELPER UI FUNCTIONS ================= */

static void update_status_ui(void) {
    bool is_active = presets[current_preset_idx].active;
    if (is_active) {
        lv_obj_add_style(status_bg, &style_active, LV_PART_MAIN);
        lv_obj_remove_style(status_bg, &style_inactive, LV_PART_MAIN);
    } else {
        lv_obj_add_style(status_bg, &style_inactive, LV_PART_MAIN);
        lv_obj_remove_style(status_bg, &style_active, LV_PART_MAIN);
    }
    if (status_label) lv_label_set_text(status_label, presets[current_preset_idx].name);
}

void ui_toggle_active_state(void) {
    presets[current_preset_idx].active = !presets[current_preset_idx].active;
    update_status_ui();
}

static const char* get_list_btn_text(lv_obj_t *btn) {
    uint32_t count = lv_obj_get_child_count(btn);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *child = lv_obj_get_child(btn, i);
        if (lv_obj_check_type(child, &lv_label_class)) return lv_label_get_text(child);
    }
    return "";
}

/* ================= CALLBACKS ================= */

static void status_gesture_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
        if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
            lv_scr_load_anim(scr_list, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
        }
    }
}

static void list_gesture_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
        if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
            update_status_ui();
            lv_scr_load_anim(scr_status, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
        }
    }
}

static void list_item_clicked_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        lv_indev_t *indev = lv_indev_get_act();
        lv_point_t p_diff;
        lv_indev_get_vect(indev, &p_diff);

        // Only process short click if movement was minimal (not a drag/swipe)
        if (abs(p_diff.x) < CLICK_DRAG_THRESHOLD && abs(p_diff.y) < CLICK_DRAG_THRESHOLD) {
            current_preset_idx = idx;
            update_status_ui();
            lv_scr_load_anim(scr_status, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        previous_active_preset_idx = current_preset_idx; // Store current active preset
        current_preset_idx = idx;
        rebuild_chain_list_ui();
        lv_scr_load_anim(scr_chain, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    }
}

static void btn_add_new_preset_cb(lv_event_t *e) {
    if (preset_count >= MAX_PRESETS) return; // Prevent adding if max reached
    lv_scr_load_anim(scr_name_select, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

static void chain_back_cb(lv_event_t *e) {
    current_preset_idx = previous_active_preset_idx; // Revert to previous active preset
    update_status_ui(); // Update UI to reflect the reverted preset
    lv_scr_load_anim(scr_list, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

static void effect_item_clicked_cb(lv_event_t *e) {
    current_effect_idx = (int)(intptr_t)lv_event_get_user_data(e);
    build_knobs_for_effect(presets[current_preset_idx].effects[current_effect_idx].type);
    lv_scr_load_anim(scr_editor, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

static void btn_add_effect_open_cb(lv_event_t *e) {
    if (presets[current_preset_idx].effect_count >= MAX_EFFECTS) return;
    lv_scr_load_anim(scr_add_select, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, false);
}

static void btn_select_effect_cb(lv_event_t *e) {
    effect_type_t type = (effect_type_t)(intptr_t)lv_event_get_user_data(e);
    const char *txt = get_list_btn_text(lv_event_get_target(e));
    int idx = presets[current_preset_idx].effect_count;
    presets[current_preset_idx].effects[idx].type = type;
    snprintf(presets[current_preset_idx].effects[idx].name, 32, "%s", txt);
    presets[current_preset_idx].effect_count++;
    rebuild_chain_list_ui();
    lv_scr_load_anim(scr_chain, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false);
}

static void select_cancel_cb(lv_event_t *e) {
    lv_scr_load_anim(scr_chain, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false);
}

static void editor_back_cb(lv_event_t *e) {
    lv_scr_load_anim(scr_chain, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

static void editor_del_cb(lv_event_t *e) {
    if (current_effect_idx >= 0 && current_effect_idx < presets[current_preset_idx].effect_count) {
        for (int i = current_effect_idx; i < presets[current_preset_idx].effect_count - 1; i++) {
            presets[current_preset_idx].effects[i] = presets[current_preset_idx].effects[i+1];
        }
        presets[current_preset_idx].effect_count--;
        rebuild_chain_list_ui();
        lv_scr_load_anim(scr_chain, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
    }
}

static void knob_event_cb(lv_event_t *e) {
    lv_obj_t *arc = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t*)lv_event_get_user_data(e);
    if(label) lv_label_set_text_fmt(label, "%d", (int)lv_arc_get_value(arc));
}

/* ================= LOGIC & DUMMY DATA ================= */

static void init_dummy_data(void) {
    const char *names[] = {"Clean", "Crunch", "High Gain", "Ambient"};
    preset_count = 4;
    for(int i=0; i<preset_count; i++) {
        snprintf(presets[i].name, 32, "%s", names[i]);
        presets[i].active = false;
        presets[i].effect_count = 0;
        if(i==0) { 
            presets[i].effects[0] = (effect_item_t){FX_CHORUS, "Chorus"};
            presets[i].effects[1] = (effect_item_t){FX_REVERB, "Reverb"};
            presets[i].effect_count = 2;
        }
    }
}

static void rebuild_chain_list_ui(void) {
    lv_obj_clean(chain_list);
    for(int i=0; i < presets[current_preset_idx].effect_count; i++) {
        lv_obj_t *item = lv_list_add_btn(chain_list, LV_SYMBOL_SETTINGS, presets[current_preset_idx].effects[i].name);
        lv_obj_add_style(item, &style_list_btn, 0);
        lv_obj_add_event_cb(item, effect_item_clicked_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

/* ================= UI BUILDERS ================= */

static lv_obj_t* create_knob(lv_obj_t *parent, const char *title, int32_t val, int32_t min, int32_t max) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 80, 100);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    
    /* Arc */
    lv_obj_t *arc = lv_arc_create(cont);
    lv_obj_set_size(arc, 60, 60);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_value(arc, val);
    lv_arc_set_range(arc, min, max);
    
    /* FIX: Hide Knob (Slider Handle) to avoid artifacts */
    lv_obj_add_style(arc, &style_knob_arc, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB); 
    lv_obj_set_style_opa(arc, 0, LV_PART_KNOB); /* Double ensure knob is invisible */
    
    lv_obj_add_flag(arc, LV_OBJ_FLAG_CLICKABLE); 
    
    lv_obj_t *val_lbl = lv_label_create(arc);
    lv_label_set_text_fmt(val_lbl, "%d", (int)val); 
    lv_obj_set_style_text_color(val_lbl, lv_color_white(), 0);
    lv_obj_center(val_lbl);

    lv_obj_add_event_cb(arc, knob_event_cb, LV_EVENT_VALUE_CHANGED, val_lbl);
    return cont;
}

static void build_knobs_for_effect(effect_type_t type) {
    lv_obj_clean(editor_container);
    if (type == FX_EQ_8BAND) {
        lv_obj_set_flex_flow(editor_container, LV_FLEX_FLOW_ROW);
        /* Use SPACE_BETWEEN for better separation */
        lv_obj_set_flex_align(editor_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        /* 8 bands */
        const char *freqs[] = {"63", "125", "250", "500", "1k", "2k", "4k", "8k"};
        for(int i=0; i<8; i++) {
            lv_obj_t *sub = lv_obj_create(editor_container);
            /* Slightly wider container to prevent overlap */
            lv_obj_set_size(sub, 28, 180);
            lv_obj_set_style_bg_opa(sub, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(sub, 0, 0);
            lv_obj_set_flex_flow(sub, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_all(sub, 0, 0);
            lv_obj_set_style_pad_gap(sub, 2, 0);

            lv_obj_t *slider = lv_slider_create(sub);
            lv_obj_set_size(slider, 8, 140);
            lv_slider_set_range(slider, -12, 12);
            lv_slider_set_value(slider, 0, LV_ANIM_OFF);
            lv_obj_center(slider);

            lv_obj_t *l = lv_label_create(sub);
            lv_label_set_text(l, freqs[i]);
            /* Reduce font scale slightly if needed, but 14 should fit if rotated or abbreviated */
            lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0xAAAAAA), 0);
            lv_obj_center(l);
        }
        lv_label_set_text(editor_label, "8-Band EQ");
    } else {
        lv_obj_set_flex_flow(editor_container, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(editor_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        /* Mix knob is standard for almost all effects */
        bool has_mix = true;

        if(type == FX_DISTORTION) { create_knob(editor_container, "Gain", 80, 0, 100); create_knob(editor_container, "Tone", 50, 0, 100); create_knob(editor_container, "Level", 60, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Distortion"); }
        else if(type == FX_OVERDRIVE) { create_knob(editor_container, "Drive", 60, 0, 100); create_knob(editor_container, "Tone", 50, 0, 100); create_knob(editor_container, "Level", 70, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Overdrive"); }
        else if(type == FX_FUZZ) { create_knob(editor_container, "Fuzz", 90, 0, 100); create_knob(editor_container, "Tone", 40, 0, 100); create_knob(editor_container, "Vol", 60, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Fuzz"); }
        else if(type == FX_GAIN) { create_knob(editor_container, "Gain", 10, 0, 30); create_knob(editor_container, "Bass", 0, -12, 12); create_knob(editor_container, "Treb", 0, -12, 12); has_mix = false; lv_label_set_text(editor_label, "Boost / Gain"); }
        else if(type == FX_COMPRESSOR) { create_knob(editor_container, "Sustain", 50, 0, 100); create_knob(editor_container, "Attack", 20, 0, 100); create_knob(editor_container, "Level", 50, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Compressor"); }
        else if(type == FX_CHORUS) { create_knob(editor_container, "Rate", 30, 0, 100); create_knob(editor_container, "Depth", 50, 0, 100); create_knob(editor_container, "Tone", 50, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Chorus"); }
        else if(type == FX_FLANGER) { create_knob(editor_container, "Rate", 20, 0, 100); create_knob(editor_container, "Depth", 60, 0, 100); create_knob(editor_container, "Res", 40, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Flanger"); }
        else if(type == FX_DELAY) { create_knob(editor_container, "Time", 400, 0, 1000); create_knob(editor_container, "Fdbk", 40, 0, 100); create_knob(editor_container, "Tone", 50, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Delay"); }
        else if(type == FX_ECHO) { create_knob(editor_container, "Time", 200, 0, 500); create_knob(editor_container, "Fdbk", 30, 0, 100); create_knob(editor_container, "Wow", 10, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Tape Echo"); }
        else if(type == FX_REVERB) { create_knob(editor_container, "Decay", 70, 0, 100); create_knob(editor_container, "Damp", 40, 0, 100); create_knob(editor_container, "PreDly", 20, 0, 200); has_mix = true; lv_label_set_text(editor_label, "Reverb"); }
        else if(type == FX_EQ_3BAND) { create_knob(editor_container, "Bass", 0, -12, 12); create_knob(editor_container, "Mid", 0, -12, 12); create_knob(editor_container, "Treb", 0, -12, 12); create_knob(editor_container, "Vol", 0, -12, 12); has_mix = false; lv_label_set_text(editor_label, "3-Band EQ"); }
        else { create_knob(editor_container, "Param 1", 50, 0, 100); has_mix = true; lv_label_set_text(editor_label, "Effect"); }

        if(has_mix) create_knob(editor_container, "Mix", 50, 0, 100);
    }
}

static void create_scr_status(void) {
    scr_status = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_status, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_add_event_cb(scr_status, status_gesture_cb, LV_EVENT_GESTURE, NULL);
    status_bg = lv_obj_create(scr_status);
    lv_obj_set_size(status_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(status_bg, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(status_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_bg, 12, 0);
    lv_obj_remove_flag(status_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_style_init(&style_active);
    lv_style_set_border_color(&style_active, lv_color_hex(0x00FF00)); /* Green */
    lv_style_init(&style_inactive);
    lv_style_set_border_color(&style_inactive, lv_color_hex(0xFF0000)); /* Red */
    status_label = lv_label_create(scr_status);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0); 
    lv_obj_set_style_transform_scale(status_label, 256, 0); 
    lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
    lv_obj_center(status_label);
}

static void create_scr_list(void) {
    scr_list = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_list, lv_color_hex(0x000000), 0);
    lv_obj_add_event_cb(scr_list, list_gesture_cb, LV_EVENT_GESTURE, NULL);
    
    lv_style_init(&style_list_btn);
    lv_style_set_height(&style_list_btn, 60); /* Large buttons */
    lv_style_set_pad_all(&style_list_btn, 10);
    lv_style_set_bg_color(&style_list_btn, lv_color_hex(0x202020));
    lv_style_set_text_color(&style_list_btn, lv_color_white());

    library_list = lv_list_create(scr_list);
    lv_obj_set_size(library_list, 240, 260); 
    lv_obj_align(library_list, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(library_list, lv_color_hex(0x101010), 0);
    lv_obj_set_style_border_width(library_list, 0, 0);
    for(int i=0; i<preset_count; i++) {
        lv_obj_t *btn = lv_list_add_btn(library_list, LV_SYMBOL_AUDIO, presets[i].name);
        lv_obj_add_style(btn, &style_list_btn, 0);
        lv_obj_add_event_cb(btn, list_item_clicked_cb, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, list_item_clicked_cb, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
    }
    lv_obj_t *btn_add = lv_button_create(scr_list);
    lv_obj_set_size(btn_add, 220, 45);
    lv_obj_align(btn_add, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(btn_add, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn_add, btn_add_new_preset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(btn_add); lv_label_set_text(l, "+ ADD NEW"); lv_obj_center(l);
}

static void create_scr_chain(void) {
    scr_chain = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_chain, lv_color_hex(0x050505), 0);
    lv_obj_add_event_cb(scr_chain, chain_gesture_cb, LV_EVENT_GESTURE, NULL);
    chain_list = lv_list_create(scr_chain);
    lv_obj_set_size(chain_list, 240, 230);
    lv_obj_align(chain_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(chain_list, lv_color_hex(0x101010), 0);
    lv_obj_t *btn_add = lv_button_create(scr_chain);
    lv_obj_set_size(btn_add, 220, 45);
    lv_obj_align(btn_add, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(btn_add, btn_add_effect_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(btn_add); lv_label_set_text(l, "+ ADD EFFECT"); lv_obj_center(l);

    // NEW: Delete Preset Button
    lv_obj_t *btn_del_preset = lv_button_create(scr_chain);
    lv_obj_set_size(btn_del_preset, 45, 35); // Same size as back button
    lv_obj_align(btn_del_preset, LV_ALIGN_TOP_RIGHT, -5, 5); // Align to top right
    lv_obj_set_style_bg_color(btn_del_preset, lv_color_hex(0xFF0000), LV_PART_MAIN); // Red
    lv_obj_add_event_cb(btn_del_preset, btn_del_preset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_del_preset = lv_label_create(btn_del_preset); lv_label_set_text(lbl_del_preset, LV_SYMBOL_TRASH); lv_obj_center(lbl_del_preset);

    lv_obj_t *btn_back = lv_button_create(scr_chain);
    lv_obj_set_size(btn_back, 45, 35);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_add_event_cb(btn_back, chain_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb = lv_label_create(btn_back); lv_label_set_text(lb, LV_SYMBOL_LEFT); lv_obj_center(lb);
}

// Implement btn_del_preset_cb
static void btn_del_preset_cb(lv_event_t *e) {
    if (preset_count == 0) return; // No presets to delete

    // Shift elements to remove the current preset
    for (int i = current_preset_idx; i < preset_count - 1; i++) {
        presets[i] = presets[i + 1];
    }
    preset_count--;

    // Adjust current_preset_idx if needed
    if (current_preset_idx >= preset_count && preset_count > 0) {
        current_preset_idx = preset_count - 1; // Move to the new last preset
    } else if (preset_count == 0) {
        current_preset_idx = 0; // No presets left
    }

    // Update previous_active_preset_idx if it was affected
    if (previous_active_preset_idx >= preset_count && preset_count > 0) {
        previous_active_preset_idx = preset_count - 1;
    } else if (preset_count == 0) {
        previous_active_preset_idx = 0;
    }
    // If the deleted preset was the previous_active_preset_idx, it's effectively gone.
    // The previous_active_preset_idx should now refer to the new current_preset_idx,
    // or adjusted as above.

    // Rebuild library_list on scr_list
    lv_obj_clean(library_list); // Clear all buttons
    for(int i=0; i<preset_count; i++) {
        lv_obj_t *btn = lv_list_add_btn(library_list, LV_SYMBOL_AUDIO, presets[i].name);
        lv_obj_add_style(btn, &style_list_btn, 0);
        lv_obj_add_event_cb(btn, list_item_clicked_cb, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, list_item_clicked_cb, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
    }
    update_status_ui(); // Update status for the new current preset

    // Load scr_list with animation
    lv_scr_load_anim(scr_list, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

static void chain_gesture_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
        if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
            current_preset_idx = previous_active_preset_idx; // Revert to previous active preset
            update_status_ui(); // Update UI to reflect the reverted preset
            lv_scr_load_anim(scr_list, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
        }
    }
}

static void create_scr_editor(void) {
    scr_editor = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_editor, lv_color_hex(0x151515), 0);
    lv_style_init(&style_knob_arc);
    lv_style_set_arc_color(&style_knob_arc, lv_color_hex(0x00A8FF));
    lv_style_set_arc_width(&style_knob_arc, 8);
    editor_label = lv_label_create(scr_editor);
    lv_obj_set_style_text_color(editor_label, lv_color_white(), 0);
    lv_obj_align(editor_label, LV_ALIGN_TOP_MID, 0, 10);
    editor_container = lv_obj_create(scr_editor);
    lv_obj_set_size(editor_container, 240, 240);
    lv_obj_align(editor_container, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_opa(editor_container, 0, 0); lv_obj_set_style_border_width(editor_container, 0, 0);
    lv_obj_t *btn_back = lv_button_create(scr_editor);
    lv_obj_set_size(btn_back, 85, 35);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 10, -5);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x00A8FF), LV_PART_MAIN); // Set "Done" to Blue
    lv_obj_add_event_cb(btn_back, editor_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back); lv_label_set_text(lbl_back, "Done"); lv_obj_center(lbl_back);
    lv_obj_t *btn_del = lv_button_create(scr_editor);
    lv_obj_set_size(btn_del, 85, 35);
    lv_obj_align(btn_del, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xFF0000), LV_PART_MAIN); // Ensure "Del" is Red
    lv_obj_add_event_cb(btn_del, editor_del_cb, LV_EVENT_CLICKED, NULL); 
    lv_obj_t *lbl_del = lv_label_create(btn_del); lv_label_set_text(lbl_del, "Del"); lv_obj_center(lbl_del);
}

static void create_scr_add_select(void) {
    scr_add_select = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_add_select, lv_color_hex(0x000000), 0);
    lv_obj_t *list = lv_list_create(scr_add_select);
    lv_obj_set_size(list, 240, 230);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x101010), 0); // Set background color

    
    struct { char *name; effect_type_t type; } effects[] = {
        {"Distortion", FX_DISTORTION}, {"Overdrive", FX_OVERDRIVE}, {"Fuzz", FX_FUZZ},
        {"Boost / Gain", FX_GAIN}, {"Compressor", FX_COMPRESSOR},
        {"Chorus", FX_CHORUS}, {"Flanger", FX_FLANGER}, {"Delay", FX_DELAY}, {"Tape Echo", FX_ECHO},
        {"Reverb", FX_REVERB}, {"3-Band EQ", FX_EQ_3BAND}, {"8-Band EQ", FX_EQ_8BAND}, 
        {NULL, 0}
    };
    for(int i=0; effects[i].name; i++) {
        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_PLUS, effects[i].name);
        lv_obj_add_style(btn, &style_list_btn, 0);
        lv_obj_add_event_cb(btn, btn_select_effect_cb, LV_EVENT_CLICKED, (void*)(intptr_t)effects[i].type);
    }
    lv_obj_t *btn_cancel = lv_button_create(scr_add_select);
    lv_obj_set_size(btn_cancel, 220, 45);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(btn_cancel, select_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(btn_cancel); lv_label_set_text(l, "Cancel"); lv_obj_center(l);
}

void ui_app_init(void) {
    init_dummy_data();
    create_scr_status();
    create_scr_list();
    create_scr_chain();
    create_scr_editor();
    create_scr_add_select();
    create_scr_name_select(); // New screen initialization
    update_status_ui();
    lv_scr_load(scr_status);
}

void ui_app_next_preset(void) {
    if (preset_count == 0) return;
    current_preset_idx++;
    if (current_preset_idx >= preset_count) {
        current_preset_idx = 0; // Wrap around to the first preset
    }
    update_status_ui();
}

void ui_app_prev_preset(void) {
    if (preset_count == 0) return;
    current_preset_idx--;
    if (current_preset_idx < 0) {
        current_preset_idx = preset_count - 1; // Wrap around to the last preset
    }
    update_status_ui();
}

// New callback function
static void btn_select_name_cb(lv_event_t *e) {
    if (preset_count >= MAX_PRESETS) { // Max preset check
        lv_scr_load_anim(scr_list, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false); // Go back if full
        return;
    }

    const char *selected_name = lv_event_get_user_data(e);
    char final_preset_name[32];
    int suffix = 0;
    bool name_exists;

    do {
        name_exists = false;
        if (suffix == 0) {
            snprintf(final_preset_name, 32, "%s", selected_name);
        } else {
            snprintf(final_preset_name, 32, "%s %d", selected_name, suffix);
        }

        // Check if this name already exists
        for (int i = 0; i < preset_count; i++) {
            if (strcmp(presets[i].name, final_preset_name) == 0) {
                name_exists = true;
                suffix++;
                break;
            }
        }
    } while (name_exists);

    snprintf(presets[preset_count].name, 32, "%s", final_preset_name);
    presets[preset_count].effect_count = 0;
    presets[preset_count].active = false;

    lv_obj_t *btn = lv_list_add_btn(library_list, LV_SYMBOL_AUDIO, presets[preset_count].name);
    lv_obj_add_style(btn, &style_list_btn, 0);
    lv_obj_add_event_cb(btn, list_item_clicked_cb, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)preset_count);
    lv_obj_add_event_cb(btn, list_item_clicked_cb, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)preset_count);
    preset_count++;
    lv_obj_scroll_to_view(btn, LV_ANIM_ON);

    lv_scr_load_anim(scr_list, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false); // Go back to preset list
}

// New callback function
static void cancel_name_select_cb(lv_event_t *e) {
    lv_scr_load_anim(scr_list, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false);
}

// New screen creation function
static void create_scr_name_select(void) {
    scr_name_select = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_name_select, lv_color_hex(0x000000), 0);

    lv_obj_t *title = lv_label_create(scr_name_select);
    lv_label_set_text(title, "Select Preset Name");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *list = lv_list_create(scr_name_select);
    lv_obj_set_size(list, 240, 230);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x101010), 0);

    for(int i = 0; preset_name_options[i] != NULL; i++) {
        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, preset_name_options[i]);
        lv_obj_add_style(btn, &style_list_btn, 0);
        lv_obj_add_event_cb(btn, btn_select_name_cb, LV_EVENT_CLICKED, (void*)preset_name_options[i]);
    }

    lv_obj_t *btn_cancel = lv_button_create(scr_name_select);
    lv_obj_set_size(btn_cancel, 220, 45);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(btn_cancel, cancel_name_select_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(btn_cancel); lv_label_set_text(l, "Cancel"); lv_obj_center(l);
}
