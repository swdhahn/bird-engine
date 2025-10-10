//
// Created by Seth Hahn on 11/17/22.
//

#include "GLPipeline.h"
#include <memory>

namespace bird::gl {

GLPipeline::GLPipeline() : GraphicsPipeline(GRAPHICS_PIPELINE_OPENGL) {}

GLPipeline::~GLPipeline() {}

void GLPipeline::init() {
  m_pWindow = std::make_unique<Window>(1280, 720, "Window");

  glewExperimental = true;
  int code = 0;
  if ((code = glewInit())) {
    std::cerr << glewGetErrorString(code) << std::endl;
    throw std::runtime_error("Couldn't initialize GLEW!");
  }

  std::cout << "Hi" << std::endl;
  /*m_shader = ShaderBuilder()
          .attachShaderFile("shaders/vertex.glsl", VERTEX)
          .attachShaderFile("shaders/fragment.glsl", FRAGMENT)
          .create();//*/

  glEnable(GL_DEPTH_TEST);
}

void GLPipeline::renderRootScene(const bird::Scene *scene) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glClearColor(0, 0, 0, 1);

  renderScene(scene);

  glfwSwapBuffers(m_pWindow->getGLFWWindow());
}

void GLPipeline::renderScene(const Scene *scene) {
  std::vector<ModelEntity *> entities;
  getModelEntities(scene, entities);

  // cull entities here

  // render entities

  m_shader->loadPerspectiveMatrix(m_camera->getPerspectiveMatrix());
  m_shader->loadViewMatrix(m_camera->getTransformMatrix());

  for (int i = 0; i < entities.size(); i++) {
    ModelEntity *e = entities[i];
    for (int j = 0; j < e->getMeshes().size(); j++) {
      GLMesh *m = (GLMesh *)e->getMeshes()[j].get();
      glBindVertexArray(m->getVAO());
      // TODO: Uniform buffer objects and textures
      m_shader->loadModelMatrix(e->getTransformMatrix());

      glDrawElements(GL_TRIANGLES,
                     e->getMeshes()[j]->getIndexBuffer()->getBufferSize(),
                     GL_UNSIGNED_INT, 0);
    }
  }
  for (int i = 0; i < scene->getChildren().size(); i++) {
    // cull scene here
    renderScene(scene->getChildren()[i]);
  }
}

void GLPipeline::cleanUp() { m_pWindow.reset(); }

std::string GLPipeline::getName() const { return "OpenGL Pipeline"; }

} // namespace bird::gl
