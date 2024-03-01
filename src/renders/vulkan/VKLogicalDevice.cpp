//
// Created by Seth Hahn on 11/7/22.
//

#include "VKLogicalDevice.h"
#include "VKPipeline.h"

namespace bird::vk {

    VKLogicalDevice::VKLogicalDevice(VKPhysicalDevice* physicalDevice) {
        createLogicalDevice(physicalDevice);
    }

    VKLogicalDevice::~VKLogicalDevice() {
        vkDestroyDevice(m_device, nullptr);
    }

    void VKLogicalDevice::createLogicalDevice(VKPhysicalDevice* physicalDevice) {
        QueueFamilyIndices indices = physicalDevice->getQueueFamilyIndices();
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0;
        for(uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(VKPipeline::s_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = VKPipeline::s_deviceExtensions.data();
        if(enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(VKPipeline::s_validationLayers.size());
            createInfo.ppEnabledLayerNames = VKPipeline::s_validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }
        if(vkCreateDevice(physicalDevice->getPhysicalDevice(), &createInfo, nullptr, &m_device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Logical device!");
        }

        vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);

        volkLoadDevice(m_device); // loads the device into volk

    }

    const VkDevice& VKLogicalDevice::getDevice() const {
        return m_device;
    }

    const VkQueue& VKLogicalDevice::getGraphicsQueue() const {
        return m_graphicsQueue;
    }

    const VkQueue& VKLogicalDevice::getPresentQueue() const {
        return m_presentQueue;
    }

} // bird