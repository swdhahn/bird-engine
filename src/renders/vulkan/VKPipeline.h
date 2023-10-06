//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_VKPIPELINE_H
#define BIRD_VKPIPELINE_H

#include "../GraphicsPipeline.h"
#include "VKConstants.h"
#include "VKPhysicalDevice.h"
#include "VKInstance.h"
#include "VKLogicalDevice.h"
#include "VKSwapChain.h"
#include "VKRenderPass.h"

namespace bird::vk {

    class VKPipeline : public GraphicsPipeline {
    public:
        VKPipeline();

        void init() override;
        void cleanUp() override;

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

} // bird

#endif //BIRD_VKPIPELINE_H
