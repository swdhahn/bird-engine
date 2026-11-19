//
// Created by Seth Hahn on 11/7/22.
//

#ifndef BIRD_VKINSTANCE_H
#define BIRD_VKINSTANCE_H

#include <string>
#include <volk.h>

#include "../Window.h"
#include "VKConstants.h"

namespace bird::vk {

/**
 * Initializes and manages a VkInstance and the presentation VkSurfaceKHR.
 */
class VKInstance {
   public:
	VKInstance(std::string applicationName, std::string engineName,
			   Window* window);
	~VKInstance();

	/** Creates the Vulkan application instance with debug validation layers if enabled. */
	void createInstance(std::string applicationName, std::string engineName);

	/** Binds a GLFW window surface to the Vulkan instance. */
	void createSurface(Window* window);

	const VkInstance& getInstance() const;
	const VkSurfaceKHR& getSurface() const;

   private:
	VkInstance m_instance;
	VkSurfaceKHR m_surface;
	VkDebugUtilsMessengerEXT m_debugMessenger;

	void populateDebugMessengerCreateInfo(
		VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void setupDebugMessenger();
};

}  // namespace bird::vk

#endif	// BIRD_VKINSTANCE_H
