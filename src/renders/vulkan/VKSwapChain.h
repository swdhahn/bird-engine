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
        void createSwapChain(const VKPhysicalDevice* physicalDevice, const VKInstance* instance, const Window* window);

        void cleanUpSwapChain();
        void createImageViews();

        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
        static VkSurfaceFormatKHR chooseSwapChainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Window* window);

        const VKLogicalDevice* m_pLogicalDevice = nullptr;

        VkSwapchainKHR m_swapChain;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;
    };

} // bird


#endif //BIRD_VKSWAPCHAIN_H
