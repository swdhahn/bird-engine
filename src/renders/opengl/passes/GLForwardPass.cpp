#include "GLForwardPass.h"

#include "../GLApi.h"

#include "../../../util/ResourceManager.h"
#include "../GLDrawCommands.h"

namespace bird::gl {

GLForwardPass::GLForwardPass(ResourceManager& resources) {
	m_shader = resources.loadShader("assets/shaders/mesh.vert",
									"assets/shaders/forward.frag");
}

void GLForwardPass::execute(const RenderQueue& queue, GLRenderTargets& targets,
							GLRenderState& state) {
	const auto& opaque = queue.bucket(RenderBucket::ForwardOpaque);
	const auto& transparent = queue.bucket(RenderBucket::Transparent);
	if (opaque.empty() && transparent.empty()) return;

	targets.hdr().bindForDrawing();
	auto& shader = static_cast<GLShader&>(*m_shader);
	shader.bind();
	state.setDepthTest(true);
	state.setDepthFunc(GLDepthFunc::LessEqual);

	state.setBlend(GLBlend::None);
	state.setDepthWrite(true);
	drawItems(opaque, shader, state);

	// Blended surfaces test against depth but never write it, so that
	// overlapping transparent layers all remain visible.
	state.setDepthWrite(false);
	const Material* bound = nullptr;
	for (const RenderItem& item : transparent) {
		if (item.material != bound) {
			bindMaterial(*item.material, state);
			state.setBlend(item.material->getBlendMode() == BlendMode::Additive
							   ? GLBlend::AdditiveAlpha
							   : GLBlend::Alpha);
			bound = item.material;
		}
		const ObjectPushConstants object{item.model};
		shader.setPushConstants(&object, sizeof(object));
		drawMesh(*item.mesh);
	}
	glBindVertexArray(0);
	state.setDepthWrite(true);
	state.setDepthFunc(GLDepthFunc::Less);
	state.setBlend(GLBlend::None);
}

}  // namespace bird::gl
