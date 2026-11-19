#include "ShadowPlanner.h"

#include <algorithm>
#include <cmath>

#include "ShadowMath.h"

namespace bird {

namespace {

constexpr float ORTHOGRAPHIC = 0.0f;
constexpr float PERSPECTIVE = 1.0f;

/** Records one view and its GPU matrix slot; returns false when full. */
bool appendView(ShadowPlan& plan, uint32_t& nextSlot, ShadowAtlasKind atlas,
				const AtlasTile& tile, uint32_t atlasSize,
				const Matrix4& viewProjection, float texelSize,
				float projectionKind) {
	if (nextSlot >= MAX_SHADOW_MATRICES) return false;
	plan.gpuData.matrices[nextSlot] = viewProjection;
	plan.gpuData.atlasRects[nextSlot] = tile.uvRect(atlasSize);
	plan.gpuData.matrixParams[nextSlot] =
		Vector4(texelSize, projectionKind, 0.0f, 0.0f);
	plan.views.push_back(ShadowView{atlas, tile, viewProjection,
									Frustum::fromViewProjection(viewProjection)});
	nextSlot++;
	return true;
}

void planDirectional(std::vector<LightSnapshot>& lights,
					 const ShadowCameraInfo& camera,
					 const ShadowSettings& settings, ShadowPlan& plan,
					 uint32_t& nextSlot) {
	auto sun = std::find_if(lights.begin(), lights.end(), [](const auto& l) {
		return l.type == LightType::Directional && l.castsShadows &&
			   l.intensity > 0.0f;
	});
	if (sun == lights.end()) return;

	const uint32_t cascadeTile = settings.directionalAtlasSize / 2;
	const std::vector<float> splits = shadow::computeCascadeSplits(
		camera.nearPlane, camera.shadowDistance, shadow::CASCADE_COUNT,
		settings.cascadeLambda);
	ShadowAtlas atlas(settings.directionalAtlasSize, cascadeTile);

	sun->shadowMatrixBase = static_cast<int32_t>(nextSlot);
	for (uint32_t c = 0; c < shadow::CASCADE_COUNT; c++) {
		const auto tile = atlas.allocate(cascadeTile);
		const shadow::CascadeProjection cascade = shadow::fitCascade(
			camera.inverseView, camera.fovY, camera.aspect, splits[c],
			splits[c + 1], sun->direction, cascadeTile,
			settings.cascadePullback);
		appendView(plan, nextSlot, ShadowAtlasKind::Directional, *tile,
				   settings.directionalAtlasSize, cascade.viewProjection,
				   cascade.texelWorldSize, ORTHOGRAPHIC);
		plan.gpuData.cascadeSplits[c] = splits[c + 1];
	}
	plan.gpuData.shadowInfo.x = static_cast<float>(shadow::CASCADE_COUNT);
}

void planLocal(std::vector<LightSnapshot>& lights,
			   const ShadowCameraInfo& camera, const Frustum& frustum,
			   const ShadowSettings& settings, ShadowPlan& plan,
			   uint32_t& nextSlot) {
	struct Candidate {
		size_t index;
		float priority;
	};
	std::vector<Candidate> candidates;
	for (size_t i = 0; i < lights.size(); i++) {
		const LightSnapshot& l = lights[i];
		if (l.type == LightType::Directional || !l.castsShadows ||
			l.intensity <= 0.0f || !frustum.intersectsSphere(l.position, l.range)) {
			continue;
		}
		// Approximate projected size: range over distance to the sphere.
		const float distance =
			std::max(glm::length(l.position - camera.position) - l.range, 1.0f);
		candidates.push_back({i, l.range / distance});
	}
	std::stable_sort(candidates.begin(), candidates.end(),
					 [](const Candidate& a, const Candidate& b) {
						 return a.priority > b.priority;
					 });

	ShadowAtlas atlas(settings.localAtlasSize, settings.minLocalTile);
	for (const Candidate& candidate : candidates) {
		LightSnapshot& light = lights[candidate.index];
		const bool isPoint = light.type == LightType::Point;
		const uint32_t faceCount = isPoint ? shadow::CUBE_FACE_COUNT : 1;
		if (nextSlot + faceCount > MAX_SHADOW_MATRICES) break;

		const float wanted = std::clamp(
			candidate.priority * static_cast<float>(settings.maxLocalTile),
			static_cast<float>(settings.minLocalTile),
			static_cast<float>(settings.maxLocalTile));
		const std::vector<AtlasTile> tiles =
			atlas.allocateGroup(static_cast<uint32_t>(wanted), faceCount);
		if (tiles.empty()) continue;  // atlas exhausted: unshadowed
		const uint32_t tileSize = tiles.front().size;

		light.shadowMatrixBase = static_cast<int32_t>(nextSlot);
		if (isPoint) {
			const auto faces = shadow::pointFaceViewProjections(
				light.position, light.range, tileSize);
			const float fov =
				shadow::guardedFieldOfView(glm::radians(90.0f), tileSize, 2.0f);
			const float texel = 2.0f * std::tan(fov * 0.5f) / tileSize;
			for (uint32_t face = 0; face < faceCount; face++) {
				appendView(plan, nextSlot, ShadowAtlasKind::Local, tiles[face],
						   settings.localAtlasSize, faces[face], texel,
						   PERSPECTIVE);
			}
		} else {
			const Matrix4 viewProjection = shadow::spotViewProjection(
				light.position, light.direction, light.outerConeAngle,
				light.range, tileSize);
			const float fov = std::min(
				shadow::guardedFieldOfView(2.0f * light.outerConeAngle,
										   tileSize, 2.0f),
				3.1f);
			const float texel = 2.0f * std::tan(fov * 0.5f) / tileSize;
			appendView(plan, nextSlot, ShadowAtlasKind::Local, tiles.front(),
					   settings.localAtlasSize, viewProjection, texel,
					   PERSPECTIVE);
		}
	}
}

}  // namespace

ShadowPlan planShadows(std::vector<LightSnapshot>& lights,
					   const ShadowCameraInfo& camera, const Frustum& frustum,
					   const ShadowSettings& settings) {
	ShadowPlan plan;
	for (LightSnapshot& light : lights) light.shadowMatrixBase = -1;
	plan.gpuData.shadowInfo =
		Vector4(0.0f, 1.0f / settings.directionalAtlasSize,
				1.0f / settings.localAtlasSize, 0.0f);

	uint32_t nextSlot = 0;
	planDirectional(lights, camera, settings, plan, nextSlot);
	planLocal(lights, camera, frustum, settings, plan, nextSlot);
	return plan;
}

}  // namespace bird
