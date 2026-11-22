#include "EnvironmentMap.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "../GraphicsConstants.h"
#include "../opengl/GLIBLEnvironment.h"

namespace bird {

namespace ibl {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

std::array<Matrix4, 6> cubemapCaptureViewProjections() {
	const Matrix4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	const Vector3 origin(0.0f);
	return {
		proj * glm::lookAt(origin, Vector3(1.0f, 0.0f, 0.0f),
						   Vector3(0.0f, -1.0f, 0.0f)),
		proj * glm::lookAt(origin, Vector3(-1.0f, 0.0f, 0.0f),
						   Vector3(0.0f, -1.0f, 0.0f)),
		proj * glm::lookAt(origin, Vector3(0.0f, 1.0f, 0.0f),
						   Vector3(0.0f, 0.0f, 1.0f)),
		proj * glm::lookAt(origin, Vector3(0.0f, -1.0f, 0.0f),
						   Vector3(0.0f, 0.0f, -1.0f)),
		proj * glm::lookAt(origin, Vector3(0.0f, 0.0f, 1.0f),
						   Vector3(0.0f, -1.0f, 0.0f)),
		proj * glm::lookAt(origin, Vector3(0.0f, 0.0f, -1.0f),
						   Vector3(0.0f, -1.0f, 0.0f))};
}

Vector3 cubemapDirection(uint32_t face, float u, float v) {
	Vector3 dir(0.0f);
	switch (face) {
		case 0:
			dir = Vector3(1.0f, -v, -u);
			break;
		case 1:
			dir = Vector3(-1.0f, -v, u);
			break;
		case 2:
			dir = Vector3(u, 1.0f, v);
			break;
		case 3:
			dir = Vector3(u, -1.0f, -v);
			break;
		case 4:
			dir = Vector3(u, -v, 1.0f);
			break;
		case 5:
			dir = Vector3(-u, -v, -1.0f);
			break;
		default:
			throw std::out_of_range(
				"ibl::cubemapDirection: face index must be in [0, 5].");
	}
	return glm::normalize(dir);
}

Vector2 directionToEquirectangularUV(const Vector3& direction) {
	const float len = glm::length(direction);
	const Vector3 v = len > 0.0f ? (direction / len) : Vector3(0.0f, 0.0f, 1.0f);
	const float u = std::atan2(v.z, v.x) / (2.0f * kPi) + 0.5f;
	const float vCoord = std::asin(std::clamp(v.y, -1.0f, 1.0f)) / kPi + 0.5f;
	return Vector2(u, vCoord);
}

float prefilterRoughnessForMip(uint32_t mipLevel, uint32_t mipCount) {
	if (mipCount <= 1) return 0.0f;
	const uint32_t clamped = std::min(mipLevel, mipCount - 1);
	return static_cast<float>(clamped) / static_cast<float>(mipCount - 1);
}

ProceduralSkyUBOData packProceduralSkyUBO(const SkyboxProceduralParams& params) {
	const float sunLen = glm::length(params.sunDirection);
	const Vector3 sunDir = sunLen > 1e-5f ? (params.sunDirection / sunLen)
										  : Vector3(0.0f, 1.0f, 0.0f);
	const float clampedRadius = std::clamp(params.sunAngularRadius, 0.005f, 0.5f);
	const float intensity = std::max(params.sunIntensity, 0.0f);
	const Vector3 sunColor = glm::max(params.sunColor, Vector3(0.0f));
	const Vector3 zenith = glm::max(params.zenithColor, Vector3(0.0f));
	const Vector3 horizon = glm::max(params.horizonColor, Vector3(0.0f));
	const Vector3 ground = glm::max(params.groundColor, Vector3(0.0f));

	ProceduralSkyUBOData data{};
	data.sunDirAndCosRadius = Vector4(sunDir, std::cos(clampedRadius));
	data.sunColorAndIntensity = Vector4(sunColor, intensity);
	data.zenithColor = Vector4(zenith, 0.0f);
	data.horizonColor = Vector4(horizon, 0.0f);
	data.groundColor = Vector4(ground, 0.0f);
	return data;
}

IBLQualitySettings sanitizeQualitySettings(const IBLQualitySettings& settings) {
	IBLQualitySettings out = settings;
	out.environmentSize = std::clamp(out.environmentSize, 16u, 2048u);
	out.irradianceSize = std::clamp(out.irradianceSize, 8u, 256u);
	out.prefilterSize = std::clamp(out.prefilterSize, 16u, 1024u);
	out.prefilterMipLevels = std::clamp(out.prefilterMipLevels, 1u, 8u);
	return out;
}

}  // namespace ibl

std::shared_ptr<EnvironmentMap> EnvironmentMap::createProcedural(
	const SkyboxProceduralParams& params, const IBLQualitySettings& quality) {
	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			return gl::GLIBLEnvironment::createProcedural(params, quality);
		default:
			throw std::runtime_error(
				"EnvironmentMap::createProcedural: unsupported graphics "
				"backend.");
	}
}

std::shared_ptr<EnvironmentMap> EnvironmentMap::createFromEquirectangular(
	const std::string& hdrImagePath, const IBLQualitySettings& quality) {
	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			return gl::GLIBLEnvironment::createFromEquirectangular(hdrImagePath,
																   quality);
		default:
			throw std::runtime_error(
				"EnvironmentMap::createFromEquirectangular: unsupported "
				"graphics backend.");
	}
}

std::shared_ptr<EnvironmentMap> EnvironmentMap::createFromEquirectangularPixels(
	const float* floatPixels, uint32_t width, uint32_t height,
	uint32_t channels, const IBLQualitySettings& quality) {
	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			return gl::GLIBLEnvironment::createFromEquirectangularPixels(
				floatPixels, width, height, channels, quality);
		default:
			throw std::runtime_error(
				"EnvironmentMap::createFromEquirectangularPixels: unsupported "
				"graphics backend.");
	}
}

std::shared_ptr<EnvironmentMap> EnvironmentMap::createFromCubemapFaces(
	const std::array<std::string, 6>& facePaths,
	const IBLQualitySettings& quality) {
	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			return gl::GLIBLEnvironment::createFromCubemapFaces(facePaths,
																quality);
		default:
			throw std::runtime_error(
				"EnvironmentMap::createFromCubemapFaces: unsupported graphics "
				"backend.");
	}
}

}  // namespace bird
