//
// Created by Seth Hahn on 11/6/22.
//

#include "Window.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

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

Window::~Window() {
	glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}

}  // namespace bird
