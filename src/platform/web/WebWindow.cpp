#include "WebWindow.h"

#include <emscripten.h>

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

#include "../../util/Input.h"
#include "../Platform.h"
#include "WebKeyMap.h"

namespace bird::platform {

namespace {

constexpr const char* kCanvas = "#canvas";

uint8_t modifierBits(bool shift, bool ctrl, bool alt, bool meta) {
	return static_cast<uint8_t>((shift ? 1 : 0) | (ctrl ? 2 : 0) |
								(alt ? 4 : 0) | (meta ? 8 : 0));
}

/** DOM buttons: 0 left, 1 middle, 2 right. */
int engineButton(unsigned short domButton) {
	switch (domButton) {
		case 1:
			return MOUSE_BUTTON_MIDDLE;
		case 2:
			return MOUSE_BUTTON_RIGHT;
		default:
			return domButton == 0 ? MOUSE_BUTTON_LEFT
								  : static_cast<int>(domButton);
	}
}

/**
 * Decodes a UTF-8 string holding exactly one code point (a printable
 * KeyboardEvent.key such as "a" or "é").
 * @return the code point, or 0 for names such as "Enter" or "Shift".
 */
uint32_t singleCodepoint(const char* text) {
	const auto* bytes = reinterpret_cast<const unsigned char*>(text);
	const size_t length = std::strlen(text);
	if (length == 0) return 0;
	uint32_t codepoint = 0;
	size_t expected = 0;
	if (bytes[0] < 0x80) {
		codepoint = bytes[0];
		expected = 1;
	} else if ((bytes[0] & 0xE0) == 0xC0) {
		codepoint = bytes[0] & 0x1F;
		expected = 2;
	} else if ((bytes[0] & 0xF0) == 0xE0) {
		codepoint = bytes[0] & 0x0F;
		expected = 3;
	} else if ((bytes[0] & 0xF8) == 0xF0) {
		codepoint = bytes[0] & 0x07;
		expected = 4;
	}
	if (expected == 0 || length != expected) return 0;
	for (size_t i = 1; i < expected; i++) {
		if ((bytes[i] & 0xC0) != 0x80) return 0;
		codepoint = (codepoint << 6) | (bytes[i] & 0x3F);
	}
	return codepoint >= 0x20 && codepoint != 0x7F ? codepoint : 0;
}

bool pointerLocked() {
	EmscriptenPointerlockChangeEvent status;
	return emscripten_get_pointerlock_status(&status) ==
			   EMSCRIPTEN_RESULT_SUCCESS &&
		   status.isActive;
}

}  // namespace

std::unique_ptr<Window> createWindow(uint32_t /*width*/, uint32_t /*height*/,
									 const std::string& /*title*/) {
	return std::make_unique<WebWindow>();
}

WebWindow::WebWindow() {
	EmscriptenWebGLContextAttributes attributes;
	emscripten_webgl_init_context_attributes(&attributes);
	attributes.majorVersion = 2;
	attributes.minorVersion = 0;
	attributes.alpha = false;
	attributes.depth = true;
	attributes.stencil = true;
	attributes.antialias = false;  // the engine resolves its own frames
	attributes.premultipliedAlpha = false;
	// Exposes EXT_color_buffer_float, which the HDR targets need.
	attributes.enableExtensionsByDefault = true;
	attributes.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
	m_context = emscripten_webgl_create_context(kCanvas, &attributes);
	if (m_context <= 0) {
		throw std::runtime_error(
			"WebWindow: this browser cannot create a WebGL 2 context");
	}
	emscripten_webgl_make_context_current(m_context);

	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this,
									true, onKey);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true,
								  onKey);
	emscripten_set_mousedown_callback(kCanvas, this, true, onMouseButton);
	// Releases outside the canvas still end a drag.
	emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this,
									true, onMouseButton);
	emscripten_set_mousemove_callback(kCanvas, this, true, onMouseMove);
	emscripten_set_wheel_callback(kCanvas, this, true, onWheel);
	emscripten_set_touchstart_callback(kCanvas, this, true, onTouch);
	emscripten_set_touchmove_callback(kCanvas, this, true, onTouch);
	emscripten_set_touchend_callback(kCanvas, this, true, onTouch);
	emscripten_set_touchcancel_callback(kCanvas, this, true, onTouch);
}

WebWindow::~WebWindow() {
	emscripten_html5_remove_all_event_listeners();
	emscripten_webgl_destroy_context(m_context);
}

bool WebWindow::pollResize() {
	double cssWidth = 0.0, cssHeight = 0.0;
	emscripten_get_element_css_size(kCanvas, &cssWidth, &cssHeight);
	const double ratio = emscripten_get_device_pixel_ratio();
	const int pixelWidth = static_cast<int>(std::lround(cssWidth * ratio));
	const int pixelHeight = static_cast<int>(std::lround(cssHeight * ratio));
	int bufferWidth = 0, bufferHeight = 0;
	emscripten_get_canvas_element_size(kCanvas, &bufferWidth, &bufferHeight);
	if (pixelWidth > 0 && pixelHeight > 0 &&
		(bufferWidth != pixelWidth || bufferHeight != pixelHeight)) {
		emscripten_set_canvas_element_size(kCanvas, pixelWidth, pixelHeight);
	}
	return updateSizes(pixelWidth, pixelHeight,
					   static_cast<int>(std::lround(cssWidth)),
					   static_cast<int>(std::lround(cssHeight)));
}

void WebWindow::setCursorMode(CursorMode mode) {
	m_cursorMode = mode;
	if (mode == CursorMode::Disabled) {
		// Deferred until the next click if not called from an input event.
		emscripten_request_pointerlock(kCanvas, true);
	} else if (pointerLocked()) {
		emscripten_exit_pointerlock();
	}
	setCursor(m_uiCursor);
}

void WebWindow::setCursor(UICursor cursor) {
	m_uiCursor = cursor;
	if (m_cursorMode != CursorMode::Normal) {
		applyCssCursor("none");
		return;
	}
	switch (cursor) {
		case UICursor::IBeam:
			applyCssCursor("text");
			break;
		case UICursor::Hand:
			applyCssCursor("pointer");
			break;
		case UICursor::ResizeHorizontal:
			applyCssCursor("ew-resize");
			break;
		case UICursor::ResizeVertical:
			applyCssCursor("ns-resize");
			break;
		case UICursor::ResizeDiagonal:
			applyCssCursor("nwse-resize");
			break;
		default:
			applyCssCursor("default");
			break;
	}
}

void WebWindow::applyCssCursor(const char* name) {
	// clang-format off
	EM_ASM({
		var canvas = document.getElementById('canvas');
		if (canvas) canvas.style.cursor = UTF8ToString($0);
	}, name);
	// clang-format on
}

void WebWindow::trackPointer(const EmscriptenMouseEvent& event) {
	if (pointerLocked()) {
		// The locked pointer stays put; accumulate its motion instead, like
		// a desktop window with a disabled cursor.
		m_cursorX += event.movementX;
		m_cursorY += event.movementY;
	} else {
		m_cursorX = event.targetX;
		m_cursorY = event.targetY;
	}
}

EM_BOOL WebWindow::onKey(int eventType, const EmscriptenKeyboardEvent* event,
						 void* self) {
	auto* window = static_cast<WebWindow*>(self);
	WindowEventListener* target = window->listener();
	const int key = engineKeyFromDomCode(event->code);
	if (target == nullptr) return false;
	const uint8_t modifiers = modifierBits(event->shiftKey, event->ctrlKey,
										   event->altKey, event->metaKey);
	if (key != KEY_UNKNOWN) {
		const InputAction action =
			eventType == EMSCRIPTEN_EVENT_KEYUP ? InputAction::Release
			: event->repeat						? InputAction::Repeat
												: InputAction::Press;
		target->onKey(key, action, modifiers);
	}
	const bool shortcut = event->ctrlKey || event->metaKey;
	if (eventType == EMSCRIPTEN_EVENT_KEYDOWN && !shortcut) {
		if (const uint32_t codepoint = singleCodepoint(event->key)) {
			target->onCharacter(codepoint);
		}
	}
	// Keep browser shortcuts (reload, dev tools, copy) and F-keys working;
	// swallow the rest so Space and arrows do not scroll the page.
	const bool functionKey = key >= KEY_F1 && key <= KEY_F12;
	return key != KEY_UNKNOWN && !shortcut && !functionKey;
}

EM_BOOL WebWindow::onMouseButton(int eventType,
								 const EmscriptenMouseEvent* event,
								 void* self) {
	auto* window = static_cast<WebWindow*>(self);
	WindowEventListener* target = window->listener();
	if (target == nullptr) return false;
	const bool pressed = eventType == EMSCRIPTEN_EVENT_MOUSEDOWN;
	if (pressed) {
		window->trackPointer(*event);
		target->onCursorMove(window->m_cursorX, window->m_cursorY);
		// A pending lock request needs this gesture.
		if (window->m_cursorMode == CursorMode::Disabled && !pointerLocked()) {
			emscripten_request_pointerlock(kCanvas, false);
		}
	}
	target->onMouseButton(
		engineButton(event->button),
		pressed ? InputAction::Press : InputAction::Release,
		modifierBits(event->shiftKey, event->ctrlKey, event->altKey,
					 event->metaKey));
	return pressed;	 // stop text selection while dragging on the canvas
}

EM_BOOL WebWindow::onMouseMove(int /*eventType*/,
							   const EmscriptenMouseEvent* event, void* self) {
	auto* window = static_cast<WebWindow*>(self);
	window->trackPointer(*event);
	if (WindowEventListener* target = window->listener()) {
		target->onCursorMove(window->m_cursorX, window->m_cursorY);
	}
	return false;
}

EM_BOOL WebWindow::onWheel(int /*eventType*/, const EmscriptenWheelEvent* event,
						   void* self) {
	auto* window = static_cast<WebWindow*>(self);
	WindowEventListener* target = window->listener();
	if (target == nullptr) return false;
	// Desktop wheels report about one unit per notch with +y meaning up;
	// DOM reports pixels (about 100 per notch), lines or pages, +y down.
	double scale = 1.0 / 100.0;
	if (event->deltaMode == DOM_DELTA_LINE) scale = 1.0 / 3.0;
	if (event->deltaMode == DOM_DELTA_PAGE) scale = 1.0;
	target->onScroll(-event->deltaX * scale, -event->deltaY * scale);
	return true;  // keep the page from scrolling
}

EM_BOOL WebWindow::onTouch(int eventType, const EmscriptenTouchEvent* event,
						   void* self) {
	auto* window = static_cast<WebWindow*>(self);
	WindowEventListener* target = window->listener();
	if (target == nullptr) return false;
	// The first finger down acts as the left mouse button until it lifts.
	for (int i = 0; i < event->numTouches; i++) {
		const EmscriptenTouchPoint& touch = event->touches[i];
		if (!touch.isChanged) continue;
		const double x = touch.targetX;
		const double y = touch.targetY;
		if (eventType == EMSCRIPTEN_EVENT_TOUCHSTART) {
			if (window->m_primaryTouch != -1) continue;
			window->m_primaryTouch = touch.identifier;
			window->m_cursorX = x;
			window->m_cursorY = y;
			target->onCursorMove(x, y);
			target->onMouseButton(MOUSE_BUTTON_LEFT, InputAction::Press, 0);
		} else if (touch.identifier == window->m_primaryTouch) {
			window->m_cursorX = x;
			window->m_cursorY = y;
			target->onCursorMove(x, y);
			if (eventType != EMSCRIPTEN_EVENT_TOUCHMOVE) {
				target->onMouseButton(MOUSE_BUTTON_LEFT, InputAction::Release,
									  0);
				window->m_primaryTouch = -1;
			}
		}
	}
	return true;  // no emulated mouse events, no page panning
}

}  // namespace bird::platform
