#include "BoneMask.h"

#include <algorithm>
#include <stdexcept>

namespace bird {

namespace {

float clampWeight(float w) {
	return std::clamp(w, 0.0f, 1.0f);
}

}  // namespace

BoneMask::BoneMask(int32_t jointCount, float defaultWeight) {
	if (jointCount < 0) {
		throw std::invalid_argument("BoneMask: jointCount must be >= 0");
	}
	m_weights.assign(static_cast<size_t>(jointCount),
					 clampWeight(defaultWeight));
}

BoneMask BoneMask::all(int32_t jointCount, float weight) {
	return BoneMask(jointCount, weight);
}

BoneMask BoneMask::fromBranch(const Skeleton& skeleton, int32_t rootJoint,
							  float weight) {
	BoneMask mask(skeleton.getJointCount(), 0.0f);
	mask.setBranchWeight(skeleton, rootJoint, weight);
	return mask;
}

BoneMask BoneMask::fromBranch(const Skeleton& skeleton,
							  const std::string& rootJointName, float weight) {
	const int32_t idx = skeleton.findJoint(rootJointName);
	if (idx < 0) {
		throw std::invalid_argument("BoneMask::fromBranch: joint '" +
									rootJointName + "' not found");
	}
	return fromBranch(skeleton, idx, weight);
}

void BoneMask::setJointWeight(int32_t joint, float weight) {
	if (joint < 0 || static_cast<size_t>(joint) >= m_weights.size()) {
		throw std::out_of_range("BoneMask::setJointWeight: invalid joint index");
	}
	m_weights[static_cast<size_t>(joint)] = clampWeight(weight);
}

void BoneMask::setBranchWeight(const Skeleton& skeleton, int32_t rootJoint,
							   float weight) {
	const int32_t count = skeleton.getJointCount();
	if (static_cast<int32_t>(m_weights.size()) != count) {
		throw std::invalid_argument(
			"BoneMask::setBranchWeight: mask size does not match skeleton");
	}
	if (rootJoint < 0 || rootJoint >= count) {
		throw std::out_of_range(
			"BoneMask::setBranchWeight: invalid rootJoint index");
	}
	const float clamped = clampWeight(weight);
	const std::vector<int32_t>& parents = skeleton.getParents();
	std::vector<uint8_t> inBranch(static_cast<size_t>(count), 0);
	inBranch[static_cast<size_t>(rootJoint)] = 1;
	m_weights[static_cast<size_t>(rootJoint)] = clamped;
	// Skeleton is topologically ordered: descendants always appear after rootJoint.
	for (int32_t i = rootJoint + 1; i < count; ++i) {
		const int32_t parent = parents[static_cast<size_t>(i)];
		if (parent >= 0 && inBranch[static_cast<size_t>(parent)]) {
			inBranch[static_cast<size_t>(i)] = 1;
			m_weights[static_cast<size_t>(i)] = clamped;
		}
	}
}

float BoneMask::getJointWeight(int32_t joint) const {
	if (joint < 0 || static_cast<size_t>(joint) >= m_weights.size()) {
		throw std::out_of_range("BoneMask::getJointWeight: invalid joint index");
	}
	return m_weights[static_cast<size_t>(joint)];
}

}  // namespace bird
