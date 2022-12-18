//
// Created by Seth Hahn on 11/6/22.
//

#include "VKSwapChain.h"

namespace bird::vk {

    VKSwapChain::VKSwapChain(const VKPhysicalDevice* physicalDevice, const VKLogicalDevice* device, const VKInstance* instance, const Window* window) : m_pLogicalDevice(device) {
        createSwapChain(physicalDevice, instance, window);
        createImageViews();
    }

    VKSwapChain::~VKSwapChain() {
        cleanUpSwapChain();
    }

    void VKSwapChain::createSwapChain(const VKPhysicalDevice* physicalDevice, const VKInstance* instance, const Window* window) {
        SwapChainSupportDetails swapChainSupportDetails = querySwapChainSupport(physicalDevice->getPhysicalDevice(), instance->getSurface());

        VkSurfaceFormatKHR surfaceFormat = chooseSwapChainFormat(swapChainSupportDetails.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupportDetails.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupportDetails.capabilities, window);

        uint32_t imageCount = swapChainSupportDetails.capabilities.minImageCount + 1;
        if(swapChainSupportDetails.capabilities.minImageCount > 0 && imageCount > swapChainSupportDetails.capabilities.maxImageCount) {
            imageCount = swapChainSupportDetails.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = instance->getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent;

        QueueFamilyIndices indices = physicalDevice->getQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if(indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupportDetails.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if(vkCreateSwapchainKHR(m_pLogicalDevice->getDevice(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Swap Chain!");
        }

        vkGetSwapchainImagesKHR(m_pLogicalDevice->getDevice(), m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_pLogicalDevice->getDevice(), m_swapChain, &imageCount, m_swapChainImages.data());
    }

    void VKSwapChain::createImageViews() {
        m_swapChainImageViews.resize(m_swapChainImages.size());

        for(size_t i = 0; i < m_swapChainImageViews.size(); i++) {
            m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }
    }

    VkImageView VKSwapChain::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(m_pLogicalDevice->getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    }

    void VKSwapChain::cleanUpSwapChain() {

        for(auto imageView : m_swapChainImageViews) {
            vkDestroyImageView(m_pLogicalDevice->getDevice(), imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_pLogicalDevice->getDevice(), m_swapChain, nullptr);
    }

    void VKSwapChain::recreateSwapChain() {

    }

    VkSurfaceFormatKHR VKSwapChain::chooseSwapChainFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for(const auto& format : availableFormats) {
            if(format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR VKSwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {

        for(const auto& present : availablePresentModes) {
            if(present == VK_PRESENT_MODE_MAILBOX_KHR) {
                return present;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VKSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, const Window* window) {
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(const_cast<GLFWwindow*>(window->getGLFWWindow()), &width, &height);

            VkExtent2D actualExtent = {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    SwapChainSupportDetails VKSwapChain::querySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if(formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, nullptr);
        if(presentCount != 0) {
            details.presentModes.resize(presentCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, details.presentModes.data());
        }

        return details;
    }

    const VkSwapchainKHR& VKSwapChain::getSwapChain() const {
        return m_swapChain;
    }

    const VkFormat& VKSwapChain::getSwapChainImageFormat() const {
        return m_swapChainImageFormat;
    }
    const VkExtent2D& VKSwapChain::getSwapChainExtent() const {
        return m_swapChainExtent;
    }

    const std::vector<VkImage>& VKSwapChain::getSwapChainImages() const {
        return m_swapChainImages;
    }

    const std::vector<VkImageView>& VKSwapChain::getSwapChainImageViews() const {
        return m_swapChainImageViews;
    }

} // bird
