//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_VKPHYSICALDEVICE_H
#define BIRD_VKPHYSICALDEVICE_H

#include <optional>
#include <cstdint>
#include <volk.h>

#include "VKConstants.h"
#include "VKInstance.h"

namespace bird::vk {

    /**
     * Stores queue family indices discovered on a physical GPU device.
     */
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        /** Returns true when both graphics and presentation queues are supported. */
        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    /**
     * Queries and selects a suitable physical GPU device meeting rendering requirements.
     */
    class VKPhysicalDevice {
    public:
        explicit VKPhysicalDevice(const VKInstance* instance);

        /** Enumerates physical devices and selects the highest performing discrete GPU. */
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

} // namespace bird::vk

#endif //BIRD_VKPHYSICALDEVICE_H
