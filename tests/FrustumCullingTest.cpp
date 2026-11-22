#include <gtest/gtest.h>

#include <cmath>

#include "renders/generic/Bounds.h"
#include "renders/generic/FrustumCuller.h"

using namespace bird;

namespace {

Matrix4 makeDefaultViewProjection() {
	const Matrix4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 1.0f, 50.0f);
	const Matrix4 view =
		glm::lookAt(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f),
					Vector3(0.0f, 1.0f, 0.0f));
	return proj * view;
}

}  // namespace

TEST(FrustumCulling, ExtractsAllSixNormalizedPlanes) {
	const Frustum frustum = Frustum::fromViewProjection(makeDefaultViewProjection());
	for (size_t i = 0; i < static_cast<size_t>(FrustumPlane::Count); i++) {
		const Vector4& p = frustum.plane(static_cast<FrustumPlane>(i));
		EXPECT_NEAR(glm::length(Vector3(p)), 1.0f, 1e-5f);
	}
	// Camera looks down -Z from origin, near=1, far=50.
	EXPECT_TRUE(frustum.containsPoint(Vector3(0.0f, 0.0f, -10.0f)));
	EXPECT_FALSE(frustum.containsPoint(Vector3(0.0f, 0.0f, -0.5f)));   // before near
	EXPECT_FALSE(frustum.containsPoint(Vector3(0.0f, 0.0f, -55.0f)));  // past far
	EXPECT_FALSE(frustum.containsPoint(Vector3(-20.0f, 0.0f, -5.0f))); // outside left
	EXPECT_FALSE(frustum.containsPoint(Vector3(20.0f, 0.0f, -5.0f)));  // outside right
	EXPECT_FALSE(frustum.containsPoint(Vector3(0.0f, -20.0f, -5.0f))); // outside bottom
	EXPECT_FALSE(frustum.containsPoint(Vector3(0.0f, 20.0f, -5.0f)));  // outside top
}

TEST(FrustumCulling, BoundingSphereFromPositionsAndTransform) {
	const float verts[] = {
		-1.0f, 0.0f, 0.0f,
		 3.0f, 0.0f, 0.0f,
		 1.0f, 2.0f, 0.0f,
		 1.0f, -2.0f, 0.0f
	};
	const BoundingSphere sphere = BoundingSphere::fromPositions(verts, 4);
	EXPECT_FALSE(sphere.isEmpty());
	EXPECT_NEAR(sphere.center.x, 1.0f, 1e-5f);
	EXPECT_NEAR(sphere.center.y, 0.0f, 1e-5f);
	EXPECT_NEAR(sphere.center.z, 0.0f, 1e-5f);
	EXPECT_NEAR(sphere.radius, 2.0f, 1e-5f);

	const Matrix4 xform =
		glm::translate(Matrix4(1.0f), Vector3(0.0f, 0.0f, -10.0f)) *
		glm::scale(Matrix4(1.0f), Vector3(1.0f, 3.0f, 2.0f));
	const BoundingSphere worldSphere = sphere.transformed(xform);
	EXPECT_NEAR(worldSphere.center.x, 1.0f, 1e-5f);
	EXPECT_NEAR(worldSphere.center.z, -10.0f, 1e-5f);
	// Conservative radius uses largest axis scale (3.0f).
	EXPECT_NEAR(worldSphere.radius, 6.0f, 1e-5f);

	EXPECT_TRUE(BoundingSphere::fromPositions(nullptr, 0).isEmpty());
	EXPECT_TRUE(BoundingSphere::fromAABB(AABB{}).isEmpty());
}

TEST(FrustumCulling, TracksRenderedAndCulledCountsPerBucket) {
	FrustumCuller culler;
	culler.beginFrame(makeDefaultViewProjection());

	const AABB visibleBox{Vector3(-1.0f, -1.0f, -11.0f), Vector3(1.0f, 1.0f, -9.0f)};
	const AABB culledBehind{Vector3(-1.0f, -1.0f, 5.0f), Vector3(1.0f, 1.0f, 7.0f)};
	const AABB culledLeft{Vector3(-100.0f, -1.0f, -11.0f), Vector3(-98.0f, 1.0f, -9.0f)};
	const AABB culledFar{Vector3(-1.0f, -1.0f, -100.0f), Vector3(1.0f, 1.0f, -90.0f)};

	EXPECT_TRUE(culler.testMesh(visibleBox, RenderBucket::Deferred));
	EXPECT_FALSE(culler.testMesh(culledBehind, RenderBucket::Deferred));
	EXPECT_TRUE(culler.testMesh(visibleBox, RenderBucket::ForwardOpaque));
	EXPECT_FALSE(culler.testMesh(culledLeft, RenderBucket::ForwardOpaque));
	EXPECT_FALSE(culler.testMesh(culledFar, RenderBucket::Transparent));

	EXPECT_TRUE(culler.testParticleEmitter(visibleBox));
	EXPECT_FALSE(culler.testParticleEmitter(culledBehind));

	const CullingStats& stats = culler.getStats();
	EXPECT_EQ(stats.totalTested, 5u);
	EXPECT_EQ(stats.rendered, 2u);
	EXPECT_EQ(stats.culled, 3u);
	EXPECT_EQ(stats.deferredRendered, 1u);
	EXPECT_EQ(stats.deferredCulled, 1u);
	EXPECT_EQ(stats.forwardOpaqueRendered, 1u);
	EXPECT_EQ(stats.forwardOpaqueCulled, 1u);
	EXPECT_EQ(stats.transparentRendered, 0u);
	EXPECT_EQ(stats.transparentCulled, 1u);
	EXPECT_EQ(stats.forwardRendered(), 1u);
	EXPECT_EQ(stats.forwardCulled(), 2u);
	EXPECT_NEAR(stats.cullRatio(), 0.6f, 1e-5f);

	EXPECT_EQ(stats.emittersTested, 2u);
	EXPECT_EQ(stats.emittersRendered, 1u);
	EXPECT_EQ(stats.emittersCulled, 1u);

	// Next frame resets all counters.
	culler.beginFrame(makeDefaultViewProjection());
	EXPECT_EQ(culler.getStats().totalTested, 0u);
	EXPECT_EQ(culler.getStats().emittersTested, 0u);
	EXPECT_FLOAT_EQ(culler.getStats().cullRatio(), 0.0f);
}

TEST(FrustumCulling, DisablingCullerPassesAllRenderables) {
	FrustumCuller culler;
	culler.setEnabled(false);
	culler.beginFrame(makeDefaultViewProjection());

	const AABB behindCamera{Vector3(-1.0f, -1.0f, 20.0f), Vector3(1.0f, 1.0f, 22.0f)};
	EXPECT_TRUE(culler.testMesh(behindCamera, RenderBucket::Deferred));
	EXPECT_TRUE(culler.testParticleEmitter(behindCamera));
	EXPECT_EQ(culler.getStats().rendered, 1u);
	EXPECT_EQ(culler.getStats().culled, 0u);
	EXPECT_EQ(culler.getStats().emittersRendered, 1u);
	EXPECT_EQ(culler.getStats().emittersCulled, 0u);
}

TEST(FrustumCulling, LargeBoxEnclosingFrustumIsVisible) {
	FrustumCuller culler;
	culler.beginFrame(makeDefaultViewProjection());

	const AABB giant{Vector3(-500.0f), Vector3(500.0f)};
	EXPECT_TRUE(culler.testMesh(giant, RenderBucket::Deferred));
}
