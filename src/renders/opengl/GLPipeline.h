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
#include "GLRenderState.h"
#include "GLRenderTargets.h"
#include "GLUIRenderer.h"
#include "passes/GLForwardPass.h"
#include "passes/GLGBufferPass.h"
#include "passes/GLLightingPass.h"
#include "passes/GLPostProcessPass.h"
#include "passes/GLShadowPass.h"

namespace bird::gl {

/**
 * OpenGL 4.1+/4.6 graphics pipeline backend: deferred by default, with a
 * forward pass for what deferred shading cannot express.
 *
 * Frame sequence:
 *  1. Shadow:   caster depth into directional cascades and a local atlas.
 *  2. G-buffer: opaque and masked lit surfaces; emissive seeds HDR.
 *  3. Depth copy from the G-buffer into the HDR target.
 *  4. Lighting: ambient + directional fullscreen, point/spot volumes.
 *  5. Forward:  unlit/force-forward opaque, then sorted transparency.
 *  6. Debug lines into HDR.
 *  7. Post:     exposure + ACES tone mapping, then FXAA to the screen.
 *  8. UI.
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
	/** Executes passes 1 through 7 for the active camera. */
	void renderWorld();

	/** Draws the debug line list into the HDR target (before post). */
	void renderDebugLines();

	/** Draws the batched UI mesh with an orthographic projection. */
	void renderUI();

	/** (Re)creates the screen-sized targets at the current size. */
	void createRenderTargets();

	/**
	 * Development aid: when BIRD_CAPTURE names a file, writes the finished
	 * frame number BIRD_CAPTURE_FRAME (default 60) as a binary PPM.
	 */
	void captureFrameIfRequested();

	std::shared_ptr<Shader> ui_shader;
	std::shared_ptr<Shader> m_postProcessShader;
	GLRenderState m_state;
	uint64_t m_frameIndex = 0;
	std::unique_ptr<GLRenderTargets> m_targets;
	std::unique_ptr<Buffer<LightsUBOData>> m_lightsBlock;
	std::unique_ptr<GLShadowPass> m_shadowPass;
	std::unique_ptr<GLGBufferPass> m_gBufferPass;
	std::unique_ptr<GLLightingPass> m_lightingPass;
	std::unique_ptr<GLForwardPass> m_forwardPass;
	std::unique_ptr<GLPostProcessPass> m_postProcessPass;
	std::unique_ptr<GLUIRenderer> m_uiRenderer;
	std::shared_ptr<Shader> m_debugLineShader;
	std::unique_ptr<GLDebugRenderer> m_debugRenderer;
};

}  // namespace bird::gl

#endif	// BIRD_GLPIPELINE_H
