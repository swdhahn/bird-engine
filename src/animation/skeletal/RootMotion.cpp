#include "RootMotion.h"

#include <cmath>

namespace bird {

namespace {

/** Extracts yaw (angle around +Y in radians) from a unit quaternion. */
float extractYaw(const Quaternion& q) {
	// Forward vector (0, 0, 1) rotated by q projected onto XZ plane.
	const Vector3 fwd = q * Vector3(0.0f, 0.0f, 1.0f);
	if (fwd.x * fwd.x + fwd.z * fwd.z <= 1e-8f) return 0.0f;
	return std::atan2(fwd.x, fwd.z);
}

/** Wraps an angle difference into [-pi, pi]. */
float wrapAngle(float radians) {
	const float pi = glm::pi<float>();
	const float twoPi = 2.0f * pi;
	radians = std::fmod(radians + pi, twoPi);
	if (radians < 0.0f) radians += twoPi;
	return radians - pi;
}

}  // namespace

void RootMotionExtractor::setConfig(const RootMotionConfig& config) {
	if (config.rootJoint != m_config.rootJoint ||
		config.enabled != m_config.enabled) {
		reset();
	}
	m_config = config;
}

void RootMotionExtractor::reset() {
	m_hasPrevious = false;
	m_prevTranslation = Vector3(0.0f);
	m_prevYaw = 0.0f;
	m_lastDelta = RootMotionDelta{};
}

RootMotionDelta RootMotionExtractor::extract(const Skeleton& skeleton,
											 Pose& localPose,
											 bool loopWrapped) {
	m_lastDelta = RootMotionDelta{};
	if (!m_config.enabled) return m_lastDelta;

	const int32_t count = skeleton.getJointCount();
	if (m_config.rootJoint < 0 || m_config.rootJoint >= count ||
		static_cast<int32_t>(localPose.joints.size()) != count) {
		return m_lastDelta;
	}

	const size_t idx = static_cast<size_t>(m_config.rootJoint);
	JointTransform& root = localPose.joints[idx];
	const JointTransform& rest = skeleton.getRestPose().joints[idx];

	const Vector3 currTrans = root.translation;
	const float currYaw = extractYaw(root.rotation);

	if (m_hasPrevious) {
		const Vector3 refTrans = loopWrapped ? rest.translation : m_prevTranslation;
		Vector3 rawDelta = currTrans - refTrans;
		if (!m_config.extractTranslationX) rawDelta.x = 0.0f;
		if (!m_config.extractTranslationY) rawDelta.y = 0.0f;
		if (!m_config.extractTranslationZ) rawDelta.z = 0.0f;
		m_lastDelta.translation = rawDelta;

		if (m_config.extractRotationY) {
			const float refYaw =
				loopWrapped ? extractYaw(rest.rotation) : m_prevYaw;
			const float deltaYaw = wrapAngle(currYaw - refYaw);
			m_lastDelta.rotation =
				glm::angleAxis(deltaYaw, Vector3(0.0f, 1.0f, 0.0f));
		}
	}

	m_prevTranslation = currTrans;
	m_prevYaw = currYaw;
	m_hasPrevious = true;

	// Strip extracted translation axes back to rest pose so the mesh does not
	// drift away from the entity's transform.
	if (m_config.extractTranslationX) root.translation.x = rest.translation.x;
	if (m_config.extractTranslationY) root.translation.y = rest.translation.y;
	if (m_config.extractTranslationZ) root.translation.z = rest.translation.z;

	if (m_config.extractRotationY) {
		const Quaternion invYaw =
			glm::angleAxis(-currYaw, Vector3(0.0f, 1.0f, 0.0f));
		root.rotation = glm::normalize(invYaw * root.rotation);
	}

	return m_lastDelta;
}

}  // namespace bird
