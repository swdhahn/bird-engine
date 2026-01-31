//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_VKSWAPCHAIN_H
#define BIRD_VKSWAPCHAIN_H

#include "VKConstants.h"
#include "VKPhysicalDevice.h"
#include "VKLogicalDevice.h"
#include "VKInstance.h"

namespace bird::vk {

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class VKSwapChain {
    public:
        VKSwapChain(const VKPhysicalDevice* physicalDevice, const VKLogicalDevice* device, const VKInstance* instance, const Window* window);
        ~VKSwapChain();

        void recreateSwapChain();

        static SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);

        const VkSwapchainKHR& getSwapChain() const;
        const VkFormat& getSwapChainImageFormat() const;
        const VkExtent2D& getSwapChainExtent() const;
        const std::vector<VkImage>& getSwapChainImages() const;
        const std::vector<VkImageView>& getSwapChainImageViews() const;

    private:
        void createSwapChain(const VKInstance* instance);
        void recreateSwapChain(const VKInstance* instance);

        void cleanUpSwapChain();
        void createImageViews();

        void createRenderPass();
        void createFramebuffers();
        void createColorResources();
        void createDepthResources();

        static VkSurfaceFormatKHR chooseSwapChainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Window* window);

        /** Probs gonna have to move these to different file */
        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                            VkFormatFeatureFlags features);
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                         VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image,
                         VkDeviceMemory &imageMemory);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

        /***/

        const VKLogicalDevice* m_pLogicalDevice = nullptr;
        const VKPhysicalDevice* m_pPhysicalDevice = nullptr;
        const Window* m_pWindow = nullptr;


        VkSwapchainKHR m_swapChain;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;
        VkRenderPass m_renderPass;
        VkImageView m_colorImageView;
        VkImageView m_depthImageView;
        VkImage m_colorImage;
        VkImage m_depthImage;
        VkDeviceMemory m_colorImageMemory;
        VkDeviceMemory m_depthImageMemory;

        std::vector<VkFramebuffer> m_swapChainFramebuffers;
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;
    };

} // bird


#endif //BIRD_VKSWAPCHAIN_H
