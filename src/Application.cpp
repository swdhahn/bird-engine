//
// Created by student on 10/5/23.
//

#include "Application.h"

namespace bird {

    Input* INPUT = nullptr;
    ResourceManager* RESOURCE_MANAGER = nullptr;

    Application::Application(Scene& scene) : m_scene(&scene) {}

    Application::~Application() {}

    void Application::init() {
        m_graphicsPipeline = new gl::GLPipeline();
        m_graphicsPipeline->init();
        INPUT = new Input(m_graphicsPipeline->getWindow().get());
        RESOURCE_MANAGER = new ResourceManager();

        INPUT->addAction(Action("move_forward").addKey(KEY_W));
        INPUT->addAction(Action("move_backward").addKey(KEY_S));
        INPUT->addAction(Action("move_left").addKey(KEY_A));
        INPUT->addAction(Action("move_right").addKey(KEY_D));
        INPUT->addAction(Action("move_up").addKey(KEY_SPACE));
        INPUT->addAction(Action("move_down").addKey(KEY_LEFT_SHIFT));

        INPUT->addAction(Action("look_up").addKey(KEY_UP));
        INPUT->addAction(Action("look_down").addKey(KEY_DOWN));
        INPUT->addAction(Action("look_left").addKey(KEY_LEFT));
        INPUT->addAction(Action("look_right").addKey(KEY_RIGHT));

        m_scene->init();

    }

    void Application::run() {
        init();

        double lastTime = glfwGetTime();
        float delta = 0.0f;

        while(!m_graphicsPipeline->getWindow()->shouldWindowClose()) {
            delta = (glfwGetTime() - lastTime);
            lastTime = glfwGetTime();
            processScene(m_scene, delta);
            m_graphicsPipeline->renderRootScene(m_scene);
            INPUT->tick();
            m_graphicsPipeline->getWindow()->pollWindow();

        }

        deinit();
    }

    void Application::processScene(Scene* scene, float delta) {
        for(auto& entity : scene->getEntities()) {
            entity->process(delta);
            if(entity->needsMatrixUpdate()) {
                entity->updateTransformationMatrix();
            }
        }
        for(auto& child : scene->getChildren()) {
            processScene(child, delta);
        }
    }

    void Application::deinit() {
        m_scene->deinit();

        delete INPUT;
        m_graphicsPipeline->cleanUp();
        delete m_graphicsPipeline;
    }

} // bird