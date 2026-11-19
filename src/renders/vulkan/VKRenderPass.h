//
// Created by Seth Hahn on 11/7/22.
//

#ifndef BIRD_VKRENDERPASS_H
#define BIRD_VKRENDERPASS_H

#include <vector>
#include <volk.h>

#include "VKConstants.h"
#include "VKSwapChain.h"

namespace bird::vk {

    /**
     * Manages a Vulkan VkRenderPass object describing color and depth attachments.
     */
    class VKRenderPass {
    public:
        VKRenderPass(const VKPhysicalDevice* pPhysicalDevice, const VKLogicalDevice* pDevice, const VKSwapChain* pSwapChain);
        ~VKRenderPass();

        const VkRenderPass& getRenderPass() const;
    private:
        void createRenderPass();

        VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        VkFormat findDepthFormat();

        VkRenderPass m_renderPass;

        const VKSwapChain* m_pSwapChain;
        const VKLogicalDevice* m_pDevice;
        const VKPhysicalDevice* m_pPhysicalDevice;
    };

} // namespace bird::vk

#endif //BIRD_VKRENDERPASS_H
