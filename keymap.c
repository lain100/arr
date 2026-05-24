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
	{ LGUI_T(KC_R) },
	{ LALT_T(KC_S) },
	{ LSFT_T(KC_N) },
	{ LCTL_T(KC_T) },
	{ RCTL_T(KC_O) },
	{ RSFT_T(KC_A) },
	{ RALT_T(KC_I) },
	{ RGUI_T(KC_E) },
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
		{ RCTL_T(KC_8),	RALT_T(KC_9) },
		{ RSFT_T(KC_7),	RSFT_T(KC_7) },
		{ KC_RBRC, KC_BSLS },
		{ S(KC_2), S(KC_2) },
		{ S(KC_RBRC), S(KC_BSLS) },
		{ S(KC_LBRC), S(KC_LBRC) },
		{ S(KC_COMM), S(KC_DOT) },
	};
	static uint16_t prev_key = 0;

	send_keyboard_report();
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
		{ LGUI_T(KC_R), { 1, 2 }, 2 },
		{ LALT_T(KC_S), { 0 }, 1 },
		{ RSFT_T(KC_A), { 6 }, 1 },
		{ RALT_T(KC_I), { 5, 7 }, 2 },
		{ RGUI_T(KC_E), { 6 }, 1 },
		{ RALT_T(KC_9), { 8 }, 1 },
		{ KC_D, { 1, 2 }, 2 },
		{ KC_F, { 1 }, 1 },
		{ KC_H, { 3 }, 1 },
		{ KC_U, { 4 }, 1 },
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

#define LAYER_CYCLE_START 0
#define LAYER_CYCLE_END   4

enum arrowkeys_morph {
	VOL_MORPH = 1,
	WWW_MORPH,
	TAB_MORPH,
};
uint16_t morph_type = 0;

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

	roll_taps_processed(keycode);
	mts_mods_on();

	switch (keycode) {
		case LT(0, KC_F15):
			if (record->event.pressed) {
				if (record->tap.count) {
					add_weak_mods(MOD_LALT);
					tap_code(KC_F4);
				} else {
					add_weak_mods(MOD_LCTL);
					register_code(KC_Z);
				}
			} else if (!record->tap.count) {
				unregister_code(KC_Z);
			}
			break;
		case LT(0, KC_F16):
			static bool is_ctrl_alt_tab = false;

			if (record->event.pressed) {
				if (record->tap.count) {
					add_weak_mods(MOD_LCTL | MOD_LALT);
					tap_code(KC_TAB);
					is_ctrl_alt_tab = true;
				} else {
					add_weak_mods(MOD_LCTL);
					register_code(KC_Y);
				}
			} else if (!record->tap.count) {
				unregister_code(KC_Y);
			}
			break;
		case LT(0, KC_F17):
			if (record->event.pressed) {
				if (record->tap.count) {
					if (get_mods() & MOD_LSFT) {
						unregister_mods(MOD_LSFT);
					} else {
						register_mods(MOD_LSFT);
					}
				} else {
					tap_code(KC_F14);
				}
			}
			break;
		case LT(0, KC_F18):
			if (record->event.pressed) {
				if (record->tap.count) {
					if (get_mods() & MOD_LCTL) {
						unregister_mods(MOD_LCTL);
					} else {
						register_mods(MOD_LCTL);
					}
					morph_type = (morph_type == TAB_MORPH) ? 0 : TAB_MORPH;
				} else {
					tap_code(KC_F16);
				}
			}
			break;
		case LT(0, KC_F19):
			if (record->event.pressed) {
				tap_code(record->tap.count ? KC_PSCR : KC_APP);
			}
			break;
		case LT(0, KC_F20):
			static uint16_t length = LAYER_CYCLE_END - LAYER_CYCLE_START + 1;

			if (record->event.pressed) {
				uint16_t offset_cur = get_highest_layer(layer_state) - LAYER_CYCLE_START;
				uint16_t next_layer = record->tap.count ?
					LAYER_CYCLE_START + ((offset_cur + 1) % length + length) % length : 0;

				layer_move(next_layer);
			}
			return false;
		case KC_LEFT:
		case KC_RGHT:
			static bool arrowkeys_registered = false;
			static uint16_t morph_code = 0;

			if (record->event.pressed) {
				if (arrowkeys_registered) {
					unregister_code(morph_code);
					arrowkeys_registered = false;
				}
				switch (morph_type) {
					case VOL_MORPH:
						morph_code = (keycode == KC_LEFT) ?
							KC_VOLD : KC_VOLU;
						break;
					case WWW_MORPH:
						morph_code = (keycode == KC_LEFT) ?
							KC_WBAK : KC_WFWD;
						break;
					case TAB_MORPH:
						if (record->event.pressed &&
							keycode == KC_LEFT) {
							add_weak_mods(MOD_LSFT);
						}
						morph_code = KC_TAB;
						break;
					default:
						return true;
				}
				register_code(morph_code);
				arrowkeys_registered = true;
				return false;
			} else if (arrowkeys_registered) {
				unregister_code(morph_code);
				arrowkeys_registered = false;
				return false;
			}
			break;
		case KC_BSPC:
		case KC_COMM:
		case S(KC_7):
			static bool bspc_registered = false;
			static bool comm_registered = false;
			static bool quot_registered = false;
			bool 	*registered = 		(keycode == KC_BSPC) ?
					&bspc_registered : ((keycode == KC_COMM) ?
					&comm_registered : &quot_registered);
			uint16_t code =	  (keycode == KC_BSPC) ?
					KC_DEL : ((keycode == KC_COMM) ?
					KC_DOT : KC_MINS);
			uint8_t mod_state = get_mods();

			if (record->event.pressed) {
				if (mod_state & MOD_MASK_SHIFT) {
					del_mods(MOD_MASK_SHIFT);
					register_code(code);
					*registered = true;
					set_mods(mod_state);
					return false;
				}
			} else if (*registered) {
				unregister_code(code);
				*registered = false;
				return false;
			}
			break;
		case LT(2, KC_SPC):
			if (!record->tap.count) {
				if (!record->event.pressed) {
					unregister_mods(MOD_LSFT | MOD_LCTL);
					if (is_ctrl_alt_tab) {
						tap_code(KC_ENT);
						is_ctrl_alt_tab = false;
					}
					if (morph_type) {
						if (arrowkeys_registered) {
							unregister_code(morph_code);
							arrowkeys_registered = false;
						}
						morph_type = 0;
					}
				}
				layer_clear();
			}
	}
	return true;
}

void post_process_record_user(uint16_t keycode, keyrecord_t *record) {
	if (record->event.pressed) {
		send_report_user(keycode);
	}
}

enum combos {
	CMB_INT4,
	CMB_LNG1,
	CMB_LNG2,
	CMB_CAPS,
	CMB_MS_BTN1,
	CMB_MS_BTN2,
	CMB_MS_BTN3,
	CMB_CAPS_WORD,
	CMB_VOL_MORPH,
	CMB_WWW_MORPH,
	CMB_TAB_MORPH,
};

const uint16_t PROGMEM cmb_int4[] = {KC_L, KC_W, COMBO_END};
const uint16_t PROGMEM cmb_lng1[] = {KC_D, KC_M, COMBO_END};
const uint16_t PROGMEM cmb_lng2[] = {KC_M, KC_F, COMBO_END};
const uint16_t PROGMEM cmb_caps[] = {KC_D, KC_F, COMBO_END};
const uint16_t PROGMEM cmb_ms_btn1[] = {LSFT_T(KC_N), LCTL_T(KC_T), COMBO_END};
const uint16_t PROGMEM cmb_ms_btn2[] = {LALT_T(KC_S), LSFT_T(KC_N), COMBO_END};
const uint16_t PROGMEM cmb_ms_btn3[] = {LALT_T(KC_S), LCTL_T(KC_T), COMBO_END};
const uint16_t PROGMEM cmb_caps_word[] = {LSFT_T(KC_N), RSFT_T(KC_A), COMBO_END};
const uint16_t PROGMEM cmb_vol_morph[] = {LT(0, KC_F16), LT(0, KC_F17), COMBO_END};
const uint16_t PROGMEM cmb_www_morph[] = {LT(0, KC_F16), LT(0, KC_F18), COMBO_END};
const uint16_t PROGMEM cmb_tab_morph[] = {LT(0, KC_F17), LT(0, KC_F18), COMBO_END};

combo_t key_combos[] = {
	[CMB_INT4] = COMBO(cmb_int4, KC_INT4),
	[CMB_LNG1] = COMBO(cmb_lng1, KC_LNG1),
	[CMB_LNG2] = COMBO(cmb_lng2, KC_LNG2),
	[CMB_CAPS] = COMBO(cmb_caps, KC_CAPS),
	[CMB_MS_BTN1] = COMBO(cmb_ms_btn1, KC_MS_BTN1),
	[CMB_MS_BTN2] = COMBO(cmb_ms_btn2, KC_MS_BTN2),
	[CMB_MS_BTN3] = COMBO(cmb_ms_btn3, KC_MS_BTN3),
	[CMB_CAPS_WORD] = COMBO_ACTION(cmb_caps_word),
	[CMB_VOL_MORPH] = COMBO_ACTION(cmb_vol_morph),
	[CMB_WWW_MORPH] = COMBO_ACTION(cmb_www_morph),
	[CMB_TAB_MORPH] = COMBO_ACTION(cmb_tab_morph),
};

void process_combo_event(uint16_t combo_index, bool pressed) {
	if (pressed) {
		switch (combo_index) {
			case CMB_CAPS_WORD:
				caps_word_on();
				break;
			case CMB_VOL_MORPH:
				morph_type = (morph_type == VOL_MORPH) ? 0 : VOL_MORPH;
				break;
			case CMB_WWW_MORPH:
				morph_type = (morph_type == WWW_MORPH) ? 0 : WWW_MORPH;
				break;
			case CMB_TAB_MORPH:
				morph_type = (morph_type == TAB_MORPH) ? 0 : TAB_MORPH;
		}
	}
}

bool caps_word_press_user(uint16_t keycode) {
	switch (keycode) {
		case KC_A ... KC_Z:
			add_weak_mods(MOD_LSFT);
			return true;

		case KC_1 ... KC_0:
		case KC_BSPC:
		case KC_MINS:
		case S(KC_BSPC):
		case S(KC_INT1):
			return true;
	}
	return false;
}

#define MODS_ACTIVATE_THRESHOLD 1
#define LAYER_CLEAR_THRESHOLD 10

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
	uint16_t total_move = abs(mouse_report.x) + abs(mouse_report.y);

	if (total_move > MODS_ACTIVATE_THRESHOLD) {
		mts_mods_on();
	}
	if (total_move > LAYER_CLEAR_THRESHOLD) {
		layer_clear();
	}
	return mouse_report;
}

uint16_t get_combo_term(uint16_t combo_index, combo_t* combo) {
	switch (combo_index) {
		case CMB_CAPS_WORD:
			return COMBO_TERM - 20;
	}
	return COMBO_TERM;
}

uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
	switch (keycode) {
		case LT(0, KC_F20):
		case LT(2, KC_SPC):
		case LT(4, KC_ENT):
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
