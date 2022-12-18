//
// Created by Seth Hahn on 11/7/22.
//

#ifndef BIRD_VKRENDERPIPELINE_H
#define BIRD_VKRENDERPIPELINE_H

namespace bird::vk {

    class VKRenderPipeline {
    public:
        VKRenderPipeline();

    private:
        void createRenderPass();
    };

} // bird

#endif //BIRD_VKRENDERPIPELINE_H
