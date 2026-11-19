//
// Created by Seth Hahn on 11/7/22.
//

#ifndef BIRD_VKLOGICALDEVICE_H
#define BIRD_VKLOGICALDEVICE_H

#include <volk.h>

#include "VKConstants.h"
#include "VKPhysicalDevice.h"

namespace bird::vk {

/**
 * Vulkan logical device interface.
 *
 * Manages device-level resource creation and exposes command and presentation
 * queues (VkQueue).
 */
class VKLogicalDevice {
   public:
	explicit VKLogicalDevice(VKPhysicalDevice* physicalDevice);
	~VKLogicalDevice();

	/** Instantiates VkDevice and retrieves queue handles for graphics and presentation. */
	void createLogicalDevice(VKPhysicalDevice* physicalDevice);

	[[nodiscard]] const VkDevice& getDevice() const;
	[[nodiscard]] const VkQueue& getGraphicsQueue() const;
	[[nodiscard]] const VkQueue& getPresentQueue() const;

   private:
	VkDevice m_device;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
};

}  // namespace bird::vk

#endif	// BIRD_VKLOGICALDEVICE_H
