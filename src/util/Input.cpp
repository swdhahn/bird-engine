
// Created by student on 10/5/23.
//

#include "Input.h"

#include <GLFW/glfw3.h>

#include "../Application.h"
#include "../scene/2D/UIManager.h"

namespace {

bird::UIManager* uiManager() {
	return bird::APPLICATION ? bird::APPLICATION->getUIManager() : nullptr;
}

/** GLFW and UI modifier bits share values (shift 1, ctrl 2, alt 4, super 8). */
uint8_t uiModifiers(int mods) {
	return static_cast<uint8_t>(mods & 0x0F);
}

bird::UIKey uiKey(int key) {
	using bird::UIKey;
	switch (key) {
		case GLFW_KEY_BACKSPACE: return UIKey::Backspace;
		case GLFW_KEY_DELETE: return UIKey::Delete;
		case GLFW_KEY_LEFT: return UIKey::Left;
		case GLFW_KEY_RIGHT: return UIKey::Right;
		case GLFW_KEY_UP: return UIKey::Up;
		case GLFW_KEY_DOWN: return UIKey::Down;
		case GLFW_KEY_HOME: return UIKey::Home;
		case GLFW_KEY_END: return UIKey::End;
		case GLFW_KEY_PAGE_UP: return UIKey::PageUp;
		case GLFW_KEY_PAGE_DOWN: return UIKey::PageDown;
		case GLFW_KEY_ENTER:
		case GLFW_KEY_KP_ENTER: return UIKey::Enter;
		case GLFW_KEY_ESCAPE: return UIKey::Escape;
		case GLFW_KEY_TAB: return UIKey::Tab;
		case GLFW_KEY_SPACE: return UIKey::Space;
		case GLFW_KEY_A: return UIKey::A;
		case GLFW_KEY_C: return UIKey::C;
		case GLFW_KEY_V: return UIKey::V;
		case GLFW_KEY_X: return UIKey::X;
		default: return UIKey::Unknown;
	}
}

bird::UIMouseButton uiButton(int button) {
	switch (button) {
		case GLFW_MOUSE_BUTTON_RIGHT: return bird::UIMouseButton::Right;
		case GLFW_MOUSE_BUTTON_MIDDLE: return bird::UIMouseButton::Middle;
		default: return bird::UIMouseButton::Left;
	}
}

}  // namespace

namespace bird {

Action::Action(std::string name) : m_name(name) {}

Action& Action::addKey(int keyCode) {
	m_keys.push_back(keyCode);
	return *this;
}

Action& Action::addMouseButton(int mouseCode) {
	m_mouseButtons.push_back(mouseCode);
	return *this;
}

float Action::getValue() {
	return m_value;
}

void Action::setValue(float value) {
	m_value = value;
}

std::vector<int>& Action::getKeys() {
	return m_keys;
}

std::vector<int>& Action::getMouseButtons() {
	return m_mouseButtons;
}
std::string Action::getName() {
	return m_name;
}

Input::Input(Window* window)
	: m_window(window), m_actions(std::vector<bird::Action>()) {
	glfwSetCursorPosCallback(window->getGLFWWindow(), cursor_position_callback);
	glfwSetMouseButtonCallback(window->getGLFWWindow(), mouse_button_callback);
	glfwSetKeyCallback(window->getGLFWWindow(), key_callback);
	glfwSetCharCallback(window->getGLFWWindow(), character_callback);
	glfwSetCursorEnterCallback(window->getGLFWWindow(), cursor_enter_callback);
	glfwSetScrollCallback(window->getGLFWWindow(), scroll_callback);
	glfwSetJoystickCallback(joystick_callback);
}

void Input::addAction(Action action) {
	m_actions.push_back(action);
}

float Input::getActionValue(std::string name) {
	for (int i = 0; i < m_actions.size(); i++) {
		if (m_actions[i].getName() == name) {
			return m_actions[i].getValue();
		}
	}

	std::cerr << ("Action could not be found: " + std::string(name))
			  << std::endl;
	return 0;
}

void Input::tick() {
	glfwGetCursorPos(m_window->getGLFWWindow(), &m_mx, &m_my);

	// While the UI owns the keyboard (a focused text box) or the pointer
	// (hovering, dragging, modal), the game does not see that input.
	const bird::UIManager* ui = uiManager();
	const bool keysBlocked = ui && ui->wantsKeyboard();
	const bool mouseBlocked = ui && ui->wantsMouse();

	for (int i = 0; i < m_actions.size(); i++) {
		Action& a = m_actions[i];
		bool flag = true;
		for (int j = 0; j < a.getKeys().size() && !keysBlocked; j++) {
			int key = glfwGetKey(m_window->getGLFWWindow(), a.getKeys()[j]);
			if (key == GLFW_PRESS) {
				a.setValue(1.0f);
				flag = false;
				break;
			}
		}
		for (int j = 0; j < a.getMouseButtons().size() && !mouseBlocked; j++) {
			int mb = glfwGetMouseButton(m_window->getGLFWWindow(),
										a.getMouseButtons()[j]);
			if (mb == GLFW_PRESS) {
				a.setValue(1.0f);
				flag = false;
				break;
			}
		}
		// joystick
		// joystick buttons
		if (flag) {
			a.setValue(0);
		}
	}
}

void Input::setMouseMode(int cursorMode) {
	glfwSetInputMode(m_window->getGLFWWindow(), GLFW_CURSOR, cursorMode);
}
void Input::setMouseRawMotion(bool rawMotion) {
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(m_window->getGLFWWindow(), GLFW_RAW_MOUSE_MOTION,
						 rawMotion ? GLFW_TRUE : GLFW_FALSE);
	} else {
		std::cerr << "Raw Motion is not supported on this device" << std::endl;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action,
						   int mods) {
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	if (UIManager* ui = uiManager()) {
		ui->mouseButton(uiButton(button), action == GLFW_PRESS,
						Vector2(xpos, ypos), uiModifiers(mods));
	}
}
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	if (UIManager* ui = uiManager()) ui->mouseMove(Vector2(xpos, ypos));
}
void cursor_enter_callback(GLFWwindow* window, int entered) {
	if (entered) {
		// The cursor entered the content area of the window
	} else {
		// The cursor left the content area of the window
	}
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	if (UIManager* ui = uiManager()) {
		ui->scroll(Vector2(xpos, ypos), Vector2(xoffset, yoffset));
	}
}
void joystick_callback(int jid, int event) {
	if (event == GLFW_CONNECTED) {
	} else if (event == GLFW_DISCONNECTED) {
	}
}
void character_callback(GLFWwindow* window, unsigned int codepoint) {
	if (UIManager* ui = uiManager()) ui->character(codepoint);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action,
				  int mods) {
	if (UIManager* ui = uiManager()) {
		ui->key(uiKey(key), action != GLFW_RELEASE, action == GLFW_REPEAT,
				uiModifiers(mods), key);
	}
}

}  // namespace bird
