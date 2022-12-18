//
// Created by Seth Hahn on 11/6/22.
//

#include "VKPipeline.h"

#include <memory>

namespace bird::vk {

    const std::vector<const char*>VKPipeline::s_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*>VKPipeline::s_deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
            // kept getting annoying error message on macOS M1 Chip unless this was added!
            "VK_KHR_portability_subset",
#endif
    };

    VKPipeline::VKPipeline() : GraphicsPipeline(GRAPHICS_PIPELINE_VULKAN) {

    }

    void VKPipeline::init() {
        m_pWindow = std::make_unique<Window>(1280, 720, "Engine!");
        m_pInstance = std::make_unique<VKInstance>("Vulkan App", "Bird Engine", m_pWindow.get());
        m_pPhysicalDevice = std::make_unique<VKPhysicalDevice>(m_pInstance.get());
        m_pLogicalDevice =  std::make_unique<VKLogicalDevice>(m_pPhysicalDevice.get());
        m_pSwapChain = std::make_unique<VKSwapChain>(m_pPhysicalDevice.get(), m_pLogicalDevice.get(), m_pInstance.get(), m_pWindow.get());
        m_pRenderPass = std::make_unique<VKRenderPass>(m_pPhysicalDevice.get(), m_pLogicalDevice.get(), m_pSwapChain.get());

        // cleanUpSwapChain() function in SwapChain

        VkPhysicalDeviceProperties pProperties {};
        vkGetPhysicalDeviceProperties(m_pPhysicalDevice->getPhysicalDevice(), &pProperties);

        std::cout << pProperties.limits.maxImageDimension2D << "  " << m_pSwapChain->getSwapChainImageFormat() << std::endl;
    }

    void VKPipeline::renderScene(const bird::Scene *scene) {

    }

    void VKPipeline::cleanUp() {
        m_pRenderPass.reset();
        m_pSwapChain.reset();
        m_pLogicalDevice.reset();
        m_pPhysicalDevice.reset();
        m_pInstance.reset();
    }

    std::string VKPipeline::getName() const {
        return "Vulkan Pipeline";
    }

} // bird