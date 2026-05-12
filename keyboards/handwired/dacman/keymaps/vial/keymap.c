#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y,
        KC_A, KC_S, KC_D, KC_F, KC_G, KC_H,
        KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N,

        KC_U, KC_I, KC_O, KC_P, KC_BSPC, KC_SPC,
        KC_J, KC_K, KC_L, KC_SCLN, KC_QUOT, KC_ENT,
        KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_ESC, KC_TAB
    )
};
