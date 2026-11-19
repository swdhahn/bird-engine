//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_VKPIPELINE_H
#define BIRD_VKPIPELINE_H

#include <vector>
#include <memory>
#include <string>
#include <volk.h>

#include "../GraphicsPipeline.h"
#include "VKConstants.h"
#include "VKPhysicalDevice.h"
#include "VKInstance.h"
#include "VKLogicalDevice.h"
#include "VKSwapChain.h"
#include "VKRenderPass.h"

namespace bird::vk {

    /**
     * Vulkan graphics backend pipeline.
     *
     * Coordinates instance lifecycle, physical device picking, logical device creation,
     * presentation swapchain management, render pass setup, and command recording.
     */
    class VKPipeline : public GraphicsPipeline {
    public:
        VKPipeline();

        /** Initializes Vulkan Volk loader, instance, device, swapchain, and passes. */
        void init() override;

        /** Waits for GPU idle and destroys Vulkan objects. */
        void cleanUp() override;

        /** Submits recorded draw commands for the active scene. */
        void renderRootScene(const Scene* scene) override;

        [[nodiscard]] std::string getName() const override;

        static const std::vector<const char*> s_validationLayers;
        static const std::vector<const char*> s_deviceExtensions;

        static inline VkSampleCountFlagBits s_msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    private:
        std::unique_ptr<VKInstance> m_pInstance = nullptr;
        std::unique_ptr<VKPhysicalDevice> m_pPhysicalDevice = nullptr;
        std::unique_ptr<VKLogicalDevice> m_pLogicalDevice = nullptr;
        std::unique_ptr<VKSwapChain> m_pSwapChain = nullptr;
        std::unique_ptr<VKRenderPass> m_pRenderPass = nullptr;
    };

} // namespace bird::vk

#endif //BIRD_VKPIPELINE_H
