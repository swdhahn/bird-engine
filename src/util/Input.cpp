
// Created by student on 10/5/23.
//

#include "Input.h"

#include <GLFW/glfw3.h>

#include "../Application.h"
#include "../scene/2D/UIManager.h"

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

	for (int i = 0; i < m_actions.size(); i++) {
		Action& a = m_actions[i];
		bool flag = true;
		for (int j = 0; j < a.getKeys().size(); j++) {
			int key = glfwGetKey(m_window->getGLFWWindow(), a.getKeys()[j]);
			if (key == GLFW_PRESS) {
				a.setValue(1.0f);
				flag = false;
				break;
			}
		}
		for (int j = 0; j < a.getMouseButtons().size(); j++) {
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
	APPLICATION->getUIManager()->processMouseUI(xpos, ypos, action);
}
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	APPLICATION->getUIManager()->processMouseUI(xpos, ypos, ACTION_NONE);
}
void cursor_enter_callback(GLFWwindow* window, int entered) {
	if (entered) {
		// The cursor entered the content area of the window
	} else {
		// The cursor left the content area of the window
	}
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {}
void joystick_callback(int jid, int event) {
	if (event == GLFW_CONNECTED) {
	} else if (event == GLFW_DISCONNECTED) {
	}
}
void character_callback(GLFWwindow* window, unsigned int codepoint) {}
void key_callback(GLFWwindow* window, int key, int scancode, int action,
				  int mods) {
	if (key == GLFW_KEY_E && action == GLFW_PRESS) {
	}
}

}  // namespace bird
