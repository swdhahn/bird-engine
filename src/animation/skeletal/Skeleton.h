#ifndef BIRD_ANIMATION_SKELETON_H
#define BIRD_ANIMATION_SKELETON_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../util/Constants.h"
#include "../../util/Math.h"

namespace bird {

/** Local translation, rotation and scale of one joint (TRS). */
struct JointTransform {
	Vector3 translation{0.0f};
	Quaternion rotation{1.0f, 0.0f, 0.0f, 0.0f};
	Vector3 scale{1.0f};

	/** Composes T * R * S. */
	Matrix4 toMatrix() const;
};

/**
 * A full set of joint-local transforms, indexed like the Skeleton's
 * joints. Stored as an array of TRS structures: blending and sampling walk
 * it linearly, so it stays cache-friendly without SIMD-oriented layouts.
 */
struct Pose {
	std::vector<JointTransform> joints;
};

/**
 * Immutable-after-build joint hierarchy shared by every animated instance
 * of a model.
 *
 * Joints are stored in topological order (every parent precedes its
 * children), so local-to-model conversion is one forward O(n) pass with
 * no recursion. addJoint() enforces that order.
 */
class Skeleton {
   public:
	/** Upper bound matching the GPU skinning palette (see GpuLayouts.h). */
	static constexpr uint32_t kMaxJoints = 256;

	/**
	 * Appends a joint.
	 *
	 * @param name Unique joint name (animation channels bind by name).
	 * @param parent Index of an earlier joint, or -1 for a root.
	 * @param inverseBind Mesh-space to joint-space matrix at bind time.
	 * @param restPose Local transform used when no clip animates the joint.
	 * @return Index of the new joint.
	 * @throws std::invalid_argument on a duplicate or empty name, a parent
	 *         that is not an earlier joint, or exceeding kMaxJoints.
	 */
	int32_t addJoint(const std::string& name, int32_t parent,
					 const Matrix4& inverseBind,
					 const JointTransform& restPose);

	/** @return the joint index, or -1 when no joint has that name. */
	int32_t findJoint(const std::string& name) const;

	int32_t getJointCount() const {
		return static_cast<int32_t>(m_parents.size());
	}
	/** @throws std::out_of_range for an invalid index. */
	const std::string& getJointName(int32_t index) const;
	/** @throws std::out_of_range for an invalid index. */
	int32_t getParent(int32_t index) const;

	BIRD_NO_EXPORT const std::vector<int32_t>& getParents() const {
		return m_parents;
	}
	BIRD_NO_EXPORT const std::vector<Matrix4>& getInverseBindMatrices() const {
		return m_inverseBind;
	}
	BIRD_NO_EXPORT const Pose& getRestPose() const { return m_restPose; }

   private:
	std::vector<std::string> m_names;
	std::vector<int32_t> m_parents;
	std::vector<Matrix4> m_inverseBind;
	Pose m_restPose;
	std::unordered_map<std::string, int32_t> m_byName;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETON_H
