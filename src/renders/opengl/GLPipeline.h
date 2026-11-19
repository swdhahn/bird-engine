//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_GLPIPELINE_H
#define BIRD_GLPIPELINE_H

#include <memory>
#include <string>

#include "../../util/File.h"
#include "../GraphicsPipeline.h"
#include "GLMesh.h"
#include "GLShader.h"
#include "GLTexture.h"
#include "GLDebugRenderer.h"
#include "GLUIRenderer.h"

namespace bird::gl {

/**
 * OpenGL 4.1+/4.6 graphics pipeline backend.
 *
 * Implements a three-phase rendering sequence:
 * 1. Scene Pass: Renders 3D meshes into an offscreen HDR/color framebuffer.
 * 2. Post-Process Pass: Draws full-screen quad applying FXAA antialiasing.
 * 3. 2D UI Pass: Renders batched orthographic UI draw commands with alpha blending.
 */
class GLPipeline : public GraphicsPipeline {
   public:
	GLPipeline();
	~GLPipeline() override;

	/** Initializes GLFW context, GLEW bindings, depth testing, shaders, and FBOs. */
	void init() override;

	/** Frees OpenGL state and framebuffers. */
	void cleanUp() override;

	/** Executes scene rendering, post-processing, and UI passes for active scene. */
	void renderRootScene(Scene* scene) override;

	/** Re-sizes internal scene framebuffers and UI orthographic projection. */
	void resize(uint32_t framebufferWidth, uint32_t framebufferHeight,
				uint32_t uiWidth, uint32_t uiHeight) override;

	std::string getName() const override;

   private:
	void renderScene(Scene* scene);

	/** Draws the debug line list into the scene target (before post). */
	void renderDebugLines();

	/** Draws the batched UI mesh with an orthographic projection. */
	void renderUI();

	/** (Re)creates the post-processed scene target at the current size. */
	void createSceneFramebuffer();

	std::shared_ptr<Shader> ui_shader;
	std::shared_ptr<Shader> m_postProcessShader;
	std::unique_ptr<GLUIRenderer> m_uiRenderer;
	std::shared_ptr<Shader> m_debugLineShader;
	std::unique_ptr<GLDebugRenderer> m_debugRenderer;
};

}  // namespace bird::gl

#endif	// BIRD_GLPIPELINE_H
