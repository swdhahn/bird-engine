#ifndef BIRD_SHADOW_PLANNER_H
#define BIRD_SHADOW_PLANNER_H

#include <cstdint>
#include <vector>

#include "Bounds.h"
#include "GpuLayouts.h"
#include "LightList.h"
#include "ShadowAtlas.h"

namespace bird {

/** Which depth atlas a shadow view renders into. */
enum class ShadowAtlasKind : uint8_t { Directional = 0, Local = 1 };

/** One depth render: a light-space projection into one atlas tile. */
struct ShadowView {
	ShadowAtlasKind atlas = ShadowAtlasKind::Local;
	AtlasTile tile;
	Matrix4 viewProjection{1.0f};
	/** Culling volume for casters. */
	Frustum frustum;
};

/** Camera parameters the planner needs to fit cascades. */
struct ShadowCameraInfo {
	Matrix4 inverseView{1.0f};
	Vector3 position{0.0f};
	float fovY = 1.2f;
	float aspect = 1.0f;
	float nearPlane = 0.1f;
	/** Distance beyond which directional shadows fade out. */
	float shadowDistance = 120.0f;
};

/** Tunables for atlas sizes and allocation. */
struct ShadowSettings {
	uint32_t directionalAtlasSize = 4096;
	uint32_t localAtlasSize = 4096;
	uint32_t minLocalTile = 128;
	uint32_t maxLocalTile = 1024;
	float cascadeLambda = 0.75f;
	/** Distance behind each cascade still captured as casters. */
	float cascadePullback = 150.0f;
};

/** Result of planning: GPU data plus the views to render. */
struct ShadowPlan {
	ShadowUBOData gpuData{};
	std::vector<ShadowView> views;
};

/**
 * Assigns shadow matrices and atlas tiles for one frame.
 *
 * The first shadow-casting directional light receives CASCADE_COUNT
 * cascades in the directional atlas (matrix slots 0..3). Shadow-casting
 * point and spot lights inside the camera frustum are prioritized by
 * proximity and size, then granted tiles from the local atlas: one per spot,
 * six per point. Lights that no longer fit degrade to smaller tiles, then
 * to no shadow. Writes LightSnapshot::shadowMatrixBase for each light.
 *
 * @param lights Frame lights; shadowMatrixBase is overwritten.
 * @param camera Camera parameters.
 * @param frustum Camera frustum.
 * @param settings Atlas and cascade tunables.
 * @return GPU block and views.
 */
ShadowPlan planShadows(std::vector<LightSnapshot>& lights,
					   const ShadowCameraInfo& camera, const Frustum& frustum,
					   const ShadowSettings& settings);

}  // namespace bird

#endif	// BIRD_SHADOW_PLANNER_H
