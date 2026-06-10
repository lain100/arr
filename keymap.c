/*
Copyright 2022 @Yowkees
Copyright 2022 MURAOKA Taro (aka KoRoN, @kaoriya)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H

#include "quantum.h"

#define HOMEROW_MASK ((1U << 1) | (1U << 5))
#define IS_HOMEROW(r) (HOMEROW_MASK & (1U << ((r)->event.key.row)))

#define IS_HOMEROW_CAG(k, r) (          \
    ((k) & (QK_LCTL|QK_LALT|QK_LGUI))   \
    && IS_QK_MOD_TAP((k))               \
    && IS_HOMEROW((r))                  \
)

#define IS_QUICK_SUCCESSION_INPUT(k1, r, k2) (          \
    IS_HOMEROW_CAG((k1), (r))                           \
    && QK_MOD_TAP_GET_TAP_KEYCODE((k2)) <= KC_Z         \
    && last_matrix_activity_elapsed() <= QUICK_TAP_TERM \
)

typedef struct {
    uint8_t index;
    uint8_t bitmask;
} tap_bit_t;

#define TAP_BIT_FROM_KEYCODE(k)                                 \
    ((tap_bit_t){                                               \
        .index   = QK_MOD_TAP_GET_TAP_KEYCODE((k)) / 8,         \
        .bitmask = (1U << QK_MOD_TAP_GET_TAP_KEYCODE((k)) % 8)  \
     })

static uint8_t pressed_keys[32];

static uint16_t    inter_keycode;
static keyrecord_t inter_record;

bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        if (IS_QUICK_SUCCESSION_INPUT(keycode, record, inter_keycode)) {
            tap_bit_t tap = TAP_BIT_FROM_KEYCODE(keycode);
            pressed_keys[tap.index] |= tap.bitmask;
            record->keycode = QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
        }

        inter_keycode = keycode;
        inter_record  = *record;
    } else {
        tap_bit_t tap = TAP_BIT_FROM_KEYCODE(keycode);

        if (pressed_keys[tap.index] & tap.bitmask) {
            pressed_keys[tap.index] &= ~tap.bitmask;
            record->tap.count++;
        }
    }
    return true;
}

typedef struct {
    uint16_t keycode;
    bool interrupted;
    bool shifted;
} mt_t;

typedef struct {
    uint16_t keycode;
    uint8_t indexes[2];
    uint8_t size;
} rt_t;

mt_t mts[] = {
    { LGUI_T(KC_N) },
    { LALT_T(KC_R) },
    { LSFT_T(KC_T) },
    { LCTL_T(KC_S) },
    { RCTL_T(KC_H) },
    { RSFT_T(KC_A) },
    { RALT_T(KC_E) },
    { RGUI_T(KC_I) },
    { RCTL_T(KC_8) },
    { RSFT_T(KC_7) },
    { RALT_T(KC_9) },
    { RGUI_T(KC_3) },
};

bool is_mod_pending = false;

void mts_mods_on(void) {
    if (!is_mod_pending) {
        return;
    }
    uint8_t pending_mods = 0;

    for (uint8_t i = 0; i < ARRAY_SIZE(mts); i++) {
        mt_t *mt = &mts[i];

        if (mt->interrupted || mt->shifted) {
            uint8_t mod = (mt->keycode >> 8) & 0x1F;

            pending_mods |= (mod & 0x10) ? (mod << 4) : mod;
            mt->interrupted = false;
            mt->shifted = false;
        }
    }
    register_mods(pending_mods);
    is_mod_pending = false;
}

void send_report_user(uint16_t keycode) {
    static const uint16_t brcts[][2] = {
        { RCTL_T(KC_8), RALT_T(KC_9) },
        { RSFT_T(KC_7), RSFT_T(KC_7) },
        { S(KC_RBRC),   S(KC_BSLS)   },
        { S(KC_LBRC),   S(KC_LBRC)   },
        { S(KC_COMM),   S(KC_DOT)    },
        { S(KC_2),      S(KC_2)      },
        { KC_RBRC,      KC_BSLS      },
    };
    static uint16_t prev_key = 0;

    for (uint8_t i = 0; i < ARRAY_SIZE(brcts); i++) {
        if (keycode == brcts[i][1]) {
            if (prev_key == brcts[i][0]) {
                uint8_t saved_weak_mods = get_weak_mods();

                del_weak_mods(MOD_LSFT);
                tap_code(KC_LEFT);
                set_weak_mods(saved_weak_mods);
                keycode = 0;
            }
            break;
        }
    }
    prev_key = keycode;
    send_keyboard_report();
}

void tap_code_attached(uint16_t keycode, bool shifted) {
    uint8_t saved_weak_mods = get_weak_mods();

    if (shifted || is_caps_word_on()) {
        add_weak_mods(MOD_LSFT);
    }
    tap_code(keycode & 0xFF);
    set_weak_mods(saved_weak_mods);
    send_report_user(keycode);
}

void roll_taps_processed(uint16_t keycode) {
    static rt_t rts[] = {
        { LGUI_T(KC_N), { 1, 2 }, 2 },
        { LALT_T(KC_R), { 0, 3 }, 2 },
        { LCTL_T(KC_S), { 1 }, 1 },
        { RSFT_T(KC_A), { 6 }, 1 },
        { RALT_T(KC_E), { 5, 7 }, 2 },
        { RGUI_T(KC_I), { 6 }, 1 },
        { RALT_T(KC_9), { 8 }, 1 },
        { KC_D, { 1, 3 }, 2 },
        { KC_Z, { 1, 2 }, 2 },
        { KC_C, { 1 }, 1 },
        { KC_O, { 4 }, 1 },
    };

    for (uint8_t i = 0; i < ARRAY_SIZE(rts); i++) {
        if (keycode == rts[i].keycode) {
            for (uint8_t j = 0; j < rts[i].size; j++) {
                mt_t *mt = &mts[rts[i].indexes[j]];

                if (mt->interrupted || mt->shifted) {
                    tap_code_attached(mt->keycode, keycode == RALT_T(KC_9));
                    mt->interrupted = false;
                    mt->shifted = false;
                    break;
                }
            }
            return;
        }
    }
}

#define LAYER_CYCLE_START 2
#define LAYER_CYCLE_END   4

enum my_keycodes {
    KC_LNGS = SAFE_RANGE,
};
enum arrowkeys_types {
    TAB_MORPH = 1,
    VOL_MORPH,
    WWW_MORPH,
    CTRL_TAB_MORPH,
    CTRL_YanZ_MORPH,
    FOUR_MOVES_MORPH,
};
uint16_t morph_type = 0;
uint16_t morph_code = 0;
bool first_iteration = false;
bool arrowkeys_registered = false;

void four_moves(uint16_t keycode) {
    for (uint8_t i = 0; i < 4; i++) {
        tap_code(keycode);
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    for (uint8_t i = 0; i < ARRAY_SIZE(mts); i++) {
        mt_t *mt = &mts[i];

        if (keycode == mt->keycode) {
            if (record->event.pressed) {
                if (record->tap.count) {
                    if (i > 7) {
                        if (is_caps_word_on()) {
                            caps_word_off();
                        }
                        add_weak_mods(MOD_LSFT);
                    }
                    break;
                } else {
                    bool *pending_state = (record->tap.interrupted) ?
                        &mt->interrupted : &mt->shifted;

                    *pending_state = true;
                    is_mod_pending = true;
                    return false;
                }
            } else {
                if (mt->shifted) {
                    if (i > 7 && is_caps_word_on()) {
                        caps_word_off();
                    }
                    roll_taps_processed(keycode);
                    mt->shifted = false;
                    mts_mods_on();
                    tap_code_attached(keycode, i > 7);
                    return false;
                }
                mt->interrupted = false;
            }
            return true;
        }
    }

    switch (keycode) {
        case KC_MS_BTN1 ... KC_MS_BTN3:
            if (record->event.pressed) {
                clear_weak_mods();
                if (!is_mod_pending) {
                    return true;
                }
                report_mouse_t mouse_report = pointing_device_get_report();

                mts_mods_on();
                mouse_report.buttons |= MOUSE_BTN1 << (keycode - KC_MS_BTN1);
                pointing_device_set_report(mouse_report);
                return false;
            }
    }

    roll_taps_processed(keycode);
    mts_mods_on();

    switch (keycode) {
        case LT(0, KC_APP):
            if (record->event.pressed &&
                    !record->tap.count) {
                add_weak_mods(MOD_LCTL);
                tap_code(KC_PSCR);
                return false;
            }
            break;
        case LT(0, KC_F15):
            if (record->event.pressed) {
                if (record->tap.count == 1) {
                    morph_type = CTRL_YanZ_MORPH;
                } else if (record->tap.count) {
                    morph_type = 0;
                    uint8_t saved_mods = get_mods();

                    del_mods(saved_mods);
                    add_weak_mods(MOD_LCTL);
                    tap_code(KC_X);
                    set_mods(saved_mods);
                } else {
                    add_weak_mods(MOD_LALT);
                    tap_code(KC_F4);
                }
            }
            return false;
        case LT(0, KC_F16):
        case LT(0, KC_F17):
        case LT(0, KC_F18):
            uint8_t mod  =  (keycode == LT(0, KC_F16)) ?
                MOD_LALT : ((keycode == LT(0, KC_F17)) ?
                MOD_LSFT : MOD_LCTL);
            uint16_t KC_EDIT = 0;

            if (record->event.pressed) {
                if (record->tap.count == 1) {
                    if (get_mods() & mod) {
                        unregister_mods(mod);
                    } else {
                        register_mods(mod);
                        if (keycode == LT(0, KC_F16)) {
                            tap_code(KC_TAB);
                        }
                    }
                } else if (record->tap.count) {
                    if (keycode == LT(0, KC_F16)) {
                        tap_code(KC_ESC);
                        morph_type = WWW_MORPH;
                    } else if (keycode == LT(0, KC_F17)) {
                        KC_EDIT = KC_V;
                    } else {
                        morph_type = CTRL_TAB_MORPH;
                    }
                    unregister_mods(mod);
                } else {
                    if (keycode == LT(0, KC_F16)) {
                        KC_EDIT = KC_C;
                    } else {
                        tap_code((keycode == LT(0,KC_F17)) ? KC_F14 : KC_F16);
                    }
                }
                if (KC_EDIT) {
                    uint8_t saved_mods = get_mods();

                    del_mods(saved_mods);
                    add_weak_mods(MOD_LCTL);
                    tap_code(KC_EDIT);
                    set_mods(saved_mods);
                }
            }
            return false;
        case LT(0, KC_F19):
            if (record->event.pressed) {
                morph_type = record->tap.count == 1 ?
                    TAB_MORPH : (record->tap.count ? VOL_MORPH : FOUR_MOVES_MORPH);
            }
            return false;
        case LT(0, KC_F20):
            static uint16_t length = LAYER_CYCLE_END - LAYER_CYCLE_START + 1;

            if (record->event.pressed) {
                uint16_t offset_cur = get_highest_layer(layer_state) - LAYER_CYCLE_START;
                uint16_t next_layer = record->tap.count ?
                    LAYER_CYCLE_START + ((offset_cur + 1) % length + length) % length : 1;

                layer_move(next_layer);
            }
            return false;
        case LT(0, KC_LNGS):
            if (record->event.pressed) {
                tap_code(record->tap.count ? KC_LNG2 : KC_CAPS);
            }
            return false;
        case KC_LEFT:
        case KC_RGHT:
            if (record->event.pressed) {
                if (arrowkeys_registered) {
                    unregister_code(morph_code);
                    arrowkeys_registered = false;
                }
                uint8_t saved_mods = 0;

                switch (morph_type) {
                    case TAB_MORPH:
                    case CTRL_TAB_MORPH:
                        if (record->event.pressed) {
                            if (keycode == KC_LEFT) {
                                add_weak_mods(MOD_LSFT);
                            }
                            if (morph_type == CTRL_TAB_MORPH) {
                                add_weak_mods(MOD_LCTL);
                                saved_mods = get_mods();
                            }
                        }
                        morph_code = KC_TAB;
                        break;
                    case CTRL_YanZ_MORPH:
                        if (record->event.pressed) {
                            add_weak_mods(MOD_LCTL);
                            saved_mods = get_mods();
                        }
                        morph_code = (keycode == KC_LEFT) ?
                            KC_Z : KC_Y;
                        break;
                    case VOL_MORPH:
                        morph_code = (keycode == KC_LEFT) ?
                            KC_VOLD : KC_VOLU;
                        break;
                    case WWW_MORPH:
                        morph_code = (keycode == KC_LEFT) ?
                            KC_WBAK : KC_WFWD;
                        break;
                    case FOUR_MOVES_MORPH:
                        four_moves(keycode);
                        morph_code = keycode;
                        first_iteration = true;
                        arrowkeys_registered = true;
                        return false;
                    default:
                        return true;
                }
                if (saved_mods) {
                    del_mods(saved_mods);
                    register_code(morph_code);
                    set_mods(saved_mods);
                } else {
                    register_code(morph_code);
                }
                arrowkeys_registered = true;
                return false;
            } else if (arrowkeys_registered) {
                unregister_code(morph_code);
                arrowkeys_registered = false;
                return false;
            }
            break;
        case S(KC_7):
        case KC_COMM:
        case KC_MINS:
            static bool quot_registered = false;
            static bool comm_registered = false;
            static bool mins_registered = false;
            bool    *registered =       (keycode == S(KC_7)) ?
                    &quot_registered : ((keycode == KC_COMM) ?
                    &comm_registered : &mins_registered);
            uint16_t KC_MORPH = (keycode == S(KC_7)) ?
                    KC_DEL :   ((keycode == KC_COMM) ?
                    KC_DOT : KC_INT1);
            uint8_t mod_state = get_mods();

            if (record->event.pressed) {
                if (mod_state & MOD_MASK_SHIFT) {
                    if (keycode == KC_MINS) {
                        add_weak_mods(MOD_LSFT);
                    }
                    del_mods(MOD_MASK_SHIFT);
                    register_code(KC_MORPH);
                    set_mods(mod_state);
                    *registered = true;
                    return false;
                }
            } else if (*registered) {
                unregister_code(KC_MORPH);
                *registered = false;
                return false;
            }
            break;
        case LT(4, KC_ENT):
            static bool is_layer4_enabled = false;

            if (!record->tap.count) {
                is_layer4_enabled = record->event.pressed;
            }
            break;
        case LT(2, KC_SPC):
            if (!record->tap.count) {
                layer_clear();
                if (!record->event.pressed) {
                    unregister_mods(MOD_LALT | MOD_LSFT | MOD_LCTL);
                    if (morph_type) {
                        if (arrowkeys_registered) {
                            unregister_code(morph_code);
                            arrowkeys_registered = false;
                        }
                        morph_type = 0;
                    }
                    if (is_layer4_enabled) {
                        layer_on(4);
                    }
                }
            }
            break;
    }
    return true;
}

void post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        send_report_user(keycode);
    }
}

#define REPEAT_DELAY 500
#define REPEAT_INTERVAL 40

void matrix_scan_user(void) {
    static uint16_t repeat_timer = 0;

    if (morph_type == FOUR_MOVES_MORPH &&
            arrowkeys_registered) {
        if (repeat_timer == 0) {
            repeat_timer = timer_read();
        } else if (timer_elapsed(repeat_timer) > (first_iteration ?
            REPEAT_DELAY : REPEAT_INTERVAL)) {
            four_moves(morph_code);
            first_iteration = false;
            repeat_timer = timer_read();
        }
    } else {
        repeat_timer = 0;
    }
}

enum combos {
    CMB_APP,
    CMB_INT4,
    CMB_LNGS,
    CMB_PSCR,
    CMB_MS_BTN1,
    CMB_MS_BTN2,
    CMB_MS_BTN3,
    CMB_CAPS_WORD,
};

const uint16_t PROGMEM cmb_app[] = {KC_Z, KC_M, COMBO_END};
const uint16_t PROGMEM cmb_int4[] = {KC_Z, KC_C, COMBO_END};
const uint16_t PROGMEM cmb_lngs[] = {KC_M, KC_C, COMBO_END};
const uint16_t PROGMEM cmb_pscr[] = {LT(0, KC_F16), LT(0, KC_F18), COMBO_END};
const uint16_t PROGMEM cmb_ms_btn1[] = {LSFT_T(KC_T), LCTL_T(KC_S), COMBO_END};
const uint16_t PROGMEM cmb_ms_btn2[] = {LALT_T(KC_R), LSFT_T(KC_T), COMBO_END};
const uint16_t PROGMEM cmb_ms_btn3[] = {LALT_T(KC_R), LCTL_T(KC_S), COMBO_END};
const uint16_t PROGMEM cmb_caps_word[] = {LSFT_T(KC_T), RSFT_T(KC_A), COMBO_END};

combo_t key_combos[] = {
    [CMB_APP] = COMBO(cmb_app, LT(0, KC_APP)),
    [CMB_INT4] = COMBO(cmb_int4, KC_INT4),
    [CMB_LNGS] = COMBO(cmb_lngs, LT(0, KC_LNGS)),
    [CMB_PSCR] = COMBO(cmb_pscr, KC_PSCR),
    [CMB_MS_BTN1] = COMBO(cmb_ms_btn1, KC_MS_BTN1),
    [CMB_MS_BTN2] = COMBO(cmb_ms_btn2, KC_MS_BTN2),
    [CMB_MS_BTN3] = COMBO(cmb_ms_btn3, KC_MS_BTN3),
    [CMB_CAPS_WORD] = COMBO(cmb_caps_word, QK_CAPS_WORD_TOGGLE),
};

bool get_combo_must_hold(uint16_t combo_index, combo_t *combo) {
    switch (combo_index) {
        case CMB_CAPS_WORD:
            return true;
    }
    return false;
}

bool caps_word_press_user(uint16_t keycode) {
    switch (keycode) {
        case KC_A ... KC_Z:
            add_weak_mods(MOD_LSFT);
            return true;
        case KC_1 ... KC_0:
        case KC_BSPC:
        case KC_MINS:
        case S(KC_7):
            return true;
    }
    return false;
}

#define COMBO_REF_DEFAULT 0

uint8_t combo_ref_from_layer(uint8_t layer) {
    switch (get_highest_layer(layer_state)) {
        case 2: return 2;
        default: return COMBO_REF_DEFAULT;
    }
}

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    if (abs(mouse_report.x) + abs(mouse_report.y)) {
        mts_mods_on();
    }
    return mouse_report;
}

uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case LT(0, KC_F20):
        case LT(2, KC_SPC):
        case LT(4, KC_ENT):
        case LT(0, KC_APP):
        case LT(0, KC_LNGS):
            return TAPPING_TERM + 50;
    }
    return TAPPING_TERM;
}

uint16_t get_quick_tap_term(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case LT(0, KC_F20):
        case LT(2, KC_SPC):
        case LT(4, KC_ENT):
            return 0;
    }
    return QUICK_TAP_TERM;
}

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  // keymap for default (VIA)
  [0] = LAYOUT_universal(
    KC_Q     , KC_W     , KC_E     , KC_R     , KC_T     ,                            KC_Y     , KC_U     ,
 KC_I     , KC_O     , KC_P     ,
    KC_A     , KC_S     , KC_D     , KC_F     , KC_G     ,                            KC_H     , KC_J     ,
 KC_K     , KC_L     , KC_MINS  ,
    KC_Z     , KC_X     , KC_C     , KC_V     , KC_B     ,                            KC_N     , KC_M     ,
 KC_COMM  , KC_DOT   , KC_SLSH  ,
    KC_LCTL  , KC_LGUI  , KC_LALT  ,LSFT_T(KC_LNG2),LT(1,KC_SPC),LT(3,KC_LNG1),KC_BSPC,LT(2,KC_ENT),LSFT_T(
KC_LNG2),KC_RALT,KC_RGUI, KC_RSFT
  ),

  [1] = LAYOUT_universal(
    KC_F1    , KC_F2    , KC_F3    , KC_F4    , KC_RBRC  ,                            KC_F6    , KC_F7    ,
 KC_F8    , KC_F9    , KC_F10   ,
    KC_F5    , KC_EXLM  , S(KC_6)  ,S(KC_INT3), S(KC_8)  ,                           S(KC_INT1), KC_BTN1  ,
 KC_PGUP  , KC_BTN2  , KC_SCLN  ,
    S(KC_EQL),S(KC_LBRC),S(KC_7)   , S(KC_2)  ,S(KC_RBRC),                            KC_LBRC  , KC_DLR   ,
 KC_PGDN  , KC_BTN3  , KC_F11   ,
    KC_INT1  , KC_EQL   , S(KC_3)  , _______  , _______  , _______  ,      TO(2)    , TO(0)    , _______  ,
 KC_RALT  , KC_RGUI  , KC_F12
  ),

  [2] = LAYOUT_universal(
    KC_TAB   , KC_7     , KC_8     , KC_9     , KC_MINS  ,                            KC_NUHS  , _______  ,
 KC_BTN3  , _______  , KC_BSPC  ,
   S(KC_QUOT), KC_4     , KC_5     , KC_6     ,S(KC_SCLN),                            S(KC_9)  , KC_BTN1  ,
 KC_UP    , KC_BTN2  , KC_QUOT  ,
    KC_SLSH  , KC_1     , KC_2     , KC_3     ,S(KC_MINS),                           S(KC_NUHS), KC_LEFT  ,
 KC_DOWN  , KC_RGHT  , _______  ,
    KC_ESC   , KC_0     , KC_DOT   , KC_DEL   , KC_ENT   , KC_BSPC  ,      _______  , _______  , _______  ,
 _______  , _______  , _______
  ),

  [3] = LAYOUT_universal(
    RGB_TOG  , AML_TO   , AML_I50  , AML_D50  , _______  ,                            _______  , _______  ,
 SSNP_HOR , SSNP_VRT , SSNP_FRE ,
    RGB_MOD  , RGB_HUI  , RGB_SAI  , RGB_VAI  , SCRL_DVI ,                            _______  , _______  ,
 _______  , _______  , _______  ,
    RGB_RMOD , RGB_HUD  , RGB_SAD  , RGB_VAD  , SCRL_DVD ,                            CPI_D1K  , CPI_D100 ,
 CPI_I100 , CPI_I1K  , KBC_SAVE ,
    QK_BOOT  , KBC_RST  , _______  , _______  , _______  , _______  ,      _______  , _______  , _______  ,
 _______  , KBC_RST  , QK_BOOT
  ),
};
// clang-format on

layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode when the highest layer is 3
    keyball_set_scroll_mode(get_highest_layer(state) == 3);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    keyball_handle_auto_mouse_layer_change(state);
#endif
    return state;
}

#ifdef OLED_ENABLE

#    include "lib/oledkit/oledkit.h"

void oledkit_render_info_user(void) {
    keyball_oled_render_keyinfo();
    keyball_oled_render_ballinfo();
    keyball_oled_render_layerinfo();
}
#endif
