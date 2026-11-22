#ifndef BIRD_ANIMATION_SKELETALDEBUGDRAW_H
#define BIRD_ANIMATION_SKELETALDEBUGDRAW_H

#include <vector>

#include "../../renders/DebugDraw.h"
#include "../../util/Constants.h"
#include "../../util/Math.h"
#include "Skeleton.h"
#include "TwoBoneIK.h"

namespace bird {

/**
 * Visual configuration for skeletal debug overlays drawn through the engine's
 * 3D DebugDrawList.
 */
struct SkeletalDebugSettings {
	/** Draw bone segments connecting each child joint to its parent. */
	bool drawBones = true;
	/** Draw local RGB XYZ coordinate axes at every joint origin. */
	bool drawJointAxes = false;
	/** Draw markers for enabled 2-Bone IK target and pole positions. */
	bool drawIKTargets = true;
	/** Length of each joint axis line in world units. */
	float jointAxisLength = 0.08f;
	/** Half-extent of the wireframe box drawn at each IK target. */
	float ikTargetSize = 0.06f;
	/** RGBA color of skeleton bone segments. */
	Vector4 boneColor{0.2f, 0.9f, 0.4f, 1.0f};
	/** RGBA color of IK target markers. */
	Vector4 ikTargetColor{1.0f, 0.25f, 0.2f, 1.0f};
	/** RGBA color of IK pole hint markers and guide lines. */
	Vector4 ikPoleColor{0.2f, 0.7f, 1.0f, 1.0f};
};

/**
 * Emits world-space debug lines visualizing the evaluated skeleton hierarchy
 * and active 2-Bone IK targets/poles into a DebugDrawList.
 *
 * @param draw Target DebugDrawList (e.g. GraphicsPipeline::getDebugDraw()).
 * @param skeleton Evaluated skeleton providing parent hierarchy.
 * @param modelMatrices Current model-space joint matrices (from SkeletalAnimator).
 * @param worldTransform Owning WorldObject's world-space transform matrix.
 * @param ikConstraints Optional list of active 2-Bone IK constraints to draw.
 * @param settings Visual options (colors, toggles, marker sizes).
 */
BIRD_NO_EXPORT void drawSkeletonDebug(
	DebugDrawList& draw, const Skeleton& skeleton,
	const std::vector<Matrix4>& modelMatrices, const Matrix4& worldTransform,
	const std::vector<TwoBoneIKConstraint>& ikConstraints = {},
	const SkeletalDebugSettings& settings = {});

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETALDEBUGDRAW_H
