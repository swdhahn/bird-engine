//
// Created by Seth Hahn on 11/7/22.
//

#include "VKInstance.h"
#include "VKPipeline.h"

namespace bird::vk {

    VKInstance::VKInstance(std::string applicationName, std::string engineName, Window* pWindow) {
        createInstance(applicationName, engineName);
        createSurface(pWindow);
    }

    VKInstance::~VKInstance() {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }

    void VKInstance::createInstance(std::string applicationName, std::string engineName) {
        VkApplicationInfo appInfo {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_3;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pApplicationName = applicationName.c_str();
        appInfo.pEngineName = engineName.c_str();

        VkInstanceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions;
        for(int i = 0; i < glfwExtensionCount; i++) {
            extensions.emplace_back(glfwExtensions[i]);
        }
        extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        if(enableValidationLayers) {
            extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();

        if(enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(VKPipeline::s_validationLayers.size());
            createInfo.ppEnabledLayerNames = VKPipeline::s_validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        if(vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan Instance!");
        }

        volkLoadInstance(m_instance); // loads all vulkan entrance points in volk

    }

    void VKInstance::createSurface(Window* pWindow) {
        if(glfwCreateWindowSurface(m_instance, const_cast<GLFWwindow*>(pWindow->getGLFWWindow()), nullptr, &m_surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan Window Surface!");
        }
    }

    const VkInstance& VKInstance::getInstance() const {
        return m_instance;
    }

    const VkSurfaceKHR& VKInstance::getSurface() const {
        return m_surface;
    }

} // bird