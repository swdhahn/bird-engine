#include <gtest/gtest.h>

#include <set>

#include "renders/generic/LightList.h"
#include "renders/generic/ShadowAtlas.h"
#include "renders/generic/ShadowMath.h"
#include "renders/generic/ShadowPlanner.h"

using namespace bird;

// ---------------------------------------------------------------- atlas

TEST(ShadowAtlas, RejectsNonPowerOfTwoSizes) {
	EXPECT_THROW(ShadowAtlas(1000, 128), std::invalid_argument);
	EXPECT_THROW(ShadowAtlas(1024, 2048), std::invalid_argument);
}

TEST(ShadowAtlas, FillsExactlyFourQuadrants) {
	ShadowAtlas atlas(1024, 128);
	std::set<std::pair<uint32_t, uint32_t>> origins;
	for (int i = 0; i < 4; i++) {
		auto tile = atlas.allocate(512);
		ASSERT_TRUE(tile.has_value());
		EXPECT_EQ(tile->size, 512u);
		origins.insert({tile->x, tile->y});
	}
	EXPECT_EQ(origins.size(), 4u);
	EXPECT_FALSE(atlas.allocate(128).has_value());
	atlas.reset();
	EXPECT_TRUE(atlas.allocate(1024).has_value());
}

TEST(ShadowAtlas, RoundsAndClampsRequests) {
	ShadowAtlas atlas(1024, 128);
	EXPECT_EQ(atlas.roundTileSize(300), 512u);
	EXPECT_EQ(atlas.roundTileSize(1), 128u);
	EXPECT_EQ(atlas.roundTileSize(5000), 1024u);
}

TEST(ShadowAtlas, MixedSizesNeverOverlap) {
	ShadowAtlas atlas(1024, 64);
	std::vector<AtlasTile> tiles;
	for (uint32_t size : {512u, 64u, 256u, 128u, 256u, 64u, 128u}) {
		auto tile = atlas.allocate(size);
		ASSERT_TRUE(tile.has_value());
		tiles.push_back(*tile);
	}
	for (size_t i = 0; i < tiles.size(); i++) {
		for (size_t j = i + 1; j < tiles.size(); j++) {
			const AtlasTile& a = tiles[i];
			const AtlasTile& b = tiles[j];
			const bool disjoint = a.x + a.size <= b.x || b.x + b.size <= a.x ||
								  a.y + a.size <= b.y || b.y + b.size <= a.y;
			EXPECT_TRUE(disjoint) << i << " overlaps " << j;
		}
	}
}

TEST(ShadowAtlas, GroupDegradesThenFailsAtomically) {
	ShadowAtlas atlas(1024, 256);
	ASSERT_TRUE(atlas.allocate(512).has_value());
	// Six 512 tiles cannot fit in the three free quadrants; six 256 can.
	auto group = atlas.allocateGroup(512, 6);
	ASSERT_EQ(group.size(), 6u);
	EXPECT_EQ(group.front().size, 256u);
	// Six remaining 256 slots exist; a seventh group of six cannot fit.
	EXPECT_EQ(atlas.allocateGroup(256, 6).size(), 6u);
	EXPECT_TRUE(atlas.allocateGroup(256, 6).empty());
}

TEST(ShadowAtlas, UvRectNormalizesTile) {
	const AtlasTile tile{512, 256, 256};
	const Vector4 rect = tile.uvRect(1024);
	EXPECT_FLOAT_EQ(rect.x, 0.5f);
	EXPECT_FLOAT_EQ(rect.y, 0.25f);
	EXPECT_FLOAT_EQ(rect.z, 0.25f);
}

// ---------------------------------------------------------------- math

TEST(ShadowMath, CascadeSplitsAreMonotonicAndBounded) {
	const auto splits = shadow::computeCascadeSplits(0.1f, 100.0f, 4, 0.75f);
	ASSERT_EQ(splits.size(), 5u);
	EXPECT_FLOAT_EQ(splits.front(), 0.1f);
	EXPECT_FLOAT_EQ(splits.back(), 100.0f);
	for (size_t i = 1; i < splits.size(); i++) {
		EXPECT_GT(splits[i], splits[i - 1]);
	}
	EXPECT_THROW(shadow::computeCascadeSplits(0.0f, 10.0f, 4, 0.5f),
				 std::invalid_argument);
}

TEST(ShadowMath, UniformLambdaSplitsEvenly) {
	const auto splits = shadow::computeCascadeSplits(1.0f, 5.0f, 4, 0.0f);
	EXPECT_FLOAT_EQ(splits[1], 2.0f);
	EXPECT_FLOAT_EQ(splits[2], 3.0f);
}

TEST(ShadowMath, CascadeContainsSliceCorners) {
	const Matrix4 inverseView = glm::translate(Matrix4(1.0f), Vector3(3, 2, 1));
	const float fov = 1.2f, aspect = 16.0f / 9.0f;
	const auto cascade = shadow::fitCascade(inverseView, fov, aspect, 1.0f,
											20.0f, glm::normalize(Vector3(1, -2, 0.5f)),
											2048, 50.0f);
	const float tanY = std::tan(fov * 0.5f), tanX = tanY * aspect;
	for (float z : {1.0f, 20.0f}) {
		for (float sx : {-1.0f, 1.0f}) {
			for (float sy : {-1.0f, 1.0f}) {
				const Vector4 corner =
					inverseView * Vector4(sx * tanX * z, sy * tanY * z, -z, 1);
				const Vector4 clip = cascade.viewProjection * corner;
				EXPECT_LE(std::abs(clip.x / clip.w), 1.0f + 1e-3f);
				EXPECT_LE(std::abs(clip.y / clip.w), 1.0f + 1e-3f);
				EXPECT_LE(std::abs(clip.z / clip.w), 1.0f + 1e-3f);
			}
		}
	}
	EXPECT_GT(cascade.texelWorldSize, 0.0f);
}

TEST(ShadowMath, CascadeTexelSnappingRemovesSubTexelDrift) {
	const Vector3 light = glm::normalize(Vector3(0.3f, -1.0f, 0.2f));
	const uint32_t resolution = 1024;
	auto originTexel = [&](const Vector3& cameraPosition) {
		const Matrix4 inverseView =
			glm::translate(Matrix4(1.0f), cameraPosition);
		const auto cascade = shadow::fitCascade(inverseView, 1.0f, 1.0f, 0.5f,
												30.0f, light, resolution, 10.0f);
		const Vector4 origin = cascade.viewProjection * Vector4(0, 0, 0, 1);
		return Vector2(origin) * (resolution * 0.5f);
	};
	// The world origin must land on whole texels wherever the camera sits.
	for (const Vector3& position :
		 {Vector3(0.0f), Vector3(0.37f, 0.0f, 1.13f), Vector3(-5.2f, 1.0f, 7.9f)}) {
		const Vector2 texel = originTexel(position);
		EXPECT_NEAR(texel.x, std::round(texel.x), 1e-2f);
		EXPECT_NEAR(texel.y, std::round(texel.y), 1e-2f);
	}
}

TEST(ShadowMath, CubeFaceSelectionMatchesFaceFrusta) {
	const Vector3 light(1.0f, 2.0f, 3.0f);
	const auto faces = shadow::pointFaceViewProjections(light, 10.0f, 512);
	const Vector3 directions[] = {{1, 0.2f, -0.3f}, {-1, 0.4f, 0.1f},
								  {0.3f, 1, 0.2f},	{0.1f, -1, -0.4f},
								  {0.2f, -0.1f, 1}, {-0.3f, 0.4f, -1}};
	for (uint32_t expected = 0; expected < 6; expected++) {
		const Vector3 dir = directions[expected];
		EXPECT_EQ(shadow::selectCubeFace(dir), expected);
		const Vector4 clip = faces[expected] * Vector4(light + dir * 5.0f, 1.0f);
		EXPECT_LT(std::abs(clip.x / clip.w), 1.0f);
		EXPECT_LT(std::abs(clip.y / clip.w), 1.0f);
	}
}

TEST(ShadowMath, GuardBandWidensFieldOfView) {
	const float base = glm::radians(90.0f);
	EXPECT_GT(shadow::guardedFieldOfView(base, 256, 2.0f), base);
	EXPECT_NEAR(shadow::guardedFieldOfView(base, 256, 0.0f), base, 1e-6f);
}

// ---------------------------------------------------------------- lights

namespace {

LightSnapshot makeLight(LightType type, Vector3 position, bool shadows) {
	LightSnapshot light;
	light.type = type;
	light.position = position;
	light.direction = Vector3(0, -1, 0);
	light.castsShadows = shadows;
	light.range = 10.0f;
	return light;
}

Frustum wideFrustum() {
	return Frustum::fromViewProjection(
		glm::perspective(2.0f, 1.0f, 0.1f, 500.0f));
}

}  // namespace

TEST(LightList, DirectionalFirstThenNearest) {
	std::vector<LightSnapshot> lights = {
		makeLight(LightType::Point, Vector3(0, 0, -50), false),
		makeLight(LightType::Directional, Vector3(0), false),
		makeLight(LightType::Point, Vector3(0, 0, -15), false)};
	const auto selection =
		selectForwardLights(lights, Vector3(0), wideFrustum(), 8);
	EXPECT_EQ(selection, (std::vector<size_t>{1, 2, 0}));

	const LightsUBOData block = buildLightsBlock(lights, selection);
	EXPECT_EQ(block.counts[0], 3);
	EXPECT_EQ(block.counts[1], 1);
	EXPECT_EQ(block.lights[0].colorType.w, GPU_LIGHT_DIRECTIONAL);
}

TEST(LightList, SkipsInvisibleAndDarkLightsAndTruncates) {
	std::vector<LightSnapshot> lights = {
		makeLight(LightType::Point, Vector3(0, 0, 100), false),	 // behind
		makeLight(LightType::Point, Vector3(0, 0, -5), false),
		makeLight(LightType::Point, Vector3(0, 0, -6), false),
		makeLight(LightType::Point, Vector3(0, 0, -7), false)};
	lights[2].intensity = 0.0f;
	const auto selection =
		selectForwardLights(lights, Vector3(0), wideFrustum(), 1);
	EXPECT_EQ(selection, (std::vector<size_t>{1}));
}

TEST(LightList, GpuLightPacksSpotCone) {
	LightSnapshot spot = makeLight(LightType::Spot, Vector3(1, 2, 3), false);
	spot.innerConeAngle = 0.2f;
	spot.outerConeAngle = 0.4f;
	spot.intensity = 2.0f;
	spot.color = Vector3(0.5f, 1.0f, 0.25f);
	const GpuLight gpu = toGpuLight(spot);
	EXPECT_EQ(gpu.colorType, Vector4(1.0f, 2.0f, 0.5f, GPU_LIGHT_SPOT));
	EXPECT_NEAR(gpu.directionCosOuter.w, std::cos(0.4f), 1e-6f);
	EXPECT_NEAR(gpu.shadowParams.x, std::cos(0.2f), 1e-6f);
	EXPECT_FLOAT_EQ(gpu.shadowParams.y, -1.0f);
}

// ---------------------------------------------------------------- planner

TEST(ShadowPlanner, AssignsCascadesThenLocalSlots) {
	std::vector<LightSnapshot> lights = {
		makeLight(LightType::Point, Vector3(0, 0, -10), true),
		makeLight(LightType::Directional, Vector3(0), true),
		makeLight(LightType::Spot, Vector3(2, 0, -12), true),
		makeLight(LightType::Point, Vector3(0, 0, -20), false)};
	ShadowCameraInfo camera;
	camera.aspect = 1.0f;
	const ShadowPlan plan =
		planShadows(lights, camera, wideFrustum(), ShadowSettings{});

	EXPECT_EQ(lights[1].shadowMatrixBase, 0);
	EXPECT_FLOAT_EQ(plan.gpuData.shadowInfo.x, 4.0f);
	// The nearer point light outranks the spot: faces 4..9, spot 10.
	EXPECT_EQ(lights[0].shadowMatrixBase, 4);
	EXPECT_EQ(lights[2].shadowMatrixBase, 10);
	EXPECT_EQ(lights[3].shadowMatrixBase, -1);
	EXPECT_EQ(plan.views.size(), 11u);
	EXPECT_EQ(plan.views[0].atlas, ShadowAtlasKind::Directional);
	EXPECT_EQ(plan.views[4].atlas, ShadowAtlasKind::Local);
	EXPECT_FLOAT_EQ(plan.gpuData.matrixParams[4].y, 1.0f);	// perspective
	EXPECT_FLOAT_EQ(plan.gpuData.matrixParams[0].y, 0.0f);	// orthographic
	EXPECT_GT(plan.gpuData.cascadeSplits.w, plan.gpuData.cascadeSplits.x);
}

TEST(ShadowPlanner, ResetsStaleIndicesAndRespectsSlotLimit) {
	std::vector<LightSnapshot> lights;
	for (int i = 0; i < 20; i++) {
		lights.push_back(
			makeLight(LightType::Point, Vector3(i * 0.5f, 0, -10), true));
	}
	lights[0].shadowMatrixBase = 99;
	ShadowSettings settings;
	settings.minLocalTile = 64;
	const ShadowPlan plan =
		planShadows(lights, ShadowCameraInfo{}, wideFrustum(), settings);
	EXPECT_LE(plan.views.size(), MAX_SHADOW_MATRICES);
	int shadowed = 0;
	for (const auto& light : lights) {
		if (light.shadowMatrixBase >= 0) {
			shadowed++;
			EXPECT_LE(light.shadowMatrixBase + 6,
					  static_cast<int32_t>(MAX_SHADOW_MATRICES));
		}
	}
	EXPECT_EQ(shadowed, static_cast<int>(MAX_SHADOW_MATRICES / 6));
}
