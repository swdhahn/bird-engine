#include <gtest/gtest.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <memory>
#include <sstream>

#include "../src/animation/skeletal/SkeletalAnimator.h"
#include "../src/animation/skeletal/SkeletalAsset.h"
#include "SkeletalImport.h"

using namespace bird;

namespace {

/*
 * Minimal rigged glTF (generated, embedded buffer): an "Armature" node
 * holding a two-joint chain root -> tip (tip one unit up), a skinned
 * triangle, and a one-second "Swing" clip rotating root 90 degrees about Z.
 * Vertex 0 = (0,1,0) follows tip, 1 = (0,0,0) follows root, 2 = (1,0,0)
 * is split half/half.
 */
const char* const kRiggedTriangle =
	R"GLTF({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0,3]}],"nodes":[{"name":"Armatu)GLTF"
	R"GLTF(re","children":[1]},{"name":"root","children":[2]},{"name":"tip","translation":[0,1,0]},)GLTF"
	R"GLTF({"name":"Mesh","mesh":0,"skin":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,)GLTF"
	R"GLTF("JOINTS_0":2,"WEIGHTS_0":3},"indices":1}]}],"skins":[{"joints":[1,2],"inverseBindMatrice)GLTF"
	R"GLTF(s":4,"skeleton":1}],"animations":[{"name":"Swing","channels":[{"sampler":0,"target":{"no)GLTF"
	R"GLTF(de":1,"path":"rotation"}}],"samplers":[{"input":5,"output":6,"interpolation":"LINEAR"}]})GLTF"
	R"GLTF(],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0)GLTF"
	R"GLTF(],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},{"buffe)GLTF"
	R"GLTF(rView":2,"componentType":5121,"count":3,"type":"VEC4"},{"bufferView":3,"componentType":5)GLTF"
	R"GLTF(126,"count":3,"type":"VEC4"},{"bufferView":4,"componentType":5126,"count":2,"type":"MAT4)GLTF"
	R"GLTF("},{"bufferView":5,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},{)GLTF"
	R"GLTF("bufferView":6,"componentType":5126,"count":2,"type":"VEC4"}],"bufferViews":[{"buffer":0)GLTF"
	R"GLTF(,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength")GLTF"
	R"GLTF(:6,"target":34963},{"buffer":0,"byteOffset":44,"byteLength":12,"target":34962},{"buffer")GLTF"
	R"GLTF(:0,"byteOffset":56,"byteLength":48,"target":34962},{"buffer":0,"byteOffset":104,"byteLen)GLTF"
	R"GLTF(gth":128},{"buffer":0,"byteOffset":232,"byteLength":8},{"buffer":0,"byteOffset":240,"byt)GLTF"
	R"GLTF(eLength":32}],"buffers":[{"byteLength":272,"uri":"data:application/octet-stream;base64,A)GLTF"
	R"GLTF(AAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAABAAIAAAABAAAAAAAAAAABAAAAAIA/AAAAAAAAA)GLTF"
	R"GLTF(AAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAD8AAAA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAA)GLTF"
	R"GLTF(AAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAA)GLTF"
	R"GLTF(AAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAgL8AAAAAAACAPwAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAA8)GLTF"
	R"GLTF(wQ1P/MENT8="}]})GLTF";

const aiScene* importRig(Assimp::Importer& importer) {
	const std::string data(kRiggedTriangle);
	return importer.ReadFileFromMemory(
		data.data(), data.size(),
		aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
			aiProcess_LimitBoneWeights,
		"gltf");
}

Vector3 skin(const SkeletalAnimator& animator, const SkinStream& skin,
			 size_t vertex, Vector3 position) {
	Vector3 result(0.0f);
	for (int i = 0; i < 4; ++i) {
		const float w = skin.weights[vertex][i];
		if (w == 0.0f) continue;
		const Matrix3x4& m = animator.getSkinningPalette()[skin.joints[vertex][i]];
		result += w * Vector3(Vector4(position, 1.0f) * m);
	}
	return result;
}

}  // namespace

TEST(SkeletalImport, ExtractsSkeletonClipsAndSkinFromGltf) {
	Assimp::Importer importer;
	const aiScene* scene = importRig(importer);
	ASSERT_NE(scene, nullptr) << importer.GetErrorString();
	ASSERT_TRUE(sceneHasSkinning(*scene));

	const SkeletalAsset asset = extractSkeletalAsset(*scene);
	const Skeleton& skeleton = *asset.skeleton;
	const int32_t root = skeleton.findJoint("root");
	const int32_t tip = skeleton.findJoint("tip");
	ASSERT_GE(root, 0);
	ASSERT_GE(tip, 0);
	EXPECT_EQ(skeleton.getParent(tip), root);
	EXPECT_GE(skeleton.findJoint("Armature"), 0);  // container kept
	EXPECT_LT(skeleton.findJoint("Armature"), root);  // topological order

	ASSERT_EQ(asset.clips.size(), 1u);
	EXPECT_NEAR(asset.clips[0]->getDuration(), 1.0f, 1e-4f);
	ASSERT_EQ(asset.skins.size(), 1u);
	ASSERT_EQ(asset.skins[0].joints.size(), 3u);
	for (const auto& weights : asset.skins[0].weights) {
		EXPECT_NEAR(weights[0] + weights[1] + weights[2] + weights[3], 1.0f,
					1e-5f);
	}
}

TEST(SkeletalImport, RoundTripsAndAnimatesTheSkinnedTriangle) {
	Assimp::Importer importer;
	const aiScene* scene = importRig(importer);
	ASSERT_NE(scene, nullptr) << importer.GetErrorString();

	std::stringstream file;
	writeSkeletalAsset(file, extractSkeletalAsset(*scene));
	const SkeletalAsset asset = readSkeletalAsset(file);

	const aiMesh& mesh = *scene->mMeshes[asset.skins[0].meshIndex];
	SkeletalAnimator animator(asset.skeleton);
	animator.play(asset.findClip("Swing"), LoopMode::Once);

	animator.update(0.0f);	// bind pose reproduces the mesh exactly
	for (unsigned v = 0; v < mesh.mNumVertices; ++v) {
		const Vector3 p(mesh.mVertices[v].x, mesh.mVertices[v].y,
						mesh.mVertices[v].z);
		const Vector3 s = skin(animator, asset.skins[0], v, p);
		EXPECT_NEAR(glm::distance(s, p), 0.0f, 1e-4f) << "vertex " << v;
	}

	animator.update(1.0f);	// root swung 90 degrees about Z
	for (unsigned v = 0; v < mesh.mNumVertices; ++v) {
		const Vector3 p(mesh.mVertices[v].x, mesh.mVertices[v].y,
						mesh.mVertices[v].z);
		const Vector3 expected(-p.y, p.x, p.z);	 // rigid 90 degree turn
		const Vector3 s = skin(animator, asset.skins[0], v, p);
		EXPECT_NEAR(glm::distance(s, expected), 0.0f, 1e-4f) << "vertex " << v;
	}
}

TEST(SkeletalAsset, RejectsCorruptFiles) {
	std::stringstream garbage("NOPE and some bytes");
	EXPECT_THROW(readSkeletalAsset(garbage), std::runtime_error);

	Assimp::Importer importer;
	const aiScene* scene = importRig(importer);
	ASSERT_NE(scene, nullptr);
	std::stringstream file;
	writeSkeletalAsset(file, extractSkeletalAsset(*scene));
	const std::string bytes = file.str();
	for (size_t cut : {size_t(5), bytes.size() / 2, bytes.size() - 1}) {
		std::stringstream truncated(bytes.substr(0, cut));
		EXPECT_THROW(readSkeletalAsset(truncated), std::runtime_error) << cut;
	}
}
