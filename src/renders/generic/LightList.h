#ifndef BIRD_LIGHT_LIST_H
#define BIRD_LIGHT_LIST_H

#include <cstdint>
#include <vector>

#include "../../scene/components/LightComponent.h"
#include "Bounds.h"
#include "GpuLayouts.h"

namespace bird {

/**
 * Immutable per-frame copy of a light's world-space state. Decouples the
 * renderer and its tests from the scene graph.
 */
struct LightSnapshot {
	LightType type = LightType::Point;
	Vector3 position{0.0f};
	Vector3 direction{0.0f, -1.0f, 0.0f};
	Vector3 color{1.0f};
	float intensity = 1.0f;
	float range = 10.0f;
	float innerConeAngle = 0.35f;
	float outerConeAngle = 0.5f;
	bool castsShadows = false;
	float shadowDepthBias = 1.0f;
	float shadowNormalBias = 1.5f;
	/** First shadow matrix index, assigned by the shadow planner; -1 none. */
	int32_t shadowMatrixBase = -1;
};

/** Captures a component's state; requires an attached world object. */
LightSnapshot snapshotLight(LightComponent& light);

/** Packs a snapshot into the std140 shader representation. */
GpuLight toGpuLight(const LightSnapshot& light);

/**
 * Chooses the lights a forward-shaded fragment evaluates: every directional
 * light first (input order), then visible local lights nearest the camera
 * first, truncated to maxLights. O(n log n).
 * @param lights All lights this frame.
 * @param cameraPosition World-space camera position.
 * @param frustum Camera frustum; local lights outside are skipped.
 * @param maxLights Capacity of the forward light block.
 * @return Indices into lights.
 */
std::vector<size_t> selectForwardLights(const std::vector<LightSnapshot>& lights,
										const Vector3& cameraPosition,
										const Frustum& frustum,
										uint32_t maxLights);

/**
 * Fills the forward light block from selected indices.
 * @return The block with counts.x = total and counts.y = directional count.
 */
LightsUBOData buildLightsBlock(const std::vector<LightSnapshot>& lights,
							   const std::vector<size_t>& selection);

}  // namespace bird

#endif	// BIRD_LIGHT_LIST_H
