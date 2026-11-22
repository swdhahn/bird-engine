//
// Created by Seth Hahn on 11/6/22.
//

#include "GlfwWindow.h"

#include "../../renders/opengl/GLApi.h"
// GLApi.h already declares GL; stop GLFW from adding a desktop GL header.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cctype>
#include <stdexcept>

#include "../../renders/GraphicsConstants.h"
#include "../../util/stb_image.h"

namespace bird::platform {

namespace {

/** GLFW and engine modifier bits share values (shift 1, ctrl 2, ...). */
uint8_t modifierBits(int mods) {
	return static_cast<uint8_t>(mods & 0x0F);
}

InputAction toAction(int glfwAction) {
	switch (glfwAction) {
		case GLFW_PRESS:
			return InputAction::Press;
		case GLFW_REPEAT:
			return InputAction::Repeat;
		default:
			return InputAction::Release;
	}
}

}  // namespace

GlfwWindow::GlfwWindow(uint32_t width, uint32_t height,
					   const std::string& title) {
	if (!glfwInit()) {
		throw std::runtime_error("GLFW could not be initialized!");
	}

	if (CURRENT_GRAPHICS_PIPELINE == GRAPHICS_PIPELINE_VULKAN) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_pWindow =
			glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
	} else if (CURRENT_GRAPHICS_PIPELINE == GRAPHICS_PIPELINE_OPENGL) {
#if defined(BIRD_GL_ES)
		// Desktop testing of the ES profile: an ES 3.0 context through EGL.
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
		glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
		m_pWindow =
			glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
		if (m_pWindow == nullptr) {
			glfwTerminate();
			throw std::runtime_error("GLFW could not create an OpenGL window");
		}
		glfwMakeContextCurrent(m_pWindow);
		int fwidth, fheight;
		glfwGetFramebufferSize(m_pWindow, &fwidth, &fheight);
		glViewport(0, 0, fwidth, fheight);
	} else {
		throw std::invalid_argument("Graphics Pipeline is not supported!");
	}
	int w, h, channels;
	unsigned char* pixels = stbi_load("logo.jpg", &w, &h, &channels, 4);

	if (pixels) {
		GLFWimage image = {w, h, pixels};
		glfwSetWindowIcon(m_pWindow, 1, &image);
		stbi_image_free(pixels);  // Free loaded data
	}
	glfwSwapInterval(0);

	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetKeyCallback(m_pWindow, onKey);
	glfwSetCharCallback(m_pWindow, onCharacter);
	glfwSetMouseButtonCallback(m_pWindow, onMouseButton);
	glfwSetCursorPosCallback(m_pWindow, onCursorMove);
	glfwSetScrollCallback(m_pWindow, onScroll);
}

bool GlfwWindow::shouldWindowClose() const {
	return glfwWindowShouldClose(m_pWindow);
}

void GlfwWindow::present() {
	glfwSwapBuffers(m_pWindow);
}

void GlfwWindow::pollEvents() {
	glfwPollEvents();
}

bool GlfwWindow::pollResize() {
	int framebufferWidth = 0, framebufferHeight = 0, width = 0, height = 0;
	glfwGetFramebufferSize(m_pWindow, &framebufferWidth, &framebufferHeight);
	glfwGetWindowSize(m_pWindow, &width, &height);
	return updateSizes(framebufferWidth, framebufferHeight, width, height);
}

void GlfwWindow::setCursorMode(CursorMode mode) {
	int value = GLFW_CURSOR_NORMAL;
	if (mode == CursorMode::Hidden) value = GLFW_CURSOR_HIDDEN;
	if (mode == CursorMode::Disabled) value = GLFW_CURSOR_DISABLED;
	glfwSetInputMode(m_pWindow, GLFW_CURSOR, value);
}

bool GlfwWindow::setRawMouseMotion(bool enabled) {
	if (!glfwRawMouseMotionSupported()) return false;
	glfwSetInputMode(m_pWindow, GLFW_RAW_MOUSE_MOTION,
					 enabled ? GLFW_TRUE : GLFW_FALSE);
	return true;
}

std::string GlfwWindow::getClipboardText() {
	const char* text = glfwGetClipboardString(m_pWindow);
	return text ? std::string(text) : std::string();
}

void GlfwWindow::setClipboardText(const std::string& text) {
	glfwSetClipboardString(m_pWindow, text.c_str());
}

void GlfwWindow::setCursor(UICursor cursor) {
	// GLFW 3.3 has no diagonal resize cursor; the crosshair stands in.
	static constexpr int kShapes[] = {GLFW_ARROW_CURSOR,	 GLFW_IBEAM_CURSOR,
									  GLFW_HAND_CURSOR,		 GLFW_HRESIZE_CURSOR,
									  GLFW_VRESIZE_CURSOR, GLFW_CROSSHAIR_CURSOR};
	const size_t index = static_cast<size_t>(cursor);
	if (index >= m_cursors.size()) return;
	if (!m_cursors[index]) {
		m_cursors[index] = glfwCreateStandardCursor(kShapes[index]);
	}
	glfwSetCursor(m_pWindow, m_cursors[index]);
}

std::string GlfwWindow::keyName(int keyCode) {
	// Printable keys follow the active keyboard layout.
	if (const char* name = glfwGetKeyName(keyCode, 0)) {
		std::string upper(name);
		for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		return upper;
	}
	if (keyCode >= GLFW_KEY_F1 && keyCode <= GLFW_KEY_F25) {
		return "F" + std::to_string(keyCode - GLFW_KEY_F1 + 1);
	}
	if (keyCode >= GLFW_KEY_KP_0 && keyCode <= GLFW_KEY_KP_9) {
		return "Numpad " + std::to_string(keyCode - GLFW_KEY_KP_0);
	}
	switch (keyCode) {
		case GLFW_KEY_SPACE: return "Space";
		case GLFW_KEY_ENTER: return "Enter";
		case GLFW_KEY_TAB: return "Tab";
		case GLFW_KEY_ESCAPE: return "Escape";
		case GLFW_KEY_BACKSPACE: return "Backspace";
		case GLFW_KEY_DELETE: return "Delete";
		case GLFW_KEY_INSERT: return "Insert";
		case GLFW_KEY_HOME: return "Home";
		case GLFW_KEY_END: return "End";
		case GLFW_KEY_PAGE_UP: return "Page Up";
		case GLFW_KEY_PAGE_DOWN: return "Page Down";
		case GLFW_KEY_LEFT: return "Left";
		case GLFW_KEY_RIGHT: return "Right";
		case GLFW_KEY_UP: return "Up";
		case GLFW_KEY_DOWN: return "Down";
		case GLFW_KEY_LEFT_SHIFT: return "Left Shift";
		case GLFW_KEY_RIGHT_SHIFT: return "Right Shift";
		case GLFW_KEY_LEFT_CONTROL: return "Left Ctrl";
		case GLFW_KEY_RIGHT_CONTROL: return "Right Ctrl";
		case GLFW_KEY_LEFT_ALT: return "Left Alt";
		case GLFW_KEY_RIGHT_ALT: return "Right Alt";
		case GLFW_KEY_LEFT_SUPER: return "Left Super";
		case GLFW_KEY_RIGHT_SUPER: return "Right Super";
		case GLFW_KEY_CAPS_LOCK: return "Caps Lock";
		case GLFW_KEY_KP_ENTER: return "Numpad Enter";
		default: return "Key " + std::to_string(keyCode);
	}
}

GlfwWindow* GlfwWindow::fromHandle(GLFWwindow* handle) {
	return static_cast<GlfwWindow*>(glfwGetWindowUserPointer(handle));
}

void GlfwWindow::onKey(GLFWwindow* handle, int key, int /*scancode*/,
					   int action, int mods) {
	if (WindowEventListener* target = fromHandle(handle)->listener()) {
		target->onKey(key, toAction(action), modifierBits(mods));
	}
}

void GlfwWindow::onCharacter(GLFWwindow* handle, unsigned int codepoint) {
	if (WindowEventListener* target = fromHandle(handle)->listener()) {
		target->onCharacter(codepoint);
	}
}

void GlfwWindow::onMouseButton(GLFWwindow* handle, int button, int action,
							   int mods) {
	if (WindowEventListener* target = fromHandle(handle)->listener()) {
		target->onMouseButton(button, toAction(action), modifierBits(mods));
	}
}

void GlfwWindow::onCursorMove(GLFWwindow* handle, double x, double y) {
	if (WindowEventListener* target = fromHandle(handle)->listener()) {
		target->onCursorMove(x, y);
	}
}

void GlfwWindow::onScroll(GLFWwindow* handle, double dx, double dy) {
	if (WindowEventListener* target = fromHandle(handle)->listener()) {
		target->onScroll(dx, dy);
	}
}

GlfwWindow::~GlfwWindow() {
	for (GLFWcursor* cursor : m_cursors) {
		if (cursor) glfwDestroyCursor(cursor);
	}
	glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}

}  // namespace bird::platform
