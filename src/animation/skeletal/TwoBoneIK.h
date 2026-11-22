#ifndef BIRD_ANIMATION_TWOBONEIK_H
#define BIRD_ANIMATION_TWOBONEIK_H

#include <cstdint>
#include <string>
#include <vector>

#include "../../util/Constants.h"
#include "../../util/Math.h"
#include "Skeleton.h"

namespace bird {

/**
 * Configuration for a single 2-Bone Analytic Inverse Kinematics chain
 * (e.g. hip -> knee -> ankle for foot placement, or shoulder -> elbow -> wrist
 * for hand reach).
 *
 * All target and pole coordinates are expressed in the character's model space
 * (the same space as Skeleton/SkeletalAnimator model matrices).
 */
struct TwoBoneIKConstraint {
	/** Optional identifier (e.g. "LeftFoot", "RightHand") for lookup/updates. */
	std::string name;
	/** Index of the upper limb joint (hip / shoulder). */
	int32_t rootJoint = -1;
	/** Index of the middle hinge joint (knee / elbow); child of rootJoint. */
	int32_t midJoint = -1;
	/** Index of the end effector joint (ankle / wrist); child of midJoint. */
	int32_t endJoint = -1;
	/** Desired model-space position of endJoint. */
	Vector3 targetPosition{0.0f};
	/**
	 * Model-space pole hint controlling the direction the middle joint bends
	 * toward (e.g. knee forward, elbow backward).
	 */
	Vector3 polePosition{0.0f, 0.0f, 1.0f};
	/** When false, preserves the current FK bend plane instead of using polePosition. */
	bool usePolePosition = true;
	/** Blend weight in [0.0, 1.0] between the FK pose (0) and IK solution (1). */
	float weight = 1.0f;
	/** Toggle to enable or disable solving this constraint. */
	bool enabled = true;
};

/**
 * Solves a 2-Bone Analytic IK constraint using the Law of Cosines.
 *
 * Runs post-pose-evaluation and pre-skinning: modifies the local rotations of
 * rootJoint and midJoint in localPose (blended by constraint.weight) and
 * updates modelMatrices in place so subsequent constraints and
 * buildSkinningPalette() observe the solved transforms.
 *
 * @param skeleton Target skeleton (topologically ordered).
 * @param localPose Joint-local pose to modify in place.
 * @param modelMatrices Current model-space matrices (updated in place).
 * @param constraint 2-bone chain definition and target/pole positions.
 * @return true if the constraint was valid, enabled, and solved; false if
 *         skipped (disabled, zero weight, invalid indices, or degenerate limb).
 */
BIRD_NO_EXPORT bool solveTwoBoneIK(const Skeleton& skeleton, Pose& localPose,
								   std::vector<Matrix4>& modelMatrices,
								   const TwoBoneIKConstraint& constraint);

}  // namespace bird

#endif	// BIRD_ANIMATION_TWOBONEIK_H
