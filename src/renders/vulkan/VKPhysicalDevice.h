//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_VKPHYSICALDEVICE_H
#define BIRD_VKPHYSICALDEVICE_H

#include "VKConstants.h"
#include "VKInstance.h"

namespace bird::vk {

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    class VKPhysicalDevice {
    public:
        explicit VKPhysicalDevice(const VKInstance* instance);

        void pickPhysicalDevice(const VKInstance* instance);

        [[nodiscard]] const VkPhysicalDevice& getPhysicalDevice() const;
        [[nodiscard]] const QueueFamilyIndices& getQueueFamilyIndices() const;

    private:
        static bool isDeviceSuitable(VkPhysicalDevice device, const VkSurfaceKHR& surface, QueueFamilyIndices& indices);
        static bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, const VkSurfaceKHR& surface);
        VkSampleCountFlagBits getMaxUsableSampleCount();

        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        QueueFamilyIndices m_indices;

    };

} // bird

#endif //BIRD_VKPHYSICALDEVICE_H
