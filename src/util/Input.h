//
// Created by student on 10/5/23.
//

#ifndef BIRD_INPUT_H
#define BIRD_INPUT_H

#include <string>
#include <vector>

#include "../renders/Window.h"
#include "Math.h"

namespace bird {

/**
 * Logical action mapping that bundles multiple physical input triggers.
 *
 * Can bind multiple keyboard keys (addKey) and mouse buttons (addMouseButton).
 * Maintains an analog intensity value in [0.0, 1.0].
 */
class Action {
   public:
	/**
	 * Creates a named action mapping (e.g. "move_forward", "jump").
	 * @param name Unique string identifier for the action.
	 */
	Action(std::string name);

	/** Binds a keyboard key code to this action. Returns *this for chaining. */
	Action& addKey(int keyCode);
	/** Binds a mouse button code to this action. Returns *this for chaining. */
	Action& addMouseButton(int mouseCode);

	/** Returns normalized action value between 0.0 (inactive) and 1.0 (active). */
	float getValue();
	void setValue(float value);

	std::vector<int>& getKeys();
	std::vector<int>& getMouseButtons();
	std::string getName();

   private:
	float m_value = 0;

	std::string m_name;
	std::vector<int> m_keys;
	std::vector<int> m_mouseButtons;
};

/**
 * Global input management subsystem.
 *
 * Maintains registered actions, captures platform window events from GLFW,
 * and tracks mouse cursor position and movement.
 */
class Input {
   public:
	/**
	 * Creates the input subsystem attached to a platform window.
	 * Registers GLFW keyboard and mouse event callbacks.
	 */
	Input(Window* window);

	/**
	 * Registers a logical action.
	 */
	void addAction(Action action);

	/**
	 * Queries the current activation intensity of a named action.
	 * @param name Identifier of the action registered via addAction().
	 * @return Float between 0.0 and 1.0 (0.0 if action not registered or not pressed).
	 */
	float getActionValue(std::string name);

	/**
	 * Controls mouse cursor visibility and capturing mode.
	 * @param cursorMode CURSOR_NORMAL, CURSOR_HIDDEN, or CURSOR_DISABLED.
	 */
	void setMouseMode(int cursorMode);

	/** Enables or disables unaccelerated raw mouse motion. */
	void setMouseRawMotion(bool rawMotion);

	/**
	 * Captures the mouse for camera control: the cursor is hidden and
	 * locked to the window, motion is raw where supported, and the UI stops
	 * receiving pointer events (and loses keyboard focus) until released.
	 */
	void setMouseCaptured(bool captured);
	bool isMouseCaptured() const { return m_captured; }

	/**
	 * Cursor movement in window pixels since the previous tick (+x right,
	 * +y down). Zero on the tick that follows a capture change, so toggling
	 * never produces a jump.
	 */
	Vector2 getMouseDelta() const { return m_mouseDelta; }
	/** Cursor position in window pixels at the last tick. */
	Vector2 getMousePosition() const { return Vector2(m_mx, m_my); }

	/**
	 * Per-frame input update tick. Evaluates current key and mouse states
	 * against registered actions.
	 */
	void tick();

   private:
	Window* m_window;
	double m_mx = 0.0, m_my = 0.0;
	Vector2 m_mouseDelta{0.0f};
	bool m_captured = false;
	bool m_resetDelta = true;  // first tick, or capture changed

	std::vector<Action> m_actions;
};

void mouse_button_callback(GLFWwindow* window, int button, int action,
						   int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void cursor_enter_callback(GLFWwindow* window, int entered);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void joystick_callback(int jid, int event);
void character_callback(GLFWwindow* window, unsigned int codepoint);
void key_callback(GLFWwindow* window, int key, int scancode, int action,
				  int mods);
}  // namespace bird

#define ACTION_NONE -1
#define ACTION_RELEASE 0
#define ACTION_PRESS 1

#define CURSOR_DISABLED 0x00034003
#define CURSOR_HIDDEN 0x00034002
#define CURSOR_NORMAL 0x00034001

#define MOUSE_BUTTON_1 0
#define MOUSE_BUTTON_2 1
#define MOUSE_BUTTON_3 2
#define MOUSE_BUTTON_4 3
#define MOUSE_BUTTON_5 4
#define MOUSE_BUTTON_6 5
#define MOUSE_BUTTON_7 6
#define MOUSE_BUTTON_8 7
#define MOUSE_BUTTON_LAST MOUSE_BUTTON_8
#define MOUSE_BUTTON_LEFT MOUSE_BUTTON_1
#define MOUSE_BUTTON_RIGHT MOUSE_BUTTON_2
#define MOUSE_BUTTON_MIDDLE MOUSE_BUTTON_3

#define KEY_UNKNOWN -1
#define KEY_SPACE 32
#define KEY_APOSTROPHE 39 /* ' */
#define KEY_COMMA 44	  /* , */
#define KEY_MINUS 45	  /* - */
#define KEY_PERIOD 46	  /* . */
#define KEY_SLASH 47	  /* / */
#define KEY_0 48
#define KEY_1 49
#define KEY_2 50
#define KEY_3 51
#define KEY_4 52
#define KEY_5 53
#define KEY_6 54
#define KEY_7 55
#define KEY_8 56
#define KEY_9 57
#define KEY_SEMICOLON 59 /* ; */
#define KEY_EQUAL 61	 /* = */
#define KEY_A 65
#define KEY_B 66
#define KEY_C 67
#define KEY_D 68
#define KEY_E 69
#define KEY_F 70
#define KEY_G 71
#define KEY_H 72
#define KEY_I 73
#define KEY_J 74
#define KEY_K 75
#define KEY_L 76
#define KEY_M 77
#define KEY_N 78
#define KEY_O 79
#define KEY_P 80
#define KEY_Q 81
#define KEY_R 82
#define KEY_S 83
#define KEY_T 84
#define KEY_U 85
#define KEY_V 86
#define KEY_W 87
#define KEY_X 88
#define KEY_Y 89
#define KEY_Z 90
#define KEY_LEFT_BRACKET 91	 /* [ */
#define KEY_BACKSLASH 92	 /* \ */
#define KEY_RIGHT_BRACKET 93 /* ] */
#define KEY_GRAVE_ACCENT 96	 /* ` */
#define KEY_WORLD_1 161		 /* non-US #1 */
#define KEY_WORLD_2 162		 /* non-US #2 */
#define KEY_ESCAPE 256
#define KEY_ENTER 257
#define KEY_TAB 258
#define KEY_BACKSPACE 259
#define KEY_INSERT 260
#define KEY_DELETE 261
#define KEY_RIGHT 262
#define KEY_LEFT 263
#define KEY_DOWN 264
#define KEY_UP 265
#define KEY_PAGE_UP 266
#define KEY_PAGE_DOWN 267
#define KEY_HOME 268
#define KEY_END 269
#define KEY_CAPS_LOCK 280
#define KEY_SCROLL_LOCK 281
#define KEY_NUM_LOCK 282
#define KEY_PRINT_SCREEN 283
#define KEY_PAUSE 284
#define KEY_F1 290
#define KEY_F2 291
#define KEY_F3 292
#define KEY_F4 293
#define KEY_F5 294
#define KEY_F6 295
#define KEY_F7 296
#define KEY_F8 297
#define KEY_F9 298
#define KEY_F10 299
#define KEY_F11 300
#define KEY_F12 301
#define KEY_F13 302
#define KEY_F14 303
#define KEY_F15 304
#define KEY_F16 305
#define KEY_F17 306
#define KEY_F18 307
#define KEY_F19 308
#define KEY_F20 309
#define KEY_F21 310
#define KEY_F22 311
#define KEY_F23 312
#define KEY_F24 313
#define KEY_F25 314
#define KEY_KP_0 320
#define KEY_KP_1 321
#define KEY_KP_2 322
#define KEY_KP_3 323
#define KEY_KP_4 324
#define KEY_KP_5 325
#define KEY_KP_6 326
#define KEY_KP_7 327
#define KEY_KP_8 328
#define KEY_KP_9 329
#define KEY_KP_DECIMAL 330
#define KEY_KP_DIVIDE 331
#define KEY_KP_MULTIPLY 332
#define KEY_KP_SUBTRACT 333
#define KEY_KP_ADD 334
#define KEY_KP_ENTER 335
#define KEY_KP_EQUAL 336
#define KEY_LEFT_SHIFT 340
#define KEY_LEFT_CONTROL 341
#define KEY_LEFT_ALT 342
#define KEY_LEFT_SUPER 343
#define KEY_RIGHT_SHIFT 344
#define KEY_RIGHT_CONTROL 345
#define KEY_RIGHT_ALT 346
#define KEY_RIGHT_SUPER 347
#define KEY_MENU 348
#define KEY_LAST KEY_MENU

#endif	// BIRD_INPUT_H
