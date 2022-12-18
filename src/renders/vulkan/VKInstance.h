//
// Created by Seth Hahn on 11/7/22.
//

#ifndef BIRD_VKINSTANCE_H
#define BIRD_VKINSTANCE_H

#include "VKConstants.h"
#include "../Window.h"

namespace bird::vk {

    class VKInstance {
    public:
        VKInstance(std::string applicationName, std::string engineName, Window* window);
        ~VKInstance();

        void createInstance(std::string applicationName, std::string engineName);
        void createSurface(Window* window);

        const VkInstance& getInstance() const;
        const VkSurfaceKHR& getSurface() const;
    private:
        VkInstance m_instance;
        VkSurfaceKHR m_surface;

    };

} // bird

#endif //BIRD_VKINSTANCE_H
