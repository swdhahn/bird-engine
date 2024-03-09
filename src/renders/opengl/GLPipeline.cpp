//
// Created by Seth Hahn on 11/17/22.
//

#include "GLPipeline.h"
#include <memory>

namespace bird::gl {

    GLPipeline::GLPipeline() : GraphicsPipeline(GRAPHICS_PIPELINE_OPENGL) {

    }

    GLPipeline::~GLPipeline() {

    }

    void GLPipeline::init() {
        m_pWindow = std::make_unique<Window>(1280, 720, "Window");

        glewExperimental = true;
        int code = 0;
        if((code = glewInit())) {
            std::cerr << glewGetErrorString(code) << std::endl;
            throw std::runtime_error("Couldn't initialize GLEW!");
        }

        initializeGraphicSpecifics();

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_DEBUG_OUTPUT);

    }

    void GLPipeline::renderRootScene(const bird::Scene *scene) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0, 0, 0, 1);

        renderScene(scene);

        glfwSwapBuffers(m_pWindow->getGLFWWindow());
    }

    void GLPipeline::renderScene(const Scene *scene) {
        std::vector<ModelEntity*> entities;
        getModelEntities(scene, entities);

        // cull entities here

        // render entities

        GlobalUBOData global;
        MaterialUBOData matData;

        global.perspective = m_camera->getPerspectiveMatrix();
        global.view = m_camera->getTransformMatrix();

        for(int i = 0; i < entities.size(); i++) {
            ModelEntity* e = entities[i];
            for(int j = 0; j < e->getMeshes().size(); j++) {
                GLMesh* m = (GLMesh*)e->getMeshes()[j].get();
                const std::shared_ptr<Material> mat = e->getMeshes()[j]->getMaterial();
                GLShader* shader = (GLShader*)mat->getShader().get();
                shader->bind();
                glBindVertexArray(m->getVAO());

                global.model = e->getTransformMatrix();

                m_globalUBO->update(&global, 1, 0);

                //std::cout << "Material:" << mat.get()->getDiffuseColor() << std::endl;

                matData.color = mat.get()->getDiffuseColor();
                matData.specularColor = mat.get()->getSpecularColor();
                matData.ambientColor = mat.get()->getAmbientColor();
                matData.shininess = mat.get()->getShininess();

                m_materialUBO->update(&matData, 1, 0);

                //glGetBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MaterialUBOData), &data);
                //std::cout << "Material:" << data.color << std::endl;

                // TODO: Uniform buffer objects and textures
                //shader->loadPerspectiveMatrix(m_camera->getPerspectiveMatrix());
                //shader->loadViewMatrix(m_camera->getTransformMatrix());
                //shader->loadModelMatrix(e->getTransformMatrix());

                if(mat != nullptr) {
                    for(int k = 0; k < mat->getTextures().size(); k++) {
                        glActiveTexture(GL_TEXTURE0 + k);
                        ((GLTexture*) mat->getTextures()[k].get())->bind();
                    }
                }
                glDrawElements(GL_TRIANGLES, e->getMeshes()[j]->getIndexBuffer()->getBufferSize(), GL_UNSIGNED_INT, 0);
            }
        }
        for(int i = 0; i < scene->getChildren().size(); i++) {
            // cull scene here
            renderScene(scene->getChildren()[i]);
        }
    }

    void GLPipeline::cleanUp() {
        m_pWindow.reset();
    }

    std::string GLPipeline::getName() const {
        return "OpenGL Pipeline";
    }

} // bird