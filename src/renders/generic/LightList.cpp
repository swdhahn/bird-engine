#include "LightList.h"

#include <algorithm>
#include <cmath>

namespace bird {

namespace {

float gpuTypeCode(LightType type) {
	switch (type) {
		case LightType::Directional:
			return GPU_LIGHT_DIRECTIONAL;
		case LightType::Spot:
			return GPU_LIGHT_SPOT;
		case LightType::Point:
		default:
			return GPU_LIGHT_POINT;
	}
}

}  // namespace

LightSnapshot snapshotLight(LightComponent& light) {
	LightSnapshot s;
	s.type = light.getType();
	s.position = light.getWorldPosition();
	s.direction = light.getWorldDirection();
	s.color = light.getColor();
	s.intensity = light.getIntensity();
	s.range = light.getRange();
	s.innerConeAngle = light.getInnerConeAngle();
	s.outerConeAngle = light.getOuterConeAngle();
	s.castsShadows = light.getCastsShadows();
	s.shadowDepthBias = light.getShadowDepthBias();
	s.shadowNormalBias = light.getShadowNormalBias();
	return s;
}

GpuLight toGpuLight(const LightSnapshot& light) {
	GpuLight gpu{};
	gpu.positionRange = Vector4(light.position, light.range);
	gpu.colorType = Vector4(light.color * light.intensity, gpuTypeCode(light.type));
	gpu.directionCosOuter =
		Vector4(glm::normalize(light.direction), std::cos(light.outerConeAngle));
	gpu.shadowParams =
		Vector4(std::cos(light.innerConeAngle),
				static_cast<float>(light.shadowMatrixBase),
				light.shadowDepthBias, light.shadowNormalBias);
	return gpu;
}

std::vector<size_t> selectForwardLights(const std::vector<LightSnapshot>& lights,
										const Vector3& cameraPosition,
										const Frustum& frustum,
										uint32_t maxLights) {
	std::vector<size_t> directional;
	std::vector<std::pair<float, size_t>> local;
	for (size_t i = 0; i < lights.size(); i++) {
		const LightSnapshot& light = lights[i];
		if (light.intensity <= 0.0f) continue;
		if (light.type == LightType::Directional) {
			directional.push_back(i);
		} else if (frustum.intersectsSphere(light.position, light.range)) {
			// Signed distance from the camera to the light's sphere: lights
			// enclosing the camera sort first.
			const float distance =
				glm::length(light.position - cameraPosition) - light.range;
			local.emplace_back(distance, i);
		}
	}
	std::stable_sort(local.begin(), local.end(),
					 [](const auto& a, const auto& b) { return a.first < b.first; });

	std::vector<size_t> selection;
	selection.reserve(std::min<size_t>(maxLights, lights.size()));
	for (size_t index : directional) {
		if (selection.size() == maxLights) return selection;
		selection.push_back(index);
	}
	for (const auto& entry : local) {
		if (selection.size() == maxLights) break;
		selection.push_back(entry.second);
	}
	return selection;
}

LightsUBOData buildLightsBlock(const std::vector<LightSnapshot>& lights,
							   const std::vector<size_t>& selection) {
	LightsUBOData block{};
	int32_t count = 0;
	int32_t directionalCount = 0;
	for (size_t index : selection) {
		if (count == static_cast<int32_t>(MAX_FORWARD_LIGHTS)) break;
		const LightSnapshot& light = lights.at(index);
		block.lights[count++] = toGpuLight(light);
		if (light.type == LightType::Directional) directionalCount++;
	}
	block.counts[0] = count;
	block.counts[1] = directionalCount;
	return block;
}

}  // namespace bird
