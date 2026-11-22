#include "GLForwardPass.h"

#include "../GLApi.h"

#include "../../../util/ResourceManager.h"
#include "../GLDrawCommands.h"

namespace bird::gl {

GLForwardPass::GLForwardPass(ResourceManager& resources,
							 GLSkinningBlock& skinning)
	: m_programs(resources.loadShader("assets/shaders/mesh.vert",
									  "assets/shaders/forward.frag"),
				 resources.loadShader("assets/shaders/mesh_skinned.vert",
									  "assets/shaders/forward.frag"),
				 skinning),
	  m_particleRenderer(resources) {}

void GLForwardPass::execute(
	const RenderQueue& queue,
	const std::vector<ParticleEmitter*>& particleEmitters,
	const Matrix4& cameraView, GLRenderTargets& targets,
	GLRenderState& state) {
	const auto& opaque = queue.bucket(RenderBucket::ForwardOpaque);
	const auto& transparent = queue.bucket(RenderBucket::Transparent);
	if (opaque.empty() && transparent.empty() && particleEmitters.empty()) {
		return;
	}

	targets.hdr().bindForDrawing();
	m_programs.reset();
	state.setDepthTest(true);
	state.setDepthFunc(GLDepthFunc::LessEqual);

	state.setBlend(GLBlend::None);
	state.setDepthWrite(true);
	drawItems(opaque, m_programs, state);

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
		GLShader& shader = m_programs.prepare(item);
		const ObjectPushConstants object{item.model};
		shader.setPushConstants(&object, sizeof(object));
		drawMesh(*item.mesh);
	}
	glBindVertexArray(0);

	// Batched CPU billboard particle emitters render inside the forward
	// transparency pass with depth testing enabled, depth writes disabled,
	// and alpha or additive blending.
	if (!particleEmitters.empty()) {
		m_particleRenderer.draw(particleEmitters, cameraView, state);
	}

	state.setDepthWrite(true);
	state.setDepthFunc(GLDepthFunc::Less);
	state.setBlend(GLBlend::None);
}

}  // namespace bird::gl
