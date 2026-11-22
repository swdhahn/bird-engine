#ifndef BIRD_ANIMATION_BONEMASK_H
#define BIRD_ANIMATION_BONEMASK_H

#include <cstdint>
#include <string>
#include <vector>

#include "../../util/Constants.h"
#include "Skeleton.h"

namespace bird {

/**
 * Per-joint blend weight array in [0.0, 1.0], indexed identically to a
 * Skeleton's joints.
 *
 * Used by animation layers and masked pose blending so partial-body
 * animations (such as upper-body aiming or reloading) can layer cleanly over
 * lower-body locomotion. A weight of 0.0 keeps the base pose completely; 1.0
 * applies the full layer weight.
 *
 * @code
 * BoneMask upperBody = BoneMask::fromBranch(*skeleton, "Spine1", 1.0f);
 * upperBody.setJointWeight(skeleton->findJoint("Spine"), 0.5f);
 * @endcode
 */
class BoneMask {
   public:
	BoneMask() = default;

	/**
	 * Creates a uniform mask with jointCount entries set to defaultWeight.
	 * @param jointCount Number of joints in the target skeleton.
	 * @param defaultWeight Initial weight clamped to [0.0, 1.0].
	 */
	explicit BoneMask(int32_t jointCount, float defaultWeight = 1.0f);

	/**
	 * Builds a uniform mask for the entire skeleton.
	 * @param jointCount Number of joints.
	 * @param weight Weight clamped to [0.0, 1.0].
	 * @return A BoneMask with every joint set to weight.
	 */
	static BoneMask all(int32_t jointCount, float weight = 1.0f);

	/**
	 * Builds a mask where rootJoint and all of its descendants have the given
	 * weight, and every other joint has weight 0.0.
	 *
	 * Runs in a single O(joints) forward pass using the skeleton's topological
	 * ordering.
	 *
	 * @param skeleton Target skeleton.
	 * @param rootJoint Index of the branch root joint.
	 * @param weight Weight for the branch, clamped to [0.0, 1.0].
	 * @return The constructed branch mask.
	 * @throws std::out_of_range when rootJoint is not a valid joint index.
	 */
	BIRD_NO_EXPORT static BoneMask fromBranch(const Skeleton& skeleton,
											  int32_t rootJoint,
											  float weight = 1.0f);

	/**
	 * Convenience overload of fromBranch() that looks up rootJoint by name.
	 * @param skeleton Target skeleton.
	 * @param rootJointName Name of the branch root joint.
	 * @param weight Weight for the branch, clamped to [0.0, 1.0].
	 * @return The constructed branch mask.
	 * @throws std::invalid_argument when the joint name is not found.
	 */
	BIRD_NO_EXPORT static BoneMask fromBranch(const Skeleton& skeleton,
											  const std::string& rootJointName,
											  float weight = 1.0f);

	/**
	 * Sets the blend weight for a single joint, clamped to [0.0, 1.0].
	 * @param joint Joint index.
	 * @param weight Desired weight.
	 * @throws std::out_of_range when joint is out of bounds.
	 */
	void setJointWeight(int32_t joint, float weight);

	/**
	 * Sets the blend weight for rootJoint and all of its descendants.
	 * @param skeleton Target skeleton (must match this mask's joint count).
	 * @param rootJoint Branch root joint index.
	 * @param weight Desired weight, clamped to [0.0, 1.0].
	 * @throws std::invalid_argument on skeleton/mask size mismatch.
	 * @throws std::out_of_range when rootJoint is invalid.
	 */
	BIRD_NO_EXPORT void setBranchWeight(const Skeleton& skeleton,
										int32_t rootJoint, float weight);

	/**
	 * Returns the weight for a single joint.
	 * @param joint Joint index.
	 * @return Weight in [0.0, 1.0].
	 * @throws std::out_of_range when joint is out of bounds.
	 */
	float getJointWeight(int32_t joint) const;

	/** @return Number of joints represented by this mask (0 if empty). */
	int32_t getJointCount() const {
		return static_cast<int32_t>(m_weights.size());
	}

	/** @return true when no per-joint weights are stored (treated as all 1.0). */
	bool isEmpty() const { return m_weights.empty(); }

	BIRD_NO_EXPORT const std::vector<float>& getWeights() const {
		return m_weights;
	}

   private:
	std::vector<float> m_weights;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_BONEMASK_H
