#include "Skeleton.h"

#include <stdexcept>

namespace bird {

Matrix4 JointTransform::toMatrix() const {
	Matrix4 m = glm::mat4_cast(rotation);
	m[0] *= scale.x;
	m[1] *= scale.y;
	m[2] *= scale.z;
	m[3] = Vector4(translation, 1.0f);
	return m;
}

int32_t Skeleton::addJoint(const std::string& name, int32_t parent,
						   const Matrix4& inverseBind,
						   const JointTransform& restPose) {
	if (name.empty()) {
		throw std::invalid_argument("Skeleton::addJoint: empty joint name");
	}
	if (m_byName.count(name)) {
		throw std::invalid_argument("Skeleton::addJoint: duplicate joint '" +
									name + "'");
	}
	const int32_t index = getJointCount();
	if (parent < -1 || parent >= index) {
		throw std::invalid_argument(
			"Skeleton::addJoint: parent of '" + name +
			"' must be -1 or an earlier joint (topological order)");
	}
	if (static_cast<uint32_t>(index) >= kMaxJoints) {
		throw std::invalid_argument("Skeleton::addJoint: more than " +
									std::to_string(kMaxJoints) + " joints");
	}
	m_names.push_back(name);
	m_parents.push_back(parent);
	m_inverseBind.push_back(inverseBind);
	m_restPose.joints.push_back(restPose);
	m_byName.emplace(name, index);
	return index;
}

int32_t Skeleton::findJoint(const std::string& name) const {
	const auto it = m_byName.find(name);
	return it == m_byName.end() ? -1 : it->second;
}

const std::string& Skeleton::getJointName(int32_t index) const {
	if (index < 0 || index >= getJointCount()) {
		throw std::out_of_range("Skeleton::getJointName: bad joint index");
	}
	return m_names[static_cast<size_t>(index)];
}

int32_t Skeleton::getParent(int32_t index) const {
	if (index < 0 || index >= getJointCount()) {
		throw std::out_of_range("Skeleton::getParent: bad joint index");
	}
	return m_parents[static_cast<size_t>(index)];
}

}  // namespace bird
