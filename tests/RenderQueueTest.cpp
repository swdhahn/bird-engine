#include <gtest/gtest.h>

#include "renders/generic/Bounds.h"
#include "renders/generic/Material.h"
#include "renders/generic/RenderQueue.h"

using namespace bird;

namespace {

// The queue never dereferences meshes, so distinct addresses suffice.
Mesh* fakeMesh(uintptr_t id) {
	return reinterpret_cast<Mesh*>(id * 16);
}

RenderItem item(Material& material, float depth, uintptr_t meshId = 1) {
	RenderItem r;
	r.mesh = fakeMesh(meshId);
	r.material = &material;
	r.viewDepth = depth;
	return r;
}

}  // namespace

TEST(RenderQueue, SubmitRoutesByMaterial) {
	Material opaque, glass;
	glass.setBlendMode(BlendMode::Transparent);
	RenderQueue queue;
	queue.submit(item(opaque, 1.0f));
	queue.submit(item(glass, 2.0f));
	EXPECT_EQ(queue.bucket(RenderBucket::Deferred).size(), 1u);
	EXPECT_EQ(queue.bucket(RenderBucket::Transparent).size(), 1u);
	EXPECT_EQ(queue.size(), 2u);
	queue.clear();
	EXPECT_EQ(queue.size(), 0u);
}

TEST(RenderQueue, RejectsIncompleteItems) {
	RenderQueue queue;
	EXPECT_THROW(queue.submit(RenderItem{}), std::invalid_argument);
}

TEST(RenderQueue, OpaqueGroupsByMaterialThenFrontToBack) {
	Material a, b;
	RenderQueue queue;
	queue.submit(item(a, 9.0f));
	queue.submit(item(b, 1.0f));
	queue.submit(item(a, 3.0f));
	queue.submit(item(b, 5.0f));
	queue.sort();
	const auto& items = queue.bucket(RenderBucket::Deferred);
	ASSERT_EQ(items.size(), 4u);
	// Materials stay contiguous and each run ascends in depth.
	EXPECT_EQ(items[0].material, items[1].material);
	EXPECT_EQ(items[2].material, items[3].material);
	EXPECT_LT(items[0].viewDepth, items[1].viewDepth);
	EXPECT_LT(items[2].viewDepth, items[3].viewDepth);
}

TEST(RenderQueue, TransparentSortsBackToFrontStably) {
	Material glass;
	glass.setBlendMode(BlendMode::Transparent);
	RenderQueue queue;
	queue.submit(item(glass, 2.0f, 1));
	queue.submit(item(glass, 8.0f, 2));
	queue.submit(item(glass, 2.0f, 3));
	queue.sort();
	const auto& items = queue.bucket(RenderBucket::Transparent);
	EXPECT_FLOAT_EQ(items[0].viewDepth, 8.0f);
	EXPECT_EQ(items[1].mesh, fakeMesh(1));	// equal depths keep order
	EXPECT_EQ(items[2].mesh, fakeMesh(3));
}

TEST(Bounds, TransformRefitsRotatedBox) {
	AABB box{Vector3(-1.0f), Vector3(1.0f)};
	const Matrix4 rotate =
		glm::rotate(Matrix4(1.0f), glm::radians(45.0f), Vector3(0, 1, 0));
	const AABB rotated = box.transformed(glm::translate(Matrix4(1.0f),
														Vector3(10, 0, 0)) *
										 rotate);
	EXPECT_NEAR(rotated.max.x, 10.0f + std::sqrt(2.0f), 1e-4);
	EXPECT_NEAR(rotated.min.y, -1.0f, 1e-4);
}

TEST(Bounds, EmptyBoxNeverIntersects) {
	const Frustum frustum = Frustum::fromViewProjection(
		glm::perspective(1.0f, 1.0f, 0.1f, 100.0f));
	EXPECT_FALSE(frustum.intersects(AABB{}));
}

TEST(Bounds, FrustumCullsBoxesBehindCamera) {
	const Frustum frustum = Frustum::fromViewProjection(
		glm::perspective(1.0f, 1.0f, 0.1f, 100.0f));
	EXPECT_TRUE(frustum.intersects(AABB{Vector3(-1, -1, -11), Vector3(1, 1, -9)}));
	EXPECT_FALSE(frustum.intersects(AABB{Vector3(-1, -1, 9), Vector3(1, 1, 11)}));
	EXPECT_FALSE(frustum.intersects(
		AABB{Vector3(-1, -1, -300), Vector3(1, 1, -200)}));
	EXPECT_TRUE(frustum.intersectsSphere(Vector3(0, 0, 2), 3.0f));
	EXPECT_FALSE(frustum.intersectsSphere(Vector3(0, 0, 5), 3.0f));
}

TEST(RenderQueue, OpaqueBucketsGroupRigidBeforeSkinned) {
	Material a, b;
	const std::vector<Matrix3x4> palette(1);
	RenderQueue queue;
	RenderItem skinnedNear = item(a, 1.0f, 1);
	skinnedNear.skinPalette = &palette;
	queue.submit(skinnedNear);
	queue.submit(item(b, 5.0f, 2));
	queue.submit(item(a, 3.0f, 3));
	queue.sort();
	const auto& deferred = queue.bucket(RenderBucket::Deferred);
	ASSERT_EQ(deferred.size(), 3u);
	// One program switch per pass: every rigid draw precedes every skinned.
	EXPECT_FALSE(deferred[0].isSkinned());
	EXPECT_FALSE(deferred[1].isSkinned());
	EXPECT_TRUE(deferred[2].isSkinned());
}
