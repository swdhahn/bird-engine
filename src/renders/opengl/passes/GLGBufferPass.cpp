#include "GLGBufferPass.h"

#include <GL/glew.h>

#include "../../../util/ResourceManager.h"
#include "../GLDrawCommands.h"

namespace bird::gl {

GLGBufferPass::GLGBufferPass(ResourceManager& resources) {
	m_shader = resources.loadShader("assets/shaders/mesh.vert",
									"assets/shaders/gbuffer.frag");
}

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

	auto& shader = static_cast<GLShader&>(*m_shader);
	shader.bind();
	// RT0 is sRGB storage: encoding on write keeps dark albedo precise.
	glEnable(GL_FRAMEBUFFER_SRGB);
	drawItems(items, shader, state);
	glDisable(GL_FRAMEBUFFER_SRGB);
}

}  // namespace bird::gl
