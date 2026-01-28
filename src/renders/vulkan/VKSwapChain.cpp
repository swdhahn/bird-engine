//
// Created by Seth Hahn on 11/6/22.
//

#include "VKSwapChain.h"

#include <GLFW/glfw3.h>
#include <volk.h>

#include "VKPipeline.h"

namespace bird::vk {

VKSwapChain::VKSwapChain(const VKPhysicalDevice* physicalDevice,
						 const VKLogicalDevice* device,
						 const VKInstance* instance, const Window* window)
	: m_pLogicalDevice(device),
	  m_pPhysicalDevice(physicalDevice),
	  m_pWindow(window) {
	createSwapChain(instance);
	createImageViews();
	createRenderPass();

	createColorResources();
	createDepthResources();
	createFramebuffers();
}

VKSwapChain::~VKSwapChain() {
	cleanUpSwapChain();
}

void VKSwapChain::createSwapChain(const VKInstance* instance) {
	SwapChainSupportDetails swapChainSupportDetails = querySwapChainSupport(
		m_pPhysicalDevice->getPhysicalDevice(), instance->getSurface());

	VkSurfaceFormatKHR surfaceFormat =
		chooseSwapChainFormat(swapChainSupportDetails.formats);
	VkPresentModeKHR presentMode =
		chooseSwapPresentMode(swapChainSupportDetails.presentModes);
	VkExtent2D extent =
		chooseSwapExtent(swapChainSupportDetails.capabilities, m_pWindow);

	uint32_t imageCount =
		swapChainSupportDetails.capabilities.minImageCount + 1;
	if (swapChainSupportDetails.capabilities.minImageCount > 0 &&
		imageCount > swapChainSupportDetails.capabilities.maxImageCount) {
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

	QueueFamilyIndices indices = m_pPhysicalDevice->getQueueFamilyIndices();
	uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
									 indices.presentFamily.value()};
	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform =
		swapChainSupportDetails.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_pLogicalDevice->getDevice(), &createInfo,
							 nullptr, &m_swapChain) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create Swap Chain!");
	}

	vkGetSwapchainImagesKHR(m_pLogicalDevice->getDevice(), m_swapChain,
							&imageCount, nullptr);
	m_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_pLogicalDevice->getDevice(), m_swapChain,
							&imageCount, m_swapChainImages.data());
}

void VKSwapChain::recreateSwapChain(const VKInstance* instance) {
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_pWindow->getGLFWWindow(), &width, &height);
	if (width == 0 || height == 0) {
		glfwGetFramebufferSize(m_pWindow->getGLFWWindow(), &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_pLogicalDevice->getDevice());

	cleanUpSwapChain();

	createSwapChain(instance);
	createImageViews();
	createColorResources();
	createDepthResources();
	createFramebuffers();
}

void VKSwapChain::createImageViews() {
	m_swapChainImageViews.resize(m_swapChainImages.size());

	for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
		m_swapChainImageViews[i] =
			createImageView(m_swapChainImages[i], m_swapChainImageFormat,
							VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

VkImageView VKSwapChain::createImageView(VkImage image, VkFormat format,
										 VkImageAspectFlags aspectFlags,
										 uint32_t mipLevels) {
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
	if (vkCreateImageView(m_pLogicalDevice->getDevice(), &viewInfo, nullptr,
						  &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}

	return imageView;
}

void VKSwapChain::cleanUpSwapChain() {
	vkDestroyImageView(m_pLogicalDevice->getDevice(), m_colorImageView,
					   nullptr);
	vkDestroyImage(m_pLogicalDevice->getDevice(), m_colorImage, nullptr);
	vkFreeMemory(m_pLogicalDevice->getDevice(), m_colorImageMemory, nullptr);
	vkDestroyImageView(m_pLogicalDevice->getDevice(), m_depthImageView,
					   nullptr);
	vkDestroyImage(m_pLogicalDevice->getDevice(), m_depthImage, nullptr);
	vkFreeMemory(m_pLogicalDevice->getDevice(), m_depthImageMemory, nullptr);

	for (auto framebuffer : m_swapChainFramebuffers) {
		vkDestroyFramebuffer(m_pLogicalDevice->getDevice(), framebuffer,
							 nullptr);
	}

	for (auto imageView : m_swapChainImageViews) {
		vkDestroyImageView(m_pLogicalDevice->getDevice(), imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_pLogicalDevice->getDevice(), m_swapChain, nullptr);

	vkDestroyRenderPass(m_pLogicalDevice->getDevice(), m_renderPass, nullptr);
}

void VKSwapChain::createRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapChainImageFormat;
	colorAttachment.samples = VKPipeline::s_msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = VKPipeline::s_msaaSamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout =
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout =
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachmentResolve{};
	colorAttachmentResolve.format = m_swapChainImageFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentResolveRef{};
	colorAttachmentResolveRef.attachment = 2;
	colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	subpass.pResolveAttachments = &colorAttachmentResolveRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcAccessMask = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
							  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
							  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
							   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 3> attachments = {
		colorAttachment, depthAttachment, colorAttachmentResolve};
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_pLogicalDevice->getDevice(), &renderPassInfo,
						   nullptr, &m_renderPass) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a render pass!");
	}
}

void VKSwapChain::createFramebuffers() {
	m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

	for (int i = 0; i < m_swapChainImageViews.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			m_colorImageView, m_depthImageView, m_swapChainImageViews[i]};

		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = m_renderPass;
		framebufferCreateInfo.attachmentCount =
			static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = m_swapChainExtent.width;
		framebufferCreateInfo.height = m_swapChainExtent.height;
		framebufferCreateInfo.layers = 1;

		if (vkCreateFramebuffer(m_pLogicalDevice->getDevice(),
								&framebufferCreateInfo, nullptr,
								&m_swapChainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

void VKSwapChain::createColorResources() {
	VkFormat colorFormat = m_swapChainImageFormat;
	createImage(m_swapChainExtent.width, m_swapChainExtent.height, 1,
				VKPipeline::s_msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_colorImage,
				m_colorImageMemory);
	m_colorImageView = createImageView(m_colorImage, colorFormat,
									   VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void VKSwapChain::createDepthResources() {
	VkFormat depthFormat = findDepthFormat();
	createImage(m_swapChainExtent.width, m_swapChainExtent.height, 1,
				VKPipeline::s_msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage,
				m_depthImageMemory);
	m_depthImageView = createImageView(m_depthImage, depthFormat,
									   VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

void VKSwapChain::createImage(uint32_t width, uint32_t height,
							  uint32_t mipLevels, VkSampleCountFlagBits samples,
							  VkFormat format, VkImageTiling tiling,
							  VkImageUsageFlags usage,
							  VkMemoryPropertyFlags properties, VkImage& image,
							  VkDeviceMemory& imageMemory) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = samples;
	imageInfo.flags = 0;

	if (vkCreateImage(m_pLogicalDevice->getDevice(), &imageInfo, nullptr,
					  &image) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create image texture!");
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(m_pLogicalDevice->getDevice(), image,
								 &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex =
		findMemoryType(memoryRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(m_pLogicalDevice->getDevice(), &allocateInfo, nullptr,
						 &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate memory for texture!");
	}

	vkBindImageMemory(m_pLogicalDevice->getDevice(), image, imageMemory, 0);
}

uint32_t VKSwapChain::findMemoryType(uint32_t typeFilter,
									 VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_pPhysicalDevice->getPhysicalDevice(),
										&memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
			(memoryProperties.memoryTypes[i].propertyFlags & properties) ==
				properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

VkSurfaceFormatKHR VKSwapChain::chooseSwapChainFormat(
	const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	for (const auto& format : availableFormats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
			format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}
	return availableFormats[0];
}

VkPresentModeKHR VKSwapChain::chooseSwapPresentMode(
	const std::vector<VkPresentModeKHR>& availablePresentModes) {
	for (const auto& present : availablePresentModes) {
		if (present == VK_PRESENT_MODE_MAILBOX_KHR) {
			return present;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VKSwapChain::chooseSwapExtent(
	const VkSurfaceCapabilitiesKHR& capabilities, const Window* window) {
	if (capabilities.currentExtent.width !=
		std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		int width, height;
		glfwGetFramebufferSize(const_cast<GLFWwindow*>(window->getGLFWWindow()),
							   &width, &height);

		VkExtent2D actualExtent = {static_cast<uint32_t>(width),
								   static_cast<uint32_t>(height)};

		actualExtent.width =
			std::clamp(actualExtent.width, capabilities.minImageExtent.width,
					   capabilities.maxImageExtent.width);
		actualExtent.height =
			std::clamp(actualExtent.height, capabilities.minImageExtent.height,
					   capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

SwapChainSupportDetails VKSwapChain::querySwapChainSupport(
	const VkPhysicalDevice& device, const VkSurfaceKHR& surface) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
											  &details.capabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
										 nullptr);
	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
											 details.formats.data());
	}

	uint32_t presentCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount,
											  nullptr);
	if (presentCount != 0) {
		details.presentModes.resize(presentCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			device, surface, &presentCount, details.presentModes.data());
	}

	return details;
}

VkFormat VKSwapChain::findDepthFormat() {
	return findSupportedFormat(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
		 VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat VKSwapChain::findSupportedFormat(
	const std::vector<VkFormat>& candidates, VkImageTiling tiling,
	VkFormatFeatureFlags features) {
	for (auto format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(
			m_pPhysicalDevice->getPhysicalDevice(), format, &props);
		if (tiling == VK_IMAGE_TILING_LINEAR &&
			(props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
				   (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("Failed to find supported image format!");
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

}  // namespace bird::vk
