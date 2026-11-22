#include "GLSkyboxPass.h"

#include "../GLApi.h"

#include "../../../util/ResourceManager.h"
#include "../GLIBLEnvironment.h"
#include "../GLShader.h"

namespace bird::gl {

GLSkyboxPass::GLSkyboxPass(ResourceManager& resources) {
	m_shader = resources.loadShader("assets/shaders/skybox.vert",
									"assets/shaders/skybox.frag");
}

void GLSkyboxPass::execute(const EnvironmentMap* environment,
						   GLRenderTargets& targets, GLRenderState& state) {
	if (environment == nullptr) return;
	const auto& glEnv = static_cast<const GLIBLEnvironment&>(*environment);

	targets.hdr().bindForDrawing();
	state.setDepthTest(true);
	state.setDepthWrite(false);
	state.setDepthFunc(GLDepthFunc::LessEqual);
	state.setBlend(GLBlend::None);
	state.setCull(GLCull::None);

	static_cast<GLShader&>(*m_shader).bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, glEnv.getEnvironmentCubemapId());
	GLIBLEnvironment::drawUnitCube();
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	state.setDepthFunc(GLDepthFunc::Less);
	state.setDepthWrite(true);
}

}  // namespace bird::gl
