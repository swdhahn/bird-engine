#ifndef BIRD_UIEVENTS_H
#define BIRD_UIEVENTS_H

#include <cstdint>

#include "../../util/Math.h"

namespace bird {

/** Modifier bit flags, independent of the windowing library. */
enum UIModifier : uint8_t {
	UI_MOD_NONE = 0,
	UI_MOD_SHIFT = 1,
	UI_MOD_CONTROL = 2,
	UI_MOD_ALT = 4,
	UI_MOD_SUPER = 8
};

enum class UIMouseButton : uint8_t { Left, Right, Middle };

/** Keys the UI reacts to; everything else arrives as UIKey::Unknown. */
enum class UIKey : uint8_t {
	Unknown,
	Backspace,
	Delete,
	Left,
	Right,
	Up,
	Down,
	Home,
	End,
	PageUp,
	PageDown,
	Enter,
	Escape,
	Tab,
	Space,
	A,
	C,
	V,
	X
};

struct UIMouseEvent {
	Vector2 position;  // window coordinates
	UIMouseButton button = UIMouseButton::Left;
	uint8_t modifiers = UI_MOD_NONE;
	int clickCount = 1;	 // 2 for a double click
};

struct UIScrollEvent {
	Vector2 position;
	Vector2 delta;	// wheel notches; positive y scrolls up
};

struct UIKeyEvent {
	UIKey key = UIKey::Unknown;
	uint8_t modifiers = UI_MOD_NONE;
	bool repeat = false;
	int keyCode = -1;  // engine key code (KEY_*), -1 if unknown

	bool shift() const { return modifiers & UI_MOD_SHIFT; }
	bool control() const { return modifiers & UI_MOD_CONTROL; }
};

}  // namespace bird

#endif	// !BIRD_UIEVENTS_H
