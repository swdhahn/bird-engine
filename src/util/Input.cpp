
// Created by student on 10/5/23.
//

#include "Input.h"

#include "../Application.h"
#include "../scene/2D/UIManager.h"

namespace {

bird::UIManager* uiManager() {
	return bird::APPLICATION ? bird::APPLICATION->getUIManager() : nullptr;
}

bird::UIKey uiKey(int key) {
	using bird::UIKey;
	switch (key) {
		case KEY_BACKSPACE: return UIKey::Backspace;
		case KEY_DELETE: return UIKey::Delete;
		case KEY_LEFT: return UIKey::Left;
		case KEY_RIGHT: return UIKey::Right;
		case KEY_UP: return UIKey::Up;
		case KEY_DOWN: return UIKey::Down;
		case KEY_HOME: return UIKey::Home;
		case KEY_END: return UIKey::End;
		case KEY_PAGE_UP: return UIKey::PageUp;
		case KEY_PAGE_DOWN: return UIKey::PageDown;
		case KEY_ENTER:
		case KEY_KP_ENTER: return UIKey::Enter;
		case KEY_ESCAPE: return UIKey::Escape;
		case KEY_TAB: return UIKey::Tab;
		case KEY_SPACE: return UIKey::Space;
		case KEY_A: return UIKey::A;
		case KEY_C: return UIKey::C;
		case KEY_V: return UIKey::V;
		case KEY_X: return UIKey::X;
		default: return UIKey::Unknown;
	}
}

bird::UIMouseButton uiButton(int button) {
	switch (button) {
		case MOUSE_BUTTON_RIGHT: return bird::UIMouseButton::Right;
		case MOUSE_BUTTON_MIDDLE: return bird::UIMouseButton::Middle;
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

Input::Input(Window* window) : m_window(window) {
	m_window->setEventListener(this);
}

Input::~Input() {
	m_window->setEventListener(nullptr);
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

bool Input::isKeyDown(int key) const {
	return key >= 0 && key < kKeyCount && m_keysDown.test(key);
}

bool Input::isMouseButtonDown(int button) const {
	return button >= 0 && button < kMouseButtonCount &&
		   m_buttonsDown.test(button);
}

void Input::tick() {
	m_mouseDelta = m_resetDelta ? Vector2(0.0f)
								: Vector2(float(m_mx - m_tickX),
										  float(m_my - m_tickY));
	m_tickX = m_mx;
	m_tickY = m_my;
	m_resetDelta = false;

	// While the UI owns the keyboard (a focused text box) or the pointer
	// (hovering, dragging, modal), the game does not see that input. A
	// captured mouse always belongs to the game.
	const bird::UIManager* ui = uiManager();
	const bool keysBlocked = ui && ui->wantsKeyboard();
	const bool mouseBlocked = !m_captured && ui && ui->wantsMouse();

	for (Action& action : m_actions) {
		bool active = false;
		if (!keysBlocked) {
			for (int key : action.getKeys()) {
				if (isKeyDown(key)) {
					active = true;
					break;
				}
			}
		}
		if (!active && !mouseBlocked) {
			for (int button : action.getMouseButtons()) {
				if (isMouseButtonDown(button)) {
					active = true;
					break;
				}
			}
		}
		action.setValue(active ? 1.0f : 0.0f);
	}
}

void Input::setMouseMode(int cursorMode) {
	switch (cursorMode) {
		case CURSOR_HIDDEN:
			m_window->setCursorMode(CursorMode::Hidden);
			break;
		case CURSOR_DISABLED:
			m_window->setCursorMode(CursorMode::Disabled);
			break;
		default:
			m_window->setCursorMode(CursorMode::Normal);
			break;
	}
}

void Input::setMouseCaptured(bool captured) {
	if (captured == m_captured) return;
	m_captured = captured;
	m_resetDelta = true;
	m_window->setCursorMode(captured ? CursorMode::Disabled
									 : CursorMode::Normal);
	m_window->setRawMouseMotion(captured);
	if (captured) {
		if (UIManager* ui = uiManager()) ui->setFocus(nullptr);
	}
}

void Input::setMouseRawMotion(bool rawMotion) {
	if (!m_window->setRawMouseMotion(rawMotion)) {
		std::cerr << "Raw Motion is not supported on this device" << std::endl;
	}
}

void Input::onKey(int key, InputAction action, uint8_t modifiers) {
	if (key >= 0 && key < kKeyCount) {
		m_keysDown.set(key, action != InputAction::Release);
	}
	if (UIManager* ui = uiManager()) {
		ui->key(uiKey(key), action != InputAction::Release,
				action == InputAction::Repeat, modifiers, key);
	}
}

void Input::onCharacter(uint32_t codepoint) {
	if (UIManager* ui = uiManager()) ui->character(codepoint);
}

void Input::onMouseButton(int button, InputAction action, uint8_t modifiers) {
	if (button >= 0 && button < kMouseButtonCount) {
		m_buttonsDown.set(button, action != InputAction::Release);
	}
	if (m_captured) return;	 // a captured pointer belongs to the game
	if (UIManager* ui = uiManager()) {
		ui->mouseButton(uiButton(button), action == InputAction::Press,
						Vector2(m_mx, m_my), modifiers);
	}
}

void Input::onCursorMove(double x, double y) {
	m_mx = x;
	m_my = y;
	if (m_captured) return;
	if (UIManager* ui = uiManager()) ui->mouseMove(Vector2(x, y));
}

void Input::onScroll(double dx, double dy) {
	if (m_captured) return;
	if (UIManager* ui = uiManager()) {
		ui->scroll(Vector2(m_mx, m_my), Vector2(dx, dy));
	}
}

}  // namespace bird
