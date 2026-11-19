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
	void resize(uint32_t framebufferWidth, uint32_t framebufferHeight,
				uint32_t uiWidth, uint32_t uiHeight) override;

	std::string getName() const override;

   private:
	void renderScene(Scene* scene);
	/** Draws the batched UI mesh with an orthographic projection. */
	void renderUI();
	/** (Re)creates the post-processed scene target at the current size. */
	void createSceneFramebuffer();

	std::shared_ptr<Shader> ui_shader;
	std::shared_ptr<Shader> m_postProcessShader;
};

}  // namespace bird::gl

#endif	// BIRD_GLPIPELINE_H
