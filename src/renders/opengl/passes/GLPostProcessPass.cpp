#include "GLPostProcessPass.h"

#include <GL/glew.h>

#include "../../../util/ResourceManager.h"
#include "../GLDrawCommands.h"

namespace bird::gl {

GLPostProcessPass::GLPostProcessPass(ResourceManager& resources) {
	m_tonemapShader =
		resources.loadShader("assets/shaders/passthrough.vert",
							 "assets/shaders/tonemap.frag");
}

void GLPostProcessPass::execute(GLRenderTargets& targets, uint32_t screenWidth,
								uint32_t screenHeight, GLRenderState& state) {
	state.setDepthTest(false);
	state.setDepthWrite(false);
	state.setBlend(GLBlend::None);
	state.setCull(GLCull::None);

	targets.ldr().bindForDrawing();
	static_cast<GLShader&>(*m_tonemapShader).bind();
	bindTextureUnit(0, targets.hdrColor());
	bindTextureUnit(SAMPLER_BINDING_GBUFFER_ALBEDO, targets.albedoOcclusion());
	bindTextureUnit(SAMPLER_BINDING_GBUFFER_NORMAL,
					targets.normalRoughnessMetallic());
	bindTextureUnit(SAMPLER_BINDING_GBUFFER_DEPTH, targets.depth());
	drawFullscreenQuad();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenWidth, screenHeight);
	targets.ldr().bindAttachments();  // FXAA shader + LDR color on unit 0
	drawFullscreenQuad();
	state.setDepthWrite(true);
}

}  // namespace bird::gl
