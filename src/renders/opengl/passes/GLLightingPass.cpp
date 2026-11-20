#include "GLLightingPass.h"

#include "../GLApi.h"

#include <cmath>

#include "../../../util/ResourceManager.h"
#include "../../generic/PrimitiveMeshes.h"
#include "../GLDrawCommands.h"

namespace bird::gl {

Matrix4 lightVolumeTransform(const LightSnapshot& light) {
	if (light.type == LightType::Spot) {
		const Vector3 forward = glm::normalize(light.direction);
		const Vector3 up = std::abs(forward.y) > 0.99f ? Vector3(0, 0, 1)
													   : Vector3(0, 1, 0);
		const Matrix4 pose = glm::inverse(
			glm::lookAt(light.position, light.position + forward, up));
		const float radius = light.range * std::tan(light.outerConeAngle);
		return glm::scale(pose, Vector3(radius, radius, light.range));
	}
	return glm::scale(glm::translate(Matrix4(1.0f), light.position),
					  Vector3(light.range));
}

GLLightingPass::GLLightingPass(ResourceManager& resources) {
	m_directionalShader =
		resources.loadShader("assets/shaders/passthrough.vert",
							 "assets/shaders/light_directional.frag");
	m_volumeShader = resources.loadShader("assets/shaders/light_volume.vert",
										  "assets/shaders/light_volume.frag");
	m_sphere = buildPositionMesh(generateLightSphere(16, 12));
	m_cone = buildPositionMesh(generateLightCone(24));
}

void GLLightingPass::bindGBuffer(const GLRenderTargets& targets) {
	bindTextureUnit(SAMPLER_BINDING_GBUFFER_ALBEDO, targets.albedoOcclusion());
	bindTextureUnit(SAMPLER_BINDING_GBUFFER_NORMAL,
					targets.normalRoughnessMetallic());
	bindTextureUnit(SAMPLER_BINDING_GBUFFER_DEPTH, targets.depth());
}

void GLLightingPass::execute(const std::vector<LightSnapshot>& lights,
							 const Frustum& frustum, GLRenderTargets& targets,
							 GLRenderState& state) {
	targets.hdr().bindForDrawing();
	bindGBuffer(targets);
	state.setBlend(GLBlend::Additive);
	state.setDepthWrite(false);
	state.setCull(GLCull::None);

	// Ambient and directional lights touch every covered pixel.
	state.setDepthTest(false);
	static_cast<GLShader&>(*m_directionalShader).bind();
	drawFullscreenQuad();

	auto& volumeShader = static_cast<GLShader&>(*m_volumeShader);
	volumeShader.bind();
	state.setDepthTest(true);
	state.setDepthFunc(GLDepthFunc::Greater);
	state.setDepthClamp(true);	// far faces beyond the far plane still draw
	for (const LightSnapshot& light : lights) {
		if (light.type == LightType::Directional || light.intensity <= 0.0f) {
			continue;
		}
		if (!frustum.intersectsSphere(light.position, light.range)) continue;
		const LightVolumePushConstants push{lightVolumeTransform(light),
											toGpuLight(light)};
		volumeShader.setPushConstants(&push, sizeof(push));
		drawMesh(light.type == LightType::Spot ? *m_cone : *m_sphere);
	}
	glBindVertexArray(0);
	state.setDepthClamp(false);
	state.setDepthFunc(GLDepthFunc::Less);
}

}  // namespace bird::gl
