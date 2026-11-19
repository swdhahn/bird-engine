//
// Created by Seth Hahn on 11/6/22.
//

#include "Window.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cctype>

#include "../util/stb_image.h"
#include "GraphicsPipeline.h"

namespace bird {

Window::Window(uint32_t width, uint32_t height, std::string title) {
	if (!glfwInit()) {
		throw std::runtime_error("GLFW could not be initialized!");
	}

	if (CURRENT_GRAPHICS_PIPELINE == GRAPHICS_PIPELINE_VULKAN) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_pWindow =
			glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
	} else if (CURRENT_GRAPHICS_PIPELINE == GRAPHICS_PIPELINE_OPENGL) {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		m_pWindow =
			glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
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
}

bool Window::shouldWindowClose() const {
	return glfwWindowShouldClose(m_pWindow);
}

void Window::pollWindow() const {
	glfwSwapBuffers(m_pWindow);
	glfwPollEvents();
}

double Window::getGLFWTime() {
	return glfwGetTime();
}

GLFWwindow* const Window::getGLFWWindow() const {
	return m_pWindow;
}

bool Window::pollResize() {
	int framebufferWidth = 0, framebufferHeight = 0, width = 0, height = 0;
	glfwGetFramebufferSize(m_pWindow, &framebufferWidth, &framebufferHeight);
	glfwGetWindowSize(m_pWindow, &width, &height);
	// A minimized window reports zero; keep the last usable size.
	if (framebufferWidth <= 0 || framebufferHeight <= 0 || width <= 0 ||
		height <= 0) {
		return false;
	}
	const bool changed = static_cast<uint32_t>(framebufferWidth) != m_framebufferWidth ||
						 static_cast<uint32_t>(framebufferHeight) != m_framebufferHeight ||
						 static_cast<uint32_t>(width) != m_width ||
						 static_cast<uint32_t>(height) != m_height;
	m_framebufferWidth = framebufferWidth;
	m_framebufferHeight = framebufferHeight;
	m_width = width;
	m_height = height;
	return changed;
}

uint32_t Window::getFramebufferWidth() const {
	return m_framebufferWidth;
}
uint32_t Window::getFramebufferHeight() const {
	return m_framebufferHeight;
}
uint32_t Window::getWidth() const {
	return m_width;
}
uint32_t Window::getHeight() const {
	return m_height;
}

std::string Window::getClipboardText() {
	const char* text = glfwGetClipboardString(m_pWindow);
	return text ? std::string(text) : std::string();
}

void Window::setClipboardText(const std::string& text) {
	glfwSetClipboardString(m_pWindow, text.c_str());
}

void Window::setCursor(UICursor cursor) {
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

std::string Window::keyName(int keyCode) {
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

Window::~Window() {
	for (GLFWcursor* cursor : m_cursors) {
		if (cursor) glfwDestroyCursor(cursor);
	}
	glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}

}  // namespace bird
