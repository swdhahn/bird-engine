#ifndef BIRD_ANIMATION_ROOTMOTION_H
#define BIRD_ANIMATION_ROOTMOTION_H

#include <cstdint>

#include "../../util/Constants.h"
#include "../../util/Math.h"
#include "Skeleton.h"

namespace bird {

/**
 * Configures which channels of a designated root joint are extracted as
 * per-frame root motion and stripped from the evaluated Pose.
 */
struct RootMotionConfig {
	/** Master toggle for root motion extraction. */
	bool enabled = false;
	/** Index of the root joint whose motion is extracted (default 0). */
	int32_t rootJoint = 0;
	/** Extract horizontal X translation delta and zero X offset in the pose. */
	bool extractTranslationX = true;
	/** Extract vertical Y translation delta (off by default so gravity owns Y). */
	bool extractTranslationY = false;
	/** Extract horizontal Z translation delta and zero Z offset in the pose. */
	bool extractTranslationZ = true;
	/** Optionally extract yaw rotation around +Y and strip it from the root joint. */
	bool extractRotationY = false;
};

/**
 * Per-frame root motion displacement extracted from the evaluated pose, in the
 * character's local (model) space.
 *
 * Consumers (entity transforms or CharacterControllerComponent) transform
 * `translation` by the entity's world rotation and apply it to move the
 * character without double-translating the visual mesh.
 */
struct RootMotionDelta {
	/** Model-space root translation delta for the current frame. */
	Vector3 translation{0.0f};
	/** Optional yaw rotation delta around +Y for the current frame. */
	Quaternion rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * Stateful root motion extractor that tracks the root joint's transform across
 * frames, computes the per-frame delta (handling loop wrap-around), and zeros
 * out the extracted channels in the Pose so the skinned mesh stays centered on
 * its entity origin.
 */
class BIRD_NO_EXPORT RootMotionExtractor {
   public:
	RootMotionExtractor() = default;

	/** Updates extraction settings; resets cached state if rootJoint changes. */
	void setConfig(const RootMotionConfig& config);
	const RootMotionConfig& getConfig() const { return m_config; }

	/**
	 * Clears the previous-frame reference (call on clip cuts or state resets so
	 * the next frame does not emit a teleportation delta).
	 */
	void reset();

	/**
	 * Extracts root motion from localPose, mutating localPose in place to zero
	 * out the extracted channels relative to the skeleton's rest pose.
	 *
	 * @param skeleton Target skeleton (provides rest-pose root reference).
	 * @param localPose Evaluated local pose for this frame (modified in place).
	 * @param loopWrapped True when the driving clip wrapped across a loop boundary.
	 * @return The extracted RootMotionDelta for this frame.
	 */
	RootMotionDelta extract(const Skeleton& skeleton, Pose& localPose,
							bool loopWrapped = false);

	/** @return Most recently extracted RootMotionDelta. */
	const RootMotionDelta& getLastDelta() const { return m_lastDelta; }

   private:
	RootMotionConfig m_config;
	bool m_hasPrevious = false;
	Vector3 m_prevTranslation{0.0f};
	float m_prevYaw = 0.0f;
	RootMotionDelta m_lastDelta;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_ROOTMOTION_H
