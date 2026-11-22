#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "renders/generic/EnvironmentMap.h"
#include "scene/Camera.h"

using namespace bird;

TEST(IBLMath, CubemapCaptureMatricesPointAlongSixCardinals) {
	const auto vps = ibl::cubemapCaptureViewProjections();
	const Vector3 expectedDirs[6] = {
		{1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
		{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f},
	};
	for (size_t face = 0; face < 6; face++) {
		const Vector4 clip = vps[face] * Vector4(expectedDirs[face], 1.0f);
		ASSERT_GT(clip.w, 0.0f);
		const Vector3 ndc = Vector3(clip) / clip.w;
		EXPECT_NEAR(ndc.x, 0.0f, 1e-4f);
		EXPECT_NEAR(ndc.y, 0.0f, 1e-4f);
		EXPECT_GE(ndc.z, -1.0f);
		EXPECT_LE(ndc.z, 1.0f);
	}
}

TEST(IBLMath, CubemapDirectionMapsCenterOfEachFace) {
	EXPECT_NEAR(glm::distance(ibl::cubemapDirection(0, 0.0f, 0.0f),
							  Vector3(1.0f, 0.0f, 0.0f)),
				0.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(ibl::cubemapDirection(1, 0.0f, 0.0f),
							  Vector3(-1.0f, 0.0f, 0.0f)),
				0.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(ibl::cubemapDirection(2, 0.0f, 0.0f),
							  Vector3(0.0f, 1.0f, 0.0f)),
				0.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(ibl::cubemapDirection(3, 0.0f, 0.0f),
							  Vector3(0.0f, -1.0f, 0.0f)),
				0.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(ibl::cubemapDirection(4, 0.0f, 0.0f),
							  Vector3(0.0f, 0.0f, 1.0f)),
				0.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(ibl::cubemapDirection(5, 0.0f, 0.0f),
							  Vector3(0.0f, 0.0f, -1.0f)),
				0.0f, 1e-5f);
	EXPECT_THROW(ibl::cubemapDirection(6, 0.0f, 0.0f), std::out_of_range);
}

TEST(IBLMath, EquirectangularUVCoversPolesAndHorizon) {
	const Vector2 northPole =
		ibl::directionToEquirectangularUV(Vector3(0.0f, 1.0f, 0.0f));
	const Vector2 southPole =
		ibl::directionToEquirectangularUV(Vector3(0.0f, -1.0f, 0.0f));
	const Vector2 plusX =
		ibl::directionToEquirectangularUV(Vector3(1.0f, 0.0f, 0.0f));
	EXPECT_NEAR(northPole.y, 1.0f, 1e-5f);
	EXPECT_NEAR(southPole.y, 0.0f, 1e-5f);
	EXPECT_NEAR(plusX.x, 0.5f, 1e-5f);
	EXPECT_NEAR(plusX.y, 0.5f, 1e-5f);
}

TEST(IBLMath, PrefilterRoughnessSpansZeroToOne) {
	EXPECT_FLOAT_EQ(ibl::prefilterRoughnessForMip(0, 5), 0.0f);
	EXPECT_FLOAT_EQ(ibl::prefilterRoughnessForMip(1, 5), 0.25f);
	EXPECT_FLOAT_EQ(ibl::prefilterRoughnessForMip(2, 5), 0.5f);
	EXPECT_FLOAT_EQ(ibl::prefilterRoughnessForMip(3, 5), 0.75f);
	EXPECT_FLOAT_EQ(ibl::prefilterRoughnessForMip(4, 5), 1.0f);
	EXPECT_FLOAT_EQ(ibl::prefilterRoughnessForMip(99, 5), 1.0f);
	EXPECT_FLOAT_EQ(ibl::prefilterRoughnessForMip(0, 1), 0.0f);
}

TEST(IBLMath, ProceduralSkySanitizesAndPacksUBO) {
	SkyboxProceduralParams params;
	params.sunDirection = Vector3(0.0f, 0.0f, 0.0f);  // degenerate
	params.sunIntensity = -5.0f;
	params.sunAngularRadius = 0.0f;
	params.zenithColor = Vector3(-1.0f, 0.5f, 1.0f);

	const ProceduralSkyUBOData ubo = ibl::packProceduralSkyUBO(params);
	// Degenerate sun direction falls back to +Y.
	EXPECT_NEAR(glm::length(Vector3(ubo.sunDirAndCosRadius)), 1.0f, 1e-5f);
	EXPECT_NEAR(ubo.sunDirAndCosRadius.y, 1.0f, 1e-5f);
	EXPECT_FLOAT_EQ(ubo.sunColorAndIntensity.w, 0.0f);
	EXPECT_FLOAT_EQ(ubo.zenithColor.x, 0.0f);
	EXPECT_FLOAT_EQ(ubo.zenithColor.y, 0.5f);
	EXPECT_NEAR(ubo.sunDirAndCosRadius.w, std::cos(0.005f), 1e-5f);
}

TEST(IBLMath, QualitySettingsClampedToSafeBounds) {
	IBLQualitySettings raw;
	raw.environmentSize = 0;
	raw.irradianceSize = 9999;
	raw.prefilterSize = 1;
	raw.prefilterMipLevels = 0;

	const IBLQualitySettings safe = ibl::sanitizeQualitySettings(raw);
	EXPECT_EQ(safe.environmentSize, 16u);
	EXPECT_EQ(safe.irradianceSize, 256u);
	EXPECT_EQ(safe.prefilterSize, 16u);
	EXPECT_EQ(safe.prefilterMipLevels, 1u);
}

TEST(IBLMath, WorldEnvironmentDefaults) {
	WorldEnvironment env;
	EXPECT_EQ(env.environmentMap, nullptr);
	EXPECT_FLOAT_EQ(env.iblIntensity, 1.0f);
	EXPECT_TRUE(env.renderSkybox);
}
