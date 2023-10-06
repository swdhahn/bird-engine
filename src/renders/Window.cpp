//
// Created by Seth Hahn on 11/6/22.
//

#include "Window.h"
#include "GraphicsPipeline.h"

namespace bird {

    Window::Window(uint32_t width, uint32_t height, std::string title) {
        if(!glfwInit()) {
            throw std::runtime_error("GLFW could not be initialized!");
        }

        if(GraphicsPipeline::getGraphicsPipelineType() == GRAPHICS_PIPELINE_VULKAN) {
#ifdef _VULKAN_DEFINED
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            m_pWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
#else
            throw std::runtime_error("Vulkan does not exist!");
#endif
        } else if(GraphicsPipeline::getGraphicsPipelineType() == GRAPHICS_PIPELINE_OPENGL) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            m_pWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

            glfwMakeContextCurrent(m_pWindow);
            int fwidth, fheight;
            glfwGetFramebufferSize(m_pWindow, &fwidth, &fheight);
            glViewport(0, 0, fwidth, fheight);
        } else {
            throw std::invalid_argument("Graphics Pipeline is not supported!");
        }
    }

    bool Window::shouldWindowClose() const {
        return glfwWindowShouldClose(m_pWindow);
    }

    void Window::pollWindow() const {
        glfwPollEvents();
    }

    GLFWwindow* const Window::getGLFWWindow() const {
        return m_pWindow;
    }

    Window::~Window() {
        glfwDestroyWindow(m_pWindow);
        glfwTerminate();
    }

} // bird