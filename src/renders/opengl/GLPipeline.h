//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_GLPIPELINE_H
#define BIRD_GLPIPELINE_H

#include "../../util/File.h"
#include "../GraphicsPipeline.h"
#include "GLMesh.h"
#include "GLShader.h"

namespace bird::gl {

class GLPipeline : public GraphicsPipeline {
public:
  GLPipeline();
  ~GLPipeline() override;
  void init() override;
  void cleanUp() override;

  void renderRootScene(const Scene *scene) override;

  std::string getName() const override;

  std::unique_ptr<Shader> m_shader;

private:
  void renderScene(const Scene *scene);
};

} // namespace bird::gl

#endif // BIRD_GLPIPELINE_H
