#include "PoseOps.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "../Interpolation.h"

namespace bird {

namespace {

Quaternion nlerpShortest(const Quaternion& from, const Quaternion& to,
						 float weight) {
	const Quaternion target = glm::dot(from, to) < 0.0f ? -to : to;
	const Quaternion blended =
		from * (1.0f - weight) + target * weight;
	const float lenSq = glm::dot(blended, blended);
	if (lenSq <= 1e-12f) return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
	return blended * (1.0f / std::sqrt(lenSq));
}

float safeDivide(float num, float den) {
	return std::fabs(den) > 1e-6f ? (num / den) : 1.0f;
}

}  // namespace

void blendPoses(const Pose& from, const Pose& to, float weight, Pose& out) {
	const size_t count = from.joints.size();
	if (to.joints.size() != count) {
		throw std::invalid_argument("blendPoses: pose sizes differ");
	}
	out.joints.resize(count);
	for (size_t i = 0; i < count; ++i) {
		const JointTransform& a = from.joints[i];
		const JointTransform& b = to.joints[i];
		JointTransform& r = out.joints[i];
		r.translation = interpolate(a.translation, b.translation, weight);
		r.rotation = interpolate(a.rotation, b.rotation, weight);
		r.scale = interpolate(a.scale, b.scale, weight);
	}
}

void blendPosesNlerp(const Pose& from, const Pose& to, float weight,
					 Pose& out) {
	const size_t count = from.joints.size();
	if (to.joints.size() != count) {
		throw std::invalid_argument("blendPosesNlerp: pose sizes differ");
	}
	out.joints.resize(count);
	for (size_t i = 0; i < count; ++i) {
		const JointTransform& a = from.joints[i];
		const JointTransform& b = to.joints[i];
		JointTransform& r = out.joints[i];
		r.translation = interpolate(a.translation, b.translation, weight);
		r.rotation = nlerpShortest(a.rotation, b.rotation, weight);
		r.scale = interpolate(a.scale, b.scale, weight);
	}
}

void blendPosesMasked(const Pose& base, const Pose& overlay,
					  const BoneMask& mask, float weight, Pose& out) {
	const size_t count = base.joints.size();
	if (overlay.joints.size() != count) {
		throw std::invalid_argument("blendPosesMasked: pose sizes differ");
	}
	if (mask.isEmpty()) {
		blendPoses(base, overlay, weight, out);
		return;
	}
	const std::vector<float>& weights = mask.getWeights();
	if (weights.size() != count) {
		throw std::invalid_argument(
			"blendPosesMasked: mask size does not match pose");
	}
	out.joints.resize(count);
	for (size_t i = 0; i < count; ++i) {
		const float w = weights[i] * weight;
		const JointTransform& a = base.joints[i];
		if (w <= 0.0f) {
			out.joints[i] = a;
		} else if (w >= 1.0f) {
			out.joints[i] = overlay.joints[i];
		} else {
			const JointTransform& b = overlay.joints[i];
			JointTransform& r = out.joints[i];
			r.translation = interpolate(a.translation, b.translation, w);
			r.rotation = interpolate(a.rotation, b.rotation, w);
			r.scale = interpolate(a.scale, b.scale, w);
		}
	}
}

void blendPosesWeighted(const Pose* const* poses, const float* weights,
						size_t count, Pose& out) {
	if (!poses || !weights || count == 0) {
		throw std::invalid_argument("blendPosesWeighted: empty pose array");
	}
	float totalWeight = 0.0f;
	size_t refIndex = count;
	const size_t jointCount = poses[0] ? poses[0]->joints.size() : 0;
	for (size_t p = 0; p < count; ++p) {
		if (!poses[p]) {
			throw std::invalid_argument("blendPosesWeighted: null pose entry");
		}
		if (poses[p]->joints.size() != jointCount) {
			throw std::invalid_argument("blendPosesWeighted: pose sizes differ");
		}
		if (weights[p] < 0.0f || !std::isfinite(weights[p])) {
			throw std::invalid_argument(
				"blendPosesWeighted: weights must be finite and >= 0");
		}
		if (weights[p] > 0.0f && refIndex == count) {
			refIndex = p;
		}
		totalWeight += weights[p];
	}
	if (totalWeight <= 1e-8f || refIndex == count) {
		throw std::invalid_argument(
			"blendPosesWeighted: total weight must be > 0");
	}
	const float invTotal = 1.0f / totalWeight;
	out.joints.resize(jointCount);
	for (size_t j = 0; j < jointCount; ++j) {
		Vector3 trans(0.0f);
		Vector3 scl(0.0f);
		Quaternion rotSum(0.0f, 0.0f, 0.0f, 0.0f);
		const Quaternion& refRot = poses[refIndex]->joints[j].rotation;
		for (size_t p = 0; p < count; ++p) {
			const float w = weights[p] * invTotal;
			if (w <= 0.0f) continue;
			const JointTransform& jt = poses[p]->joints[j];
			trans += jt.translation * w;
			scl += jt.scale * w;
			const Quaternion q =
				glm::dot(refRot, jt.rotation) < 0.0f ? -jt.rotation : jt.rotation;
			rotSum += q * w;
		}
		JointTransform& r = out.joints[j];
		r.translation = trans;
		r.scale = scl;
		const float lenSq = glm::dot(rotSum, rotSum);
		r.rotation = lenSq > 1e-12f ? (rotSum * (1.0f / std::sqrt(lenSq)))
									: Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
	}
}

void makeAdditivePose(const Pose& target, const Pose& reference, Pose& out) {
	const size_t count = target.joints.size();
	if (reference.joints.size() != count) {
		throw std::invalid_argument("makeAdditivePose: pose sizes differ");
	}
	out.joints.resize(count);
	for (size_t i = 0; i < count; ++i) {
		const JointTransform& t = target.joints[i];
		const JointTransform& r = reference.joints[i];
		JointTransform& d = out.joints[i];
		d.translation = t.translation - r.translation;
		d.rotation = glm::normalize(t.rotation * glm::inverse(r.rotation));
		d.scale = Vector3(safeDivide(t.scale.x, r.scale.x),
						  safeDivide(t.scale.y, r.scale.y),
						  safeDivide(t.scale.z, r.scale.z));
	}
}

void applyAdditivePose(const Pose& base, const Pose& additive, float weight,
					   Pose& out) {
	const BoneMask emptyMask;
	applyAdditivePoseMasked(base, additive, emptyMask, weight, out);
}

void applyAdditivePoseMasked(const Pose& base, const Pose& additive,
							 const BoneMask& mask, float weight, Pose& out) {
	const size_t count = base.joints.size();
	if (additive.joints.size() != count) {
		throw std::invalid_argument(
			"applyAdditivePoseMasked: pose sizes differ");
	}
	const bool hasMask = !mask.isEmpty();
	if (hasMask && static_cast<size_t>(mask.getJointCount()) != count) {
		throw std::invalid_argument(
			"applyAdditivePoseMasked: mask size does not match pose");
	}
	const std::vector<float>& weights = mask.getWeights();
	const Quaternion identity(1.0f, 0.0f, 0.0f, 0.0f);
	out.joints.resize(count);
	for (size_t i = 0; i < count; ++i) {
		const float w = hasMask ? (weights[i] * weight) : weight;
		const JointTransform& b = base.joints[i];
		if (w == 0.0f) {
			out.joints[i] = b;
			continue;
		}
		const JointTransform& a = additive.joints[i];
		JointTransform& r = out.joints[i];
		r.translation = b.translation + a.translation * w;
		const Quaternion deltaRot = interpolate(identity, a.rotation, w);
		r.rotation = glm::normalize(deltaRot * b.rotation);
		r.scale = b.scale * interpolate(Vector3(1.0f), a.scale, w);
	}
}

void localToModel(const Skeleton& skeleton, const Pose& local,
				  std::vector<Matrix4>& model) {
	const size_t count = static_cast<size_t>(skeleton.getJointCount());
	if (local.joints.size() != count) {
		throw std::invalid_argument("localToModel: pose/skeleton mismatch");
	}
	model.resize(count);
	const std::vector<int32_t>& parents = skeleton.getParents();
	for (size_t i = 0; i < count; ++i) {
		const Matrix4 joint = local.joints[i].toMatrix();
		const int32_t parent = parents[i];
		// Topological order guarantees model[parent] is already final.
		model[i] = parent < 0 ? joint
							  : model[static_cast<size_t>(parent)] * joint;
	}
}

Matrix3x4 toPaletteEntry(const Matrix4& m) {
	return Matrix3x4(Vector4(m[0][0], m[1][0], m[2][0], m[3][0]),
					 Vector4(m[0][1], m[1][1], m[2][1], m[3][1]),
					 Vector4(m[0][2], m[1][2], m[2][2], m[3][2]));
}

std::vector<float> computeJointRadii(const Skeleton& skeleton,
									 const SkinStream& skin,
									 const float* positions,
									 size_t vertexCount) {
	if (skin.joints.size() != vertexCount || skin.weights.size() != vertexCount) {
		throw std::invalid_argument(
			"computeJointRadii: skin does not match the vertex count");
	}
	const std::vector<Matrix4>& inverseBind = skeleton.getInverseBindMatrices();
	std::vector<float> radii(inverseBind.size(), 0.0f);
	for (size_t v = 0; v < vertexCount; ++v) {
		const Vector4 position(positions[v * 3], positions[v * 3 + 1],
							   positions[v * 3 + 2], 1.0f);
		for (int k = 0; k < 4; ++k) {
			if (skin.weights[v][k] <= 0.0f) continue;
			const size_t joint = skin.joints[v][k];
			if (joint >= radii.size()) {
				throw std::invalid_argument(
					"computeJointRadii: joint index outside the skeleton");
			}
			const float distance =
				glm::length(Vector3(inverseBind[joint] * position));
			radii[joint] = std::max(radii[joint], distance);
		}
	}
	return radii;
}

AABB skinnedBounds(const std::vector<Matrix4>& jointModel,
				   const std::vector<float>& jointRadii) {
	AABB box;
	const size_t count = std::min(jointModel.size(), jointRadii.size());
	for (size_t j = 0; j < count; ++j) {
		if (jointRadii[j] <= 0.0f) continue;
		const Matrix4& m = jointModel[j];
		const float scale = std::max({glm::length(Vector3(m[0])),
									  glm::length(Vector3(m[1])),
									  glm::length(Vector3(m[2]))});
		const Vector3 origin(m[3]);
		const Vector3 reach(jointRadii[j] * scale);
		box.expand(origin - reach);
		box.expand(origin + reach);
	}
	return box;
}

void buildSkinningPalette(const Skeleton& skeleton,
						  const std::vector<Matrix4>& model,
						  std::vector<Matrix3x4>& palette) {
	const std::vector<Matrix4>& inverseBind = skeleton.getInverseBindMatrices();
	if (model.size() != inverseBind.size()) {
		throw std::invalid_argument(
			"buildSkinningPalette: matrix/skeleton mismatch");
	}
	palette.resize(model.size());
	for (size_t i = 0; i < model.size(); ++i) {
		palette[i] = toPaletteEntry(model[i] * inverseBind[i]);
	}
}

}  // namespace bird
