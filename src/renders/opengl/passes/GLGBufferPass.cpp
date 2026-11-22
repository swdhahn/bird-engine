#include "GLGBufferPass.h"

#include "../GLApi.h"
#include "../GLCapabilities.h"

#include "../../../util/ResourceManager.h"
#include "../GLDrawCommands.h"

namespace bird::gl {

GLGBufferPass::GLGBufferPass(ResourceManager& resources,
							 GLSkinningBlock& skinning)
	: m_programs(resources.loadShader("assets/shaders/mesh.vert",
									  "assets/shaders/gbuffer.frag"),
				 resources.loadShader("assets/shaders/mesh_skinned.vert",
									  "assets/shaders/gbuffer.frag"),
				 skinning) {}

void GLGBufferPass::execute(const std::vector<RenderItem>& items,
							GLRenderTargets& targets, const Vector3& clearColor,
							GLRenderState& state) {
	targets.gBuffer().bindForDrawing();
	state.setDepthWrite(true);	// glClear honors the depth mask
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	const float background[4] = {clearColor.x, clearColor.y, clearColor.z, 1.0f};
	glClearBufferfv(GL_COLOR, 0, zeros);
	glClearBufferfv(GL_COLOR, 1, zeros);
	glClearBufferfv(GL_COLOR, 2, background);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

	state.setBlend(GLBlend::None);
	state.setDepthTest(true);
	state.setDepthFunc(GLDepthFunc::Less);

	m_programs.reset();
	// RT0 is sRGB storage: encoding on write keeps dark albedo precise.
	// Without write control (ES) sRGB attachments always encode.
	const bool srgbToggle = GLCapabilities::current().srgbWriteControl;
	if (srgbToggle) glEnable(GL_FRAMEBUFFER_SRGB);
	drawItems(items, m_programs, state);
	if (srgbToggle) glDisable(GL_FRAMEBUFFER_SRGB);
}

}  // namespace bird::gl
