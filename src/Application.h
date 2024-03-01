//
// Created by student on 10/5/23.
//

#ifndef BIRD_APPLICATION_H
#define BIRD_APPLICATION_H

#include "renders/GraphicsPipeline.h"
#include "renders/opengl/GLPipeline.h"
#include "renders/vulkan/VKPipeline.h"
#include "util/Input.h"

namespace bird {

    class Application {
    public:
        Application(Scene& scene);
        ~Application();

        void init();
        void deinit();

        void run();
        void processScene(Scene* scene, float delta);
    private:
        GraphicsPipeline* m_graphicsPipeline;
        Scene* m_scene;

    };

} // bird

#endif //BIRD_APPLICATION_H
