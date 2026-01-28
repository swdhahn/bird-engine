//
// Created by Seth Hahn on 11/6/22.
//

#include "VKPhysicalDevice.h"

#include "VKPipeline.h"
#include "VKSwapChain.h"

namespace bird::vk {

VKPhysicalDevice::VKPhysicalDevice(const VKInstance* pInstance) {
	pickPhysicalDevice(pInstance);
}

void VKPhysicalDevice::pickPhysicalDevice(const VKInstance* pInstance) {
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(pInstance->getInstance(), &deviceCount, nullptr);
	if (deviceCount == 0)
		throw std::runtime_error("Failed to GPUs with Vulkan support!");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(pInstance->getInstance(), &deviceCount,
							   devices.data());

	for (const auto& device : devices) {
		QueueFamilyIndices indices =
			findQueueFamilies(device, pInstance->getSurface());
		if (isDeviceSuitable(device, pInstance->getSurface(), indices)) {
			m_indices = indices;
			m_physicalDevice = device;
			VKPipeline::s_msaaSamples = getMaxUsableSampleCount();
			break;
		}
	}

	// device suitability possibly?!?!
	if (m_physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("Failed to find a suitable GPU!");
}

VkSampleCountFlagBits VKPhysicalDevice::getMaxUsableSampleCount() {
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts =
		physicalDeviceProperties.limits.framebufferColorSampleCounts &
		physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) {
		return VK_SAMPLE_COUNT_64_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_32_BIT) {
		return VK_SAMPLE_COUNT_32_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_16_BIT) {
		return VK_SAMPLE_COUNT_16_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_8_BIT) {
		return VK_SAMPLE_COUNT_8_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_4_BIT) {
		return VK_SAMPLE_COUNT_4_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_2_BIT) {
		return VK_SAMPLE_COUNT_2_BIT;
	}

	return VK_SAMPLE_COUNT_1_BIT;
}

bool VKPhysicalDevice::isDeviceSuitable(VkPhysicalDevice device,
										const VkSurfaceKHR& surface,
										QueueFamilyIndices& indices) {
	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainAdequate;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupportDetails =
			VKSwapChain::querySwapChainSupport(device, surface);
		swapChainAdequate = !swapChainSupportDetails.formats.empty() &&
							!swapChainSupportDetails.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.isComplete() && extensionsSupported && swapChainAdequate &&
		   supportedFeatures.samplerAnisotropy;
}

QueueFamilyIndices VKPhysicalDevice::findQueueFamilies(
	VkPhysicalDevice device, const VkSurfaceKHR& surface) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
											 nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
											 queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
											 &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}
		if (indices.isComplete()) break;
		i++;
	}

	return indices;
}

bool VKPhysicalDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
										 nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
										 availableExtensions.data());

	std::set<std::string> requiredExtensions(
		VKPipeline::s_deviceExtensions.begin(),
		VKPipeline::s_deviceExtensions.end());
	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

const QueueFamilyIndices& VKPhysicalDevice::getQueueFamilyIndices() const {
	return m_indices;
}

const VkPhysicalDevice& VKPhysicalDevice::getPhysicalDevice() const {
	return m_physicalDevice;
}

}  // namespace bird::vk
