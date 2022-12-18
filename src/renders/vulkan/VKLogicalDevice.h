//
// Created by Seth Hahn on 11/7/22.
//

#ifndef BIRD_VKLOGICALDEVICE_H
#define BIRD_VKLOGICALDEVICE_H

#include "VKConstants.h"
#include "VKPhysicalDevice.h"

namespace bird::vk {

    class VKLogicalDevice {
    public:
        explicit VKLogicalDevice(VKPhysicalDevice* physicalDevice);
        ~VKLogicalDevice();

        void createLogicalDevice(VKPhysicalDevice* physicalDevice);

        [[nodiscard]] const VkDevice& getDevice() const;
        [[nodiscard]] const VkQueue& getGraphicsQueue() const;
        [[nodiscard]] const VkQueue& getPresentQueue() const;
    private:
        VkDevice m_device;
        VkQueue m_graphicsQueue;
        VkQueue m_presentQueue;

    };

} // bird

#endif //BIRD_VKLOGICALDEVICE_H
