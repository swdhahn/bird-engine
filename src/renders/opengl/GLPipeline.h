//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_GLPIPELINE_H
#define BIRD_GLPIPELINE_H

#include <memory>

#include "../../util/File.h"
#include "../GraphicsPipeline.h"
#include "GLMesh.h"
#include "GLShader.h"
#include "GLTexture.h"

namespace bird::gl {

class GLPipeline : public GraphicsPipeline {
   public:
	GLPipeline();
	~GLPipeline() override;
	void init() override;
	void cleanUp() override;

	void renderRootScene(Scene* scene) override;

	std::string getName() const override;

   private:
	void renderScene(Scene* scene);
	/** Draws the batched UI mesh with an orthographic projection. */
	void renderUI();

	std::shared_ptr<Shader> ui_shader;
};

}  // namespace bird::gl

#endif	// BIRD_GLPIPELINE_H
