#include <gtest/gtest.h>

#include <cmath>

#include "scene/components/ParticleEmitter.h"

using namespace bird;

TEST(ParticleEmitter, FixedCapacityPoolEnforcesMaximumCount) {
	ParticleEmitter emitter(16);
	EXPECT_EQ(emitter.getCapacity(), 16u);
	EXPECT_EQ(emitter.getActiveCount(), 0u);
	EXPECT_TRUE(emitter.getWorldBounds().isEmpty());

	const size_t spawned1 = emitter.emitBurst(10);
	EXPECT_EQ(spawned1, 10u);
	EXPECT_EQ(emitter.getActiveCount(), 10u);
	EXPECT_FALSE(emitter.getWorldBounds().isEmpty());

	// Requesting more than remaining capacity clamps to 6.
	const size_t spawned2 = emitter.emitBurst(20);
	EXPECT_EQ(spawned2, 6u);
	EXPECT_EQ(emitter.getActiveCount(), 16u);

	emitter.reset();
	EXPECT_EQ(emitter.getActiveCount(), 0u);
	EXPECT_TRUE(emitter.getWorldBounds().isEmpty());
}

TEST(ParticleEmitter, ContinuousSpawnRateAndLifetimeRecycling) {
	ParticleEmitter emitter(64);
	emitter.setSpawnRate(20.0f);  // 20 particles / second
	emitter.setLifetimeRange(1.0f, 1.0f);
	emitter.setInitialSpeedRange(0.0f, 0.0f);
	emitter.setGravity(Vector3(0.0f));

	emitter.step(0.5f);  // 10 spawned
	EXPECT_EQ(emitter.getActiveCount(), 10u);

	emitter.step(0.25f);  // +5 spawned (total 15, first batch age = 0.25s)
	EXPECT_EQ(emitter.getActiveCount(), 15u);

	// Step another 0.8s: first batch (age 1.05s >= 1.0s) expires, second batch
	// (age 0.8s) survives, and 16 new particles spawn -> 5 + 16 = 21.
	emitter.step(0.8f);
	EXPECT_EQ(emitter.getActiveCount(), 21u);
}

TEST(ParticleEmitter, ScheduledBurstsFireAtTargetTimesAndLoop) {
	ParticleEmitter emitter(64);
	emitter.setSpawnRate(0.0f);
	emitter.setDuration(2.0f);
	emitter.setLooping(true);
	emitter.setLifetimeRange(10.0f, 10.0f);
	emitter.addBurst(0.5f, 8);

	emitter.step(0.25f);
	EXPECT_EQ(emitter.getActiveCount(), 0u);

	emitter.step(0.30f);  // t = 0.55s -> crosses 0.5s burst
	EXPECT_EQ(emitter.getActiveCount(), 8u);

	emitter.step(0.50f);  // t = 1.05s -> burst already fired this cycle
	EXPECT_EQ(emitter.getActiveCount(), 8u);

	// Wrap past duration (2.0s) to t = 0.65s in next cycle -> burst fires again!
	emitter.step(1.60f);
	EXPECT_EQ(emitter.getActiveCount(), 16u);
}

TEST(ParticleEmitter, GravityDragAndConeSpreadKinematics) {
	ParticleEmitter emitter(8);
	emitter.setSpawnRate(0.0f);
	emitter.setLifetimeRange(5.0f, 5.0f);
	emitter.setInitialSpeedRange(10.0f, 10.0f);
	emitter.setEmissionCone(Vector3(0.0f, 1.0f, 0.0f), 0.0f);  // strictly +Y
	emitter.setGravity(Vector3(0.0f, -10.0f, 0.0f));
	emitter.setDrag(0.0f);
	emitter.setSizeRange(0.2f, 0.2f);

	ASSERT_EQ(emitter.emitBurst(1), 1u);

	// Step 0.5s: v_y = 10 + (-10)*0.5 = 5; y = 0 + 5 * 0.5 = 2.5
	emitter.step(0.5f);
	const AABB& bounds = emitter.getWorldBounds();
	EXPECT_NEAR(bounds.center().x, 0.0f, 1e-4f);
	EXPECT_NEAR(bounds.center().y, 2.5f, 1e-4f);
	EXPECT_NEAR(bounds.center().z, 0.0f, 1e-4f);
}

TEST(ParticleEmitter, ColorAndSizeCurvesSampleOverNormalizedLifetime) {
	ParticleEmitter emitter(4);
	emitter.setSpawnRate(0.0f);
	emitter.setLifetimeRange(2.0f, 2.0f);
	emitter.setInitialSpeedRange(0.0f, 0.0f);
	emitter.setGravity(Vector3(0.0f));

	Curve<Vector4> colorCurve;
	colorCurve.addKey(0.0f, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
	colorCurve.addKey(1.0f, Vector4(0.0f, 0.0f, 1.0f, 0.0f));
	emitter.setColorOverLife(colorCurve);

	Curve<float> sizeCurve;
	sizeCurve.addKey(0.0f, 2.0f);
	sizeCurve.addKey(1.0f, 4.0f);
	emitter.setSizeOverLife(sizeCurve);

	ASSERT_EQ(emitter.emitBurst(1), 1u);

	// Advance to half-life (t = 1.0s / 2.0s = 0.5 normalized).
	emitter.step(1.0f);

	const size_t vertCount = emitter.buildBillboardQuads(
		Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f),
		Vector3(0.0f, 0.0f, -1.0f));
	ASSERT_EQ(vertCount, 4u);

	const ParticleQuadVertex* verts = emitter.getQuadVertices();
	// Color at t = 0.5 is (0.5, 0.0, 0.5, 0.5).
	EXPECT_NEAR(verts[0].color.r, 0.5f, 1e-4f);
	EXPECT_NEAR(verts[0].color.g, 0.0f, 1e-4f);
	EXPECT_NEAR(verts[0].color.b, 0.5f, 1e-4f);
	EXPECT_NEAR(verts[0].color.a, 0.5f, 1e-4f);

	// Size at t = 0.5 is 3.0 -> half-size = 1.5 -> bottom-left (-1.5, -1.5, 0)
	// and top-right (+1.5, +1.5, 0).
	EXPECT_NEAR(verts[0].position.x, -1.5f, 1e-4f);
	EXPECT_NEAR(verts[0].position.y, -1.5f, 1e-4f);
	EXPECT_NEAR(verts[2].position.x, 1.5f, 1e-4f);
	EXPECT_NEAR(verts[2].position.y, 1.5f, 1e-4f);
}

TEST(ParticleEmitter, AlphaParticlesSortBackToFrontAlongCameraForward) {
	ParticleEmitter emitter(4);
	emitter.setSpawnRate(0.0f);
	emitter.setLifetimeRange(10.0f, 10.0f);
	emitter.setInitialSpeedRange(0.0f, 0.0f);
	emitter.setGravity(Vector3(0.0f));
	emitter.setBlendMode(ParticleBlendMode::Alpha);
	emitter.setSizeRange(1.0f, 1.0f);

	// Spawn particle 0 near camera (z = -2) and particle 1 far from camera (z = -10).
	emitter.setPositionOffset(Vector3(0.0f, 0.0f, -2.0f));
	emitter.emitBurst(1);
	emitter.setPositionOffset(Vector3(0.0f, 0.0f, -10.0f));
	emitter.emitBurst(1);

	const size_t vertCount = emitter.buildBillboardQuads(
		Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f),
		Vector3(0.0f, 0.0f, -1.0f));
	ASSERT_EQ(vertCount, 8u);

	const ParticleQuadVertex* verts = emitter.getQuadVertices();
	// Farther particle (z = -10) must be emitted into quad 0 before nearer (z = -2).
	EXPECT_NEAR(verts[0].position.z, -10.0f, 1e-4f);
	EXPECT_NEAR(verts[4].position.z, -2.0f, 1e-4f);
}
