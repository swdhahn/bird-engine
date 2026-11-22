#include "WebKeyMap.h"

#include <unordered_map>

#include "../../util/Input.h"

namespace bird::platform {

namespace {

std::unordered_map<std::string, int> buildTable() {
	std::unordered_map<std::string, int> table = {
		{"Space", KEY_SPACE},
		{"Quote", KEY_APOSTROPHE},
		{"Comma", KEY_COMMA},
		{"Minus", KEY_MINUS},
		{"Period", KEY_PERIOD},
		{"Slash", KEY_SLASH},
		{"Semicolon", KEY_SEMICOLON},
		{"Equal", KEY_EQUAL},
		{"BracketLeft", KEY_LEFT_BRACKET},
		{"Backslash", KEY_BACKSLASH},
		{"BracketRight", KEY_RIGHT_BRACKET},
		{"Backquote", KEY_GRAVE_ACCENT},
		{"Escape", KEY_ESCAPE},
		{"Enter", KEY_ENTER},
		{"Tab", KEY_TAB},
		{"Backspace", KEY_BACKSPACE},
		{"Insert", KEY_INSERT},
		{"Delete", KEY_DELETE},
		{"ArrowRight", KEY_RIGHT},
		{"ArrowLeft", KEY_LEFT},
		{"ArrowDown", KEY_DOWN},
		{"ArrowUp", KEY_UP},
		{"PageUp", KEY_PAGE_UP},
		{"PageDown", KEY_PAGE_DOWN},
		{"Home", KEY_HOME},
		{"End", KEY_END},
		{"CapsLock", KEY_CAPS_LOCK},
		{"ScrollLock", KEY_SCROLL_LOCK},
		{"NumLock", KEY_NUM_LOCK},
		{"PrintScreen", KEY_PRINT_SCREEN},
		{"Pause", KEY_PAUSE},
		{"NumpadDecimal", KEY_KP_DECIMAL},
		{"NumpadDivide", KEY_KP_DIVIDE},
		{"NumpadMultiply", KEY_KP_MULTIPLY},
		{"NumpadSubtract", KEY_KP_SUBTRACT},
		{"NumpadAdd", KEY_KP_ADD},
		{"NumpadEnter", KEY_KP_ENTER},
		{"NumpadEqual", KEY_KP_EQUAL},
		{"ShiftLeft", KEY_LEFT_SHIFT},
		{"ControlLeft", KEY_LEFT_CONTROL},
		{"AltLeft", KEY_LEFT_ALT},
		{"MetaLeft", KEY_LEFT_SUPER},
		{"ShiftRight", KEY_RIGHT_SHIFT},
		{"ControlRight", KEY_RIGHT_CONTROL},
		{"AltRight", KEY_RIGHT_ALT},
		{"MetaRight", KEY_RIGHT_SUPER},
		{"ContextMenu", KEY_MENU},
	};
	for (char letter = 'A'; letter <= 'Z'; letter++) {
		table[std::string("Key") + letter] = KEY_A + (letter - 'A');
	}
	for (char digit = '0'; digit <= '9'; digit++) {
		table[std::string("Digit") + digit] = KEY_0 + (digit - '0');
		table[std::string("Numpad") + digit] = KEY_KP_0 + (digit - '0');
	}
	for (int f = 1; f <= 12; f++) {
		table["F" + std::to_string(f)] = KEY_F1 + (f - 1);
	}
	return table;
}

}  // namespace

int engineKeyFromDomCode(const std::string& code) {
	static const std::unordered_map<std::string, int> table = buildTable();
	const auto found = table.find(code);
	return found == table.end() ? KEY_UNKNOWN : found->second;
}

}  // namespace bird::platform
