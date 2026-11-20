#include "SkeletalImport.h"

#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bird {

namespace {

/** Assimp's documented fallback when a file omits ticks per second. */
constexpr double kDefaultTicksPerSecond = 25.0;

Matrix4 toMatrix(const aiMatrix4x4& m) {
	// Assimp is row-major; glm is column-major.
	return Matrix4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3,
				   m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
}

Vector3 toVector(const aiVector3D& v) {
	return Vector3(v.x, v.y, v.z);
}

Quaternion toQuaternion(const aiQuaternion& q) {
	return glm::normalize(Quaternion(q.w, q.x, q.y, q.z));
}

JointTransform decompose(const aiMatrix4x4& m) {
	aiVector3D scaling, position;
	aiQuaternion rotation;
	m.Decompose(scaling, rotation, position);
	JointTransform t;
	t.translation = toVector(position);
	t.rotation = toQuaternion(rotation);
	t.scale = toVector(scaling);
	return t;
}

/** Marks a node and every ancestor as required. */
void requireWithAncestors(const aiNode* node,
						  std::unordered_set<const aiNode*>& required) {
	for (; node && required.insert(node).second; node = node->mParent) {
	}
}

const aiNode* findNode(const aiScene& scene, const aiString& name) {
	return scene.mRootNode ? scene.mRootNode->FindNode(name) : nullptr;
}

/** Depth-first walk emitting required nodes parent-before-child. */
void addJoints(const aiNode* node, int32_t parent,
			   const std::unordered_set<const aiNode*>& required,
			   const std::unordered_map<std::string, Matrix4>& inverseBinds,
			   Skeleton& skeleton) {
	if (!required.count(node)) return;
	const std::string name = node->mName.C_Str();
	const auto bind = inverseBinds.find(name);
	int32_t index;
	try {
		index = skeleton.addJoint(
			name, parent, bind == inverseBinds.end() ? Matrix4(1.0f) : bind->second,
			decompose(node->mTransformation));
	} catch (const std::invalid_argument& e) {
		throw std::runtime_error(std::string("Skeletal import: ") + e.what());
	}
	for (unsigned i = 0; i < node->mNumChildren; ++i) {
		addJoints(node->mChildren[i], index, required, inverseBinds, skeleton);
	}
}

template <typename Key, typename Value, typename Convert>
Curve<Value> toCurve(const Key* keys, unsigned count, double ticksPerSecond,
					 Convert convert) {
	Curve<Value> curve(CurveInterpolation::Linear);
	std::vector<Keyframe<Value>> frames;
	frames.reserve(count);
	for (unsigned i = 0; i < count; ++i) {
		const float time = static_cast<float>(keys[i].mTime / ticksPerSecond);
		// Some exporters emit duplicate times; keep the last value.
		if (!frames.empty() && frames.back().time == time) frames.pop_back();
		Keyframe<Value> frame;
		frame.time = time;
		frame.value = convert(keys[i].mValue);
		frames.push_back(frame);
	}
	curve.setKeys(std::move(frames));
	return curve;
}

std::shared_ptr<SkeletalClip> toClip(const aiAnimation& animation,
									 const Skeleton& skeleton,
									 size_t clipIndex) {
	const double ticksPerSecond = animation.mTicksPerSecond > 0.0
									  ? animation.mTicksPerSecond
									  : kDefaultTicksPerSecond;
	std::string name = animation.mName.C_Str();
	if (name.empty()) name = "Clip" + std::to_string(clipIndex);
	auto clip = std::make_shared<SkeletalClip>(
		name, static_cast<float>(std::max(animation.mDuration, 0.0) /
								 ticksPerSecond));
	for (unsigned c = 0; c < animation.mNumChannels; ++c) {
		const aiNodeAnim& channel = *animation.mChannels[c];
		JointTrack track;
		track.joint = skeleton.findJoint(channel.mNodeName.C_Str());
		if (track.joint < 0) continue;	// node outside the skeleton
		track.translation = toCurve<aiVectorKey, Vector3>(
			channel.mPositionKeys, channel.mNumPositionKeys, ticksPerSecond,
			toVector);
		track.rotation = toCurve<aiQuatKey, Quaternion>(
			channel.mRotationKeys, channel.mNumRotationKeys, ticksPerSecond,
			toQuaternion);
		track.scale = toCurve<aiVectorKey, Vector3>(
			channel.mScalingKeys, channel.mNumScalingKeys, ticksPerSecond,
			toVector);
		clip->addTrack(std::move(track));
	}
	return clip;
}

SkinStream toSkin(const aiMesh& mesh, uint32_t meshIndex,
				  const Skeleton& skeleton) {
	struct Influence {
		uint16_t joint;
		float weight;
	};
	std::vector<std::vector<Influence>> influences(mesh.mNumVertices);
	for (unsigned b = 0; b < mesh.mNumBones; ++b) {
		const aiBone& bone = *mesh.mBones[b];
		const int32_t joint = skeleton.findJoint(bone.mName.C_Str());
		if (joint < 0) {
			throw std::runtime_error("Skeletal import: bone '" +
									 std::string(bone.mName.C_Str()) +
									 "' has no joint");
		}
		for (unsigned w = 0; w < bone.mNumWeights; ++w) {
			const aiVertexWeight& vw = bone.mWeights[w];
			if (vw.mVertexId >= mesh.mNumVertices || !(vw.mWeight > 0.0f)) {
				continue;
			}
			influences[vw.mVertexId].push_back(
				{static_cast<uint16_t>(joint), vw.mWeight});
		}
	}
	SkinStream skin;
	skin.meshIndex = meshIndex;
	skin.joints.resize(mesh.mNumVertices);
	skin.weights.resize(mesh.mNumVertices);
	for (unsigned v = 0; v < mesh.mNumVertices; ++v) {
		std::vector<Influence>& list = influences[v];
		std::sort(list.begin(), list.end(),
				  [](const Influence& a, const Influence& b) {
					  return a.weight > b.weight;
				  });
		if (list.size() > 4) list.resize(4);
		float total = 0.0f;
		for (const Influence& i : list) total += i.weight;
		std::array<uint16_t, 4> joints{0, 0, 0, 0};
		std::array<float, 4> weights{0.0f, 0.0f, 0.0f, 0.0f};
		if (total <= 0.0f) {
			weights[0] = 1.0f;	// unweighted: follow the root rigidly
		} else {
			for (size_t i = 0; i < list.size(); ++i) {
				joints[i] = list[i].joint;
				weights[i] = list[i].weight / total;
			}
		}
		skin.joints[v] = joints;
		skin.weights[v] = weights;
	}
	return skin;
}

}  // namespace

bool sceneHasSkinning(const aiScene& scene) {
	for (unsigned i = 0; i < scene.mNumMeshes; ++i) {
		if (scene.mMeshes[i]->HasBones()) return true;
	}
	return false;
}

SkeletalAsset extractSkeletalAsset(const aiScene& scene) {
	SkeletalAsset asset;
	asset.skeleton = std::make_shared<Skeleton>();
	if (!scene.mRootNode) return asset;

	std::unordered_set<const aiNode*> required;
	std::unordered_map<std::string, Matrix4> inverseBinds;
	for (unsigned m = 0; m < scene.mNumMeshes; ++m) {
		const aiMesh& mesh = *scene.mMeshes[m];
		for (unsigned b = 0; b < mesh.mNumBones; ++b) {
			const aiBone& bone = *mesh.mBones[b];
			const aiNode* node = findNode(scene, bone.mName);
			if (!node) {
				throw std::runtime_error(
					"Skeletal import: bone '" + std::string(bone.mName.C_Str()) +
					"' has no matching node");
			}
			requireWithAncestors(node, required);
			inverseBinds.emplace(bone.mName.C_Str(), toMatrix(bone.mOffsetMatrix));
		}
	}
	for (unsigned a = 0; a < scene.mNumAnimations; ++a) {
		const aiAnimation& animation = *scene.mAnimations[a];
		for (unsigned c = 0; c < animation.mNumChannels; ++c) {
			requireWithAncestors(findNode(scene, animation.mChannels[c]->mNodeName),
								 required);
		}
	}
	if (required.size() > Skeleton::kMaxJoints) {
		throw std::runtime_error(
			"Skeletal import: " + std::to_string(required.size()) +
			" joints exceed the limit of " +
			std::to_string(Skeleton::kMaxJoints));
	}
	addJoints(scene.mRootNode, -1, required, inverseBinds, *asset.skeleton);

	for (unsigned a = 0; a < scene.mNumAnimations; ++a) {
		asset.clips.push_back(toClip(*scene.mAnimations[a], *asset.skeleton, a));
	}
	for (unsigned m = 0; m < scene.mNumMeshes; ++m) {
		if (scene.mMeshes[m]->HasBones()) {
			asset.skins.push_back(toSkin(*scene.mMeshes[m], m, *asset.skeleton));
		}
	}
	return asset;
}

}  // namespace bird
