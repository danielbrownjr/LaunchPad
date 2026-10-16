/*
 * Small QWERTY map for the 800x480 touchscreen.
 *
 * LVGL 9.4's default bottom-left LV_SYMBOL_KEYBOARD is a CANCEL key.  Since
 * this form has its own Back/Close controls, use that corner for case shift
 * instead.  "ABC"/"abc" are LVGL's built-in case-mode selectors and work
 * without synthesizing text or attaching keyboard-specific event callbacks.
 */
#include "wifi_keyboard.h"

#define ALPHA_BUTTONS 37
#define SPECIAL_BUTTONS 40

static const char *const s_lower[] = {
    "1#", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", LV_SYMBOL_BACKSPACE, "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
    "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "\n",
    "ABC", "-", "_", " ", ":", LV_SYMBOL_OK, ""
};

static const char *const s_upper[] = {
    "1#", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", LV_SYMBOL_BACKSPACE, "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
    "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "\n",
    "abc", "-", "_", " ", ":", LV_SYMBOL_OK, ""
};

static const char *const s_special[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "+", "&", "/", "*", "=", "%", "!", "?", "#", "<", ">", "\n",
    "\\", "@", "$", "(", ")", "{", "}", "[", "]", ";", "\"", "'", "\n",
    "abc", LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};

/* Other buttons use LVGL's default width 1. Control maps describe buttons,
 * excluding the '\n' row boundaries and the terminal empty string. */
static const lv_buttonmatrix_ctrl_t s_alpha_ctrl[ALPHA_BUTTONS] = {
    [0] = LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_WIDTH_2, /* 1# */
    [11] = LV_BUTTONMATRIX_CTRL_WIDTH_2, /* Backspace */
    [31] = LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_WIDTH_2, /* Shift */
    [34] = LV_BUTTONMATRIX_CTRL_WIDTH_6, /* Space */
    [36] = LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_WIDTH_2, /* OK */
};

static const lv_buttonmatrix_ctrl_t s_special_ctrl[SPECIAL_BUTTONS] = {
    [10] = LV_BUTTONMATRIX_CTRL_WIDTH_2, /* Backspace */
    [11] = LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_WIDTH_2, /* abc */
    [35] = LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_WIDTH_2, /* abc */
    [36] = LV_BUTTONMATRIX_CTRL_WIDTH_2, /* Left */
    [37] = LV_BUTTONMATRIX_CTRL_WIDTH_6, /* Space */
    [38] = LV_BUTTONMATRIX_CTRL_WIDTH_2, /* Right */
    [39] = LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_WIDTH_2, /* OK */
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
_Static_assert(COUNT(s_lower) == ALPHA_BUTTONS + 4, "lower keyboard map/buttons mismatch");
_Static_assert(COUNT(s_upper) == ALPHA_BUTTONS + 4, "upper keyboard map/buttons mismatch");
_Static_assert(COUNT(s_special) == SPECIAL_BUTTONS + 4, "special keyboard map/buttons mismatch");

void lp_wifi_keyboard_apply_layout(lv_obj_t *keyboard)
{
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, s_lower, s_alpha_ctrl);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, s_upper, s_alpha_ctrl);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_SPECIAL, s_special, s_special_ctrl);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
}
