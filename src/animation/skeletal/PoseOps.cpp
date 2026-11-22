#include "PoseOps.h"

#include <algorithm>
#include <stdexcept>

#include "../Interpolation.h"

namespace bird {

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
