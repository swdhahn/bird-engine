#include "AndroidKeyMap.h"

#include <android/input.h>
#include <android/keycodes.h>

#include "../../util/Input.h"

namespace bird::platform {

int engineKeyFromAndroid(int32_t code) {
	if (code >= AKEYCODE_A && code <= AKEYCODE_Z) {
		return KEY_A + (code - AKEYCODE_A);
	}
	if (code >= AKEYCODE_0 && code <= AKEYCODE_9) {
		return KEY_0 + (code - AKEYCODE_0);
	}
	if (code >= AKEYCODE_F1 && code <= AKEYCODE_F12) {
		return KEY_F1 + (code - AKEYCODE_F1);
	}
	if (code >= AKEYCODE_NUMPAD_0 && code <= AKEYCODE_NUMPAD_9) {
		return KEY_KP_0 + (code - AKEYCODE_NUMPAD_0);
	}
	switch (code) {
		case AKEYCODE_SPACE: return KEY_SPACE;
		case AKEYCODE_APOSTROPHE: return KEY_APOSTROPHE;
		case AKEYCODE_COMMA: return KEY_COMMA;
		case AKEYCODE_MINUS: return KEY_MINUS;
		case AKEYCODE_PERIOD: return KEY_PERIOD;
		case AKEYCODE_SLASH: return KEY_SLASH;
		case AKEYCODE_SEMICOLON: return KEY_SEMICOLON;
		case AKEYCODE_EQUALS: return KEY_EQUAL;
		case AKEYCODE_LEFT_BRACKET: return KEY_LEFT_BRACKET;
		case AKEYCODE_BACKSLASH: return KEY_BACKSLASH;
		case AKEYCODE_RIGHT_BRACKET: return KEY_RIGHT_BRACKET;
		case AKEYCODE_GRAVE: return KEY_GRAVE_ACCENT;
		case AKEYCODE_ESCAPE: return KEY_ESCAPE;
		case AKEYCODE_ENTER: return KEY_ENTER;
		case AKEYCODE_NUMPAD_ENTER: return KEY_KP_ENTER;
		case AKEYCODE_TAB: return KEY_TAB;
		case AKEYCODE_DEL: return KEY_BACKSPACE;	 // Android's "delete" is backspace
		case AKEYCODE_FORWARD_DEL: return KEY_DELETE;
		case AKEYCODE_INSERT: return KEY_INSERT;
		case AKEYCODE_DPAD_RIGHT: return KEY_RIGHT;
		case AKEYCODE_DPAD_LEFT: return KEY_LEFT;
		case AKEYCODE_DPAD_DOWN: return KEY_DOWN;
		case AKEYCODE_DPAD_UP: return KEY_UP;
		case AKEYCODE_PAGE_UP: return KEY_PAGE_UP;
		case AKEYCODE_PAGE_DOWN: return KEY_PAGE_DOWN;
		case AKEYCODE_MOVE_HOME: return KEY_HOME;
		case AKEYCODE_MOVE_END: return KEY_END;
		case AKEYCODE_CAPS_LOCK: return KEY_CAPS_LOCK;
		case AKEYCODE_SHIFT_LEFT: return KEY_LEFT_SHIFT;
		case AKEYCODE_SHIFT_RIGHT: return KEY_RIGHT_SHIFT;
		case AKEYCODE_CTRL_LEFT: return KEY_LEFT_CONTROL;
		case AKEYCODE_CTRL_RIGHT: return KEY_RIGHT_CONTROL;
		case AKEYCODE_ALT_LEFT: return KEY_LEFT_ALT;
		case AKEYCODE_ALT_RIGHT: return KEY_RIGHT_ALT;
		case AKEYCODE_META_LEFT: return KEY_LEFT_SUPER;
		case AKEYCODE_META_RIGHT: return KEY_RIGHT_SUPER;
		default: return KEY_UNKNOWN;
	}
}

uint8_t engineModifiersFromAndroid(int32_t metaState) {
	uint8_t bits = 0;
	if (metaState & AMETA_SHIFT_ON) bits |= 1;
	if (metaState & AMETA_CTRL_ON) bits |= 2;
	if (metaState & AMETA_ALT_ON) bits |= 4;
	if (metaState & AMETA_META_ON) bits |= 8;
	return bits;
}

}  // namespace bird::platform
