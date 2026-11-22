#include "GLShadowPass.h"

#include "../GLApi.h"

#include "../../../util/ResourceManager.h"
#include "../GLDrawCommands.h"

namespace bird::gl {

namespace {

std::shared_ptr<Texture> makeAtlas(uint32_t size) {
	return TextureBuilder(size, size, FORMAT_DEPTH32F)
		.clampToEdge()
		.depthCompare()
		.build();
}

}  // namespace

GLShadowPass::GLShadowPass(ResourceManager& resources,
						   const ShadowSettings& settings,
						   GLSkinningBlock& skinning)
	: m_settings(settings),
	  m_depthPrograms(
		  resources.loadShader("assets/shaders/shadow_depth.vert",
							   "assets/shaders/shadow_depth.frag"),
		  resources.loadShader("assets/shaders/shadow_depth_skinned.vert",
							   "assets/shaders/shadow_depth.frag"),
		  skinning) {
	m_directionalDepth = makeAtlas(settings.directionalAtlasSize);
	m_localDepth = makeAtlas(settings.localAtlasSize);
	m_directionalTarget = std::make_unique<GLFrameBuffer>(
		settings.directionalAtlasSize, settings.directionalAtlasSize,
		std::vector<std::shared_ptr<Texture>>{}, m_directionalDepth, nullptr,
		FRAMEBUFFER_OPT_NONE);
	m_localTarget = std::make_unique<GLFrameBuffer>(
		settings.localAtlasSize, settings.localAtlasSize,
		std::vector<std::shared_ptr<Texture>>{}, m_localDepth, nullptr,
		FRAMEBUFFER_OPT_NONE);

	auto initial = std::make_unique<ShadowUBOData[]>(1);
	m_shadowBlock = BufferBuilder<ShadowUBOData>(BUFFER_UNIFORM |
												 BUFFER_STAGED | BUFFER_DYNAMIC)
						.data(std::move(initial), 1)
						.bindingPoint(UBO_BINDING_SHADOWS)
						.build();
}

void GLShadowPass::execute(const ShadowPlan& plan,
						   const std::vector<RenderItem>& casters,
						   GLRenderState& state) {
	ShadowUBOData data = plan.gpuData;
	m_shadowBlock->update(&data, 1, 0);
	if (plan.views.empty()) return;

	m_depthPrograms.reset();
	state.setBlend(GLBlend::None);
	state.setDepthTest(true);
	state.setDepthWrite(true);
	state.setDepthFunc(GLDepthFunc::Less);
	state.setDepthClamp(true);	// casters behind the near plane still occlude
	state.setCull(GLCull::None);
	state.setPolygonOffset(true, 2.0f, 4.0f);

	// Each atlas is cleared once, before its first view.
	bool directionalCleared = false;
	bool localCleared = false;
	for (const ShadowView& view : plan.views) {
		const bool directional = view.atlas == ShadowAtlasKind::Directional;
		GLFrameBuffer& target =
			directional ? *m_directionalTarget : *m_localTarget;
		bool& cleared = directional ? directionalCleared : localCleared;
		target.bindForDrawing();
		if (!cleared) {
			state.setScissor(false);
			glClear(GL_DEPTH_BUFFER_BIT);
			cleared = true;
		}
		glViewport(view.tile.x, view.tile.y, view.tile.size, view.tile.size);
		state.setScissor(true);
		glScissor(view.tile.x, view.tile.y, view.tile.size, view.tile.size);

		for (const RenderItem& caster : casters) {
			if (!view.frustum.intersects(caster.worldBounds)) continue;
			GLShader& shader = m_depthPrograms.prepare(caster);
			const ShadowPushConstants push{caster.model, view.viewProjection};
			shader.setPushConstants(&push, sizeof(push));
			drawMesh(*caster.mesh);
		}
	}
	glBindVertexArray(0);
	state.setScissor(false);
	state.setPolygonOffset(false);
	state.setDepthClamp(false);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLShadowPass::bindForSampling() {
	m_shadowBlock->bindToBindingPoint();
	bindTextureUnit(SAMPLER_BINDING_SHADOW_DIRECTIONAL, *m_directionalDepth);
	bindTextureUnit(SAMPLER_BINDING_SHADOW_LOCAL, *m_localDepth);
}

}  // namespace bird::gl
