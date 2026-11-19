//
// Created by Seth Hahn on 11/7/22.
//

#ifndef BIRD_VKRENDERPIPELINE_H
#define BIRD_VKRENDERPIPELINE_H

namespace bird::vk {

    /**
     * Manages a Vulkan VkPipeline state object (PSO) and pipeline layout.
     */
    class VKRenderPipeline {
    public:
        VKRenderPipeline();

    private:
        void createRenderPass();
    };

} // namespace bird::vk

#endif //BIRD_VKRENDERPIPELINE_H
