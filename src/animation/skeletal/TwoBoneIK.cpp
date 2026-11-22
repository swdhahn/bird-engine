#include "TwoBoneIK.h"

#include <algorithm>
#include <cmath>

#include "../Interpolation.h"

namespace bird {

namespace {

/**
 * Computes the shortest-arc quaternion rotating unit vector `from` onto unit
 * vector `to`, safely handling parallel and anti-parallel cases.
 */
Quaternion rotationBetween(const Vector3& from, const Vector3& to) {
	const float cosTheta = std::clamp(glm::dot(from, to), -1.0f, 1.0f);
	if (cosTheta >= 1.0f - 1e-6f) {
		return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if (cosTheta <= -1.0f + 1e-6f) {
		// 180-degree rotation around any axis perpendicular to `from`.
		Vector3 axis = glm::cross(Vector3(1.0f, 0.0f, 0.0f), from);
		if (glm::dot(axis, axis) < 1e-6f) {
			axis = glm::cross(Vector3(0.0f, 1.0f, 0.0f), from);
		}
		return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
	}
	const Vector3 axis = glm::cross(from, to);
	const float s = std::sqrt((1.0f + cosTheta) * 2.0f);
	const float invS = 1.0f / s;
	return glm::normalize(
		Quaternion(s * 0.5f, axis.x * invS, axis.y * invS, axis.z * invS));
}

/** Extracts the pure rotation quaternion from an affine 4x4 matrix (strips scale). */
Quaternion extractMatrixRotation(const Matrix4& m) {
	Vector3 c0(m[0]);
	Vector3 c1(m[1]);
	Vector3 c2(m[2]);
	const float l0 = glm::length(c0);
	const float l1 = glm::length(c1);
	const float l2 = glm::length(c2);
	if (l0 > 1e-6f) c0 /= l0;
	if (l1 > 1e-6f) c1 /= l1;
	if (l2 > 1e-6f) c2 /= l2;
	Matrix3 rotMat(c0, c1, c2);
	return glm::normalize(glm::quat_cast(rotMat));
}

/** Recomputes model matrices from startJoint onward in topological order. */
void updateModelMatricesFrom(const Skeleton& skeleton, const Pose& localPose,
							 std::vector<Matrix4>& modelMatrices,
							 int32_t startJoint) {
	const int32_t count = skeleton.getJointCount();
	const std::vector<int32_t>& parents = skeleton.getParents();
	for (int32_t i = startJoint; i < count; ++i) {
		const size_t idx = static_cast<size_t>(i);
		const Matrix4 localMat = localPose.joints[idx].toMatrix();
		const int32_t parent = parents[idx];
		modelMatrices[idx] =
			parent < 0 ? localMat
					   : modelMatrices[static_cast<size_t>(parent)] * localMat;
	}
}

/** Finds any unit vector perpendicular to unit vector `dir`. */
Vector3 arbitraryOrthogonal(const Vector3& dir) {
	const Vector3 ref = std::fabs(dir.y) < 0.9f ? Vector3(0.0f, 1.0f, 0.0f)
												: Vector3(1.0f, 0.0f, 0.0f);
	return glm::normalize(glm::cross(dir, ref));
}

}  // namespace

bool solveTwoBoneIK(const Skeleton& skeleton, Pose& localPose,
					std::vector<Matrix4>& modelMatrices,
					const TwoBoneIKConstraint& constraint) {
	if (!constraint.enabled) return false;
	const float weight = std::clamp(constraint.weight, 0.0f, 1.0f);
	if (weight <= 1e-6f) return false;

	const int32_t jointCount = skeleton.getJointCount();
	if (static_cast<int32_t>(localPose.joints.size()) != jointCount ||
		static_cast<int32_t>(modelMatrices.size()) != jointCount) {
		return false;
	}
	if (constraint.rootJoint < 0 || constraint.rootJoint >= jointCount ||
		constraint.midJoint < 0 || constraint.midJoint >= jointCount ||
		constraint.endJoint < 0 || constraint.endJoint >= jointCount) {
		return false;
	}
	if (constraint.rootJoint >= constraint.midJoint ||
		constraint.midJoint >= constraint.endJoint) {
		return false;
	}

	const size_t rootIdx = static_cast<size_t>(constraint.rootJoint);
	const size_t midIdx = static_cast<size_t>(constraint.midJoint);
	const size_t endIdx = static_cast<size_t>(constraint.endJoint);

	const Vector3 A(modelMatrices[rootIdx][3]);
	const Vector3 B(modelMatrices[midIdx][3]);
	const Vector3 C(modelMatrices[endIdx][3]);
	const Vector3 T = constraint.targetPosition;

	const Vector3 ab = B - A;
	const Vector3 bc = C - B;
	const float len1 = glm::length(ab);
	const float len2 = glm::length(bc);
	if (len1 <= 1e-5f || len2 <= 1e-5f) return false;

	const Vector3 at = T - A;
	const float targetDist = glm::length(at);
	const float minReach = std::fabs(len1 - len2) + 1e-4f;
	const float maxReach = len1 + len2 - 1e-4f;
	if (maxReach <= minReach) return false;

	const float d = std::clamp(targetDist, minReach, maxReach);
	const Vector3 dirAT =
		targetDist > 1e-5f ? (at / targetDist) : (ab / len1);

	// Law of Cosines: interior angle alpha at rootJoint between A->T and A->B.
	const float cosAlpha =
		std::clamp((len1 * len1 + d * d - len2 * len2) / (2.0f * len1 * d),
				   -1.0f, 1.0f);
	const float sinAlpha = std::sqrt(std::max(0.0f, 1.0f - cosAlpha * cosAlpha));

	// Determine bend direction perpendicular to dirAT inside the IK plane.
	Vector3 bendDir(0.0f);
	bool haveBendDir = false;
	if (constraint.usePolePosition) {
		const Vector3 poleVec = constraint.polePosition - A;
		const Vector3 poleProj = poleVec - dirAT * glm::dot(poleVec, dirAT);
		const float poleLen = glm::length(poleProj);
		if (poleLen > 1e-5f) {
			bendDir = poleProj / poleLen;
			haveBendDir = true;
		}
	}
	if (!haveBendDir) {
		const Vector3 fkProj = ab - dirAT * glm::dot(ab, dirAT);
		const float fkLen = glm::length(fkProj);
		if (fkLen > 1e-5f) {
			bendDir = fkProj / fkLen;
		} else {
			bendDir = arbitraryOrthogonal(dirAT);
		}
	}

	const Vector3 solvedB = A + (dirAT * cosAlpha + bendDir * sinAlpha) * len1;
	const Vector3 solvedC = A + dirAT * d;

	const Quaternion origRootLocal = localPose.joints[rootIdx].rotation;
	const Quaternion origMidLocal = localPose.joints[midIdx].rotation;

	// Step 1: Solve rootJoint rotation so A->B points toward solvedB.
	const Vector3 origDirAB = ab / len1;
	const Vector3 solvedDirAB = glm::normalize(solvedB - A);
	const Quaternion deltaRootModel = rotationBetween(origDirAB, solvedDirAB);
	const Quaternion currRootModelRot =
		extractMatrixRotation(modelMatrices[rootIdx]);
	const Quaternion solvedRootModelRot =
		glm::normalize(deltaRootModel * currRootModelRot);

	const int32_t rootParent = skeleton.getParent(constraint.rootJoint);
	const Quaternion rootParentRot =
		rootParent >= 0
			? extractMatrixRotation(
				  modelMatrices[static_cast<size_t>(rootParent)])
			: Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
	const Quaternion solvedRootLocal =
		glm::normalize(glm::inverse(rootParentRot) * solvedRootModelRot);

	// Temporarily apply full root rotation to evaluate exact mid/end positions.
	localPose.joints[rootIdx].rotation = solvedRootLocal;
	updateModelMatricesFrom(skeleton, localPose, modelMatrices,
							constraint.rootJoint);

	// Step 2: Solve midJoint rotation so B->C points toward solvedC.
	const Vector3 curB(modelMatrices[midIdx][3]);
	const Vector3 curC(modelMatrices[endIdx][3]);
	const Vector3 curBC = curC - curB;
	const Vector3 targetBC = solvedC - curB;
	Quaternion solvedMidLocal = origMidLocal;
	if (glm::length(curBC) > 1e-5f && glm::length(targetBC) > 1e-5f) {
		const Quaternion deltaMidModel =
			rotationBetween(glm::normalize(curBC), glm::normalize(targetBC));
		const Quaternion currMidModelRot =
			extractMatrixRotation(modelMatrices[midIdx]);
		const Quaternion solvedMidModelRot =
			glm::normalize(deltaMidModel * currMidModelRot);
		const int32_t midParent = skeleton.getParent(constraint.midJoint);
		const Quaternion midParentRot =
			midParent >= 0
				? extractMatrixRotation(
					  modelMatrices[static_cast<size_t>(midParent)])
				: Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
		solvedMidLocal =
			glm::normalize(glm::inverse(midParentRot) * solvedMidModelRot);
	}

	// Step 3: Blend FK and IK local rotations by constraint.weight and update.
	if (weight < 1.0f) {
		localPose.joints[rootIdx].rotation =
			interpolate(origRootLocal, solvedRootLocal, weight);
		localPose.joints[midIdx].rotation =
			interpolate(origMidLocal, solvedMidLocal, weight);
	} else {
		localPose.joints[rootIdx].rotation = solvedRootLocal;
		localPose.joints[midIdx].rotation = solvedMidLocal;
	}

	updateModelMatricesFrom(skeleton, localPose, modelMatrices,
							constraint.rootJoint);
	return true;
}

}  // namespace bird
