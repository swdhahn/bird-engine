#ifndef BIRD_ENVIRONMENT_MAP_H
#define BIRD_ENVIRONMENT_MAP_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "../../util/Math.h"
#include "GpuLayouts.h"

namespace bird {

/**
 * Configuration for generating a physically-inspired procedural HDR sky
 * cubemap on the GPU without requiring an external .hdr image file.
 */
struct SkyboxProceduralParams {
	/** Direction toward the sun in world space (normalized before upload). */
	Vector3 sunDirection{0.4f, 0.7f, 0.5f};
	/** Linear RGB color of the sun disc and atmospheric halo. */
	Vector3 sunColor{1.0f, 0.95f, 0.85f};
	/** Linear HDR radiance multiplier for the sun disc and halo (>= 0). */
	float sunIntensity = 12.0f;
	/** Angular half-radius of the sun disc in radians, clamped to [0.005, 0.5]. */
	float sunAngularRadius = 0.035f;
	/** Linear HDR color at the sky zenith (+Y). */
	Vector3 zenithColor{0.12f, 0.30f, 0.68f};
	/** Linear HDR color along the horizon (Y = 0). */
	Vector3 horizonColor{0.62f, 0.76f, 0.92f};
	/** Linear HDR ground-hemisphere bounce color (-Y). */
	Vector3 groundColor{0.15f, 0.13f, 0.11f};
};

/**
 * Resolution settings for precomputed IBL cubemaps.
 *
 * Defaults balance reflection fidelity with fast startup precomputation on
 * both desktop OpenGL 4.1+ and OpenGL ES 3.0+.
 */
struct IBLQualitySettings {
	/** Face width/height in pixels of the HDR environment (skybox) cubemap. */
	uint32_t environmentSize = 512;
	/** Face width/height in pixels of the diffuse irradiance cubemap. */
	uint32_t irradianceSize = 32;
	/** Base mip face width/height of the specular prefiltered cubemap. */
	uint32_t prefilterSize = 128;
	/**
	 * Number of roughness mip levels in the prefiltered cubemap. Defaults to
	 * IBL_PREFILTER_MIP_LEVELS (5) to match MAX_REFLECTION_LOD in shaders.
	 */
	uint32_t prefilterMipLevels = IBL_PREFILTER_MIP_LEVELS;
};

/**
 * Pure CPU math helpers for Image-Based Lighting and cubemap capture.
 *
 * Kept separate from OpenGL calls so unit tests can verify cubemap face
 * orientations, equirectangular UV mapping, and roughness mip schedules
 * without an active graphics context.
 */
namespace ibl {

/** Resolution (width and height) of the shared 2D BRDF integration LUT. */
constexpr uint32_t kBrdfLutSize = 256;

/**
 * Returns the six 90-degree perspective * lookAt matrices used to render into
 * OpenGL cubemap faces in standard order: +X, -X, +Y, -Y, +Z, -Z.
 * @return Array of six combined projection * view matrices.
 */
std::array<Matrix4, 6> cubemapCaptureViewProjections();

/**
 * Converts a cubemap face index and normalized face coordinates into a unit
 * 3D direction vector following OpenGL cubemap face conventions.
 * @param face Face index in [0, 5] (+X, -X, +Y, -Y, +Z, -Z).
 * @param u Horizontal face coordinate in [-1, 1].
 * @param v Vertical face coordinate in [-1, 1].
 * @return Normalized 3D direction vector.
 * @throws std::out_of_range when face >= 6.
 */
Vector3 cubemapDirection(uint32_t face, float u, float v);

/**
 * Maps a 3D unit direction to equirectangular texture coordinates in [0, 1]^2,
 * matching assets/shaders/ibl_equirect_to_cube.frag.
 * @param direction 3D direction vector (normalized internally).
 * @return Equirectangular UV coordinates.
 */
Vector2 directionToEquirectangularUV(const Vector3& direction);

/**
 * Computes the perceptual roughness assigned to a prefilter mip level.
 * Mip 0 maps to roughness 0.0 (mirror); mip (mipCount - 1) maps to 1.0.
 * @param mipLevel Zero-based mip index.
 * @param mipCount Total number of prefilter mip levels (>= 1).
 * @return Roughness in [0.0, 1.0].
 */
float prefilterRoughnessForMip(uint32_t mipLevel, uint32_t mipCount);

/**
 * Validates and clamps procedural sky parameters (normalizing sunDirection,
 * clamping negative colors/intensities and sunAngularRadius) and packs them
 * into a std140 ProceduralSkyUBOData block.
 * @param params User-supplied procedural sky parameters.
 * @return Sanitized std140 UBO payload ready for GPU upload.
 */
ProceduralSkyUBOData packProceduralSkyUBO(const SkyboxProceduralParams& params);

/**
 * Clamps IBLQualitySettings fields to safe positive bounds.
 * @param settings User-supplied quality settings.
 * @return Sanitized quality settings.
 */
IBLQualitySettings sanitizeQualitySettings(const IBLQualitySettings& settings);

}  // namespace ibl

/**
 * Backend-agnostic Image-Based Lighting (IBL) environment resource.
 *
 * Encapsulates three precomputed GPU textures for a scene environment:
 *  - HDR environment cubemap (rendered by the skybox pass)
 *  - Diffuse irradiance cubemap (hemisphere cosine convolution)
 *  - Specular prefiltered cubemap (mipmapped by perceptual roughness via GGX
 *    importance sampling)
 *
 * The 2D split-sum BRDF integration LUT is environment-independent and is
 * generated once and shared across all EnvironmentMap instances.
 *
 * Assign an EnvironmentMap to Camera::getWorldEnvironment().environmentMap to
 * enable skybox rendering and IBL reflections in both the deferred PBR
 * lighting pass and forward PBR pass.
 */
class EnvironmentMap {
   public:
	virtual ~EnvironmentMap() = default;

	/** @return Face resolution in pixels of the environment (skybox) cubemap. */
	uint32_t getEnvironmentResolution() const { return m_environmentSize; }

	/** @return Face resolution in pixels of the diffuse irradiance cubemap. */
	uint32_t getIrradianceResolution() const { return m_irradianceSize; }

	/** @return Base mip resolution in pixels of the prefiltered cubemap. */
	uint32_t getPrefilterResolution() const { return m_prefilterSize; }

	/** @return Number of mip levels in the prefiltered specular cubemap. */
	uint32_t getPrefilterMipLevels() const { return m_prefilterMipLevels; }

	/**
	 * Generates a procedural HDR sky cubemap on the GPU and precomputes its
	 * diffuse irradiance and GGX specular prefilter maps.
	 *
	 * Requires an initialized GraphicsPipeline and ResourceManager.
	 *
	 * @param params Sky gradient, ground bounce, and sun parameters.
	 * @param quality Optional cubemap resolution overrides.
	 * @return Shared EnvironmentMap ready for WorldEnvironment::environmentMap.
	 * @throws std::runtime_error if the active backend lacks IBL support.
	 */
	static std::shared_ptr<EnvironmentMap> createProcedural(
		const SkyboxProceduralParams& params = SkyboxProceduralParams{},
		const IBLQualitySettings& quality = IBLQualitySettings{});

	/**
	 * Loads an HDR equirectangular image from disk (e.g. Radiance .hdr via
	 * stb_image), projects it into an HDR cubemap, and precomputes diffuse
	 * irradiance and specular prefilter maps.
	 *
	 * @param hdrImagePath Path to an equirectangular image readable by VFS.
	 * @param quality Optional cubemap resolution overrides.
	 * @return Shared EnvironmentMap ready for WorldEnvironment::environmentMap.
	 * @throws std::runtime_error on missing/unreadable image or unsupported
	 *         backend.
	 */
	static std::shared_ptr<EnvironmentMap> createFromEquirectangular(
		const std::string& hdrImagePath,
		const IBLQualitySettings& quality = IBLQualitySettings{});

	/**
	 * Builds an EnvironmentMap from in-memory floating-point equirectangular
	 * RGB or RGBA pixel data and precomputes its IBL maps.
	 *
	 * @param floatPixels Tightly packed linear HDR float pixels.
	 * @param width Image width in pixels (> 0).
	 * @param height Image height in pixels (> 0).
	 * @param channels Number of channels per pixel (3 for RGB, 4 for RGBA).
	 * @param quality Optional cubemap resolution overrides.
	 * @return Shared EnvironmentMap.
	 * @throws std::invalid_argument on null pointer, zero dimensions, or
	 *         unsupported channel count.
	 */
	static std::shared_ptr<EnvironmentMap> createFromEquirectangularPixels(
		const float* floatPixels, uint32_t width, uint32_t height,
		uint32_t channels,
		const IBLQualitySettings& quality = IBLQualitySettings{});

	/**
	 * Loads six individual cubemap face images (+X, -X, +Y, -Y, +Z, -Z) from
	 * disk (LDR sRGB or HDR float) and precomputes diffuse irradiance and
	 * specular prefilter maps.
	 *
	 * @param facePaths Six image paths in OpenGL cubemap order
	 *        (+X, -X, +Y, -Y, +Z, -Z).
	 * @param quality Optional cubemap resolution overrides.
	 * @return Shared EnvironmentMap.
	 * @throws std::runtime_error on missing/mismatched face images or
	 *         unsupported backend.
	 */
	static std::shared_ptr<EnvironmentMap> createFromCubemapFaces(
		const std::array<std::string, 6>& facePaths,
		const IBLQualitySettings& quality = IBLQualitySettings{});

   protected:
	EnvironmentMap(uint32_t envSize, uint32_t irrSize, uint32_t prefSize,
				   uint32_t prefMips)
		: m_environmentSize(envSize),
		  m_irradianceSize(irrSize),
		  m_prefilterSize(prefSize),
		  m_prefilterMipLevels(prefMips) {}

	uint32_t m_environmentSize;
	uint32_t m_irradianceSize;
	uint32_t m_prefilterSize;
	uint32_t m_prefilterMipLevels;
};

}  // namespace bird

#endif	// BIRD_ENVIRONMENT_MAP_H
