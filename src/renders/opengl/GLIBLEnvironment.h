#ifndef BIRD_GL_IBL_ENVIRONMENT_H
#define BIRD_GL_IBL_ENVIRONMENT_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "../generic/EnvironmentMap.h"

namespace bird::gl {

/**
 * OpenGL 4.1+ / OpenGL ES 3.0+ implementation of EnvironmentMap.
 *
 * Owns three RGBA16F cubemaps (environment/skybox, diffuse irradiance, and
 * roughness-mipmapped GGX specular prefilter) and shares a single RG16F
 * split-sum BRDF integration LUT across all environments.
 */
class GLIBLEnvironment : public EnvironmentMap {
   public:
	~GLIBLEnvironment() override;

	GLIBLEnvironment(const GLIBLEnvironment&) = delete;
	GLIBLEnvironment& operator=(const GLIBLEnvironment&) = delete;

	/** @return OpenGL texture ID of the HDR environment (skybox) cubemap. */
	uint32_t getEnvironmentCubemapId() const { return m_environmentCubemap; }

	/** @return OpenGL texture ID of the diffuse irradiance cubemap. */
	uint32_t getIrradianceCubemapId() const { return m_irradianceCubemap; }

	/** @return OpenGL texture ID of the specular prefiltered cubemap. */
	uint32_t getPrefilterCubemapId() const { return m_prefilterCubemap; }

	/**
	 * Generates a procedural HDR sky cubemap and precomputes its irradiance
	 * and GGX specular prefilter maps.
	 * @param params Sky gradient, ground bounce, and sun parameters.
	 * @param quality Cubemap resolution settings.
	 */
	static std::shared_ptr<GLIBLEnvironment> createProcedural(
		const SkyboxProceduralParams& params, const IBLQualitySettings& quality);

	/**
	 * Loads an HDR equirectangular image via VFS + stb_image, projects it into
	 * a cubemap, and precomputes its irradiance and prefilter maps.
	 * @param hdrImagePath Asset path to the equirectangular image.
	 * @param quality Cubemap resolution settings.
	 */
	static std::shared_ptr<GLIBLEnvironment> createFromEquirectangular(
		const std::string& hdrImagePath, const IBLQualitySettings& quality);

	/**
	 * Builds an IBL environment from in-memory float equirectangular pixels.
	 * @param floatPixels Tightly packed linear HDR RGB or RGBA float data.
	 * @param width Image width in pixels.
	 * @param height Image height in pixels.
	 * @param channels 3 (RGB) or 4 (RGBA).
	 * @param quality Cubemap resolution settings.
	 */
	static std::shared_ptr<GLIBLEnvironment> createFromEquirectangularPixels(
		const float* floatPixels, uint32_t width, uint32_t height,
		uint32_t channels, const IBLQualitySettings& quality);

	/**
	 * Loads six cubemap face images (+X, -X, +Y, -Y, +Z, -Z) via VFS +
	 * stb_image and precomputes irradiance and specular prefilter maps.
	 * @param facePaths Six face image paths in OpenGL cubemap order.
	 * @param quality Cubemap resolution settings.
	 */
	static std::shared_ptr<GLIBLEnvironment> createFromCubemapFaces(
		const std::array<std::string, 6>& facePaths,
		const IBLQualitySettings& quality);

	/**
	 * Binds the IBL textures for PBR shading:
	 *  - SAMPLER_BINDING_IBL_IRRADIANCE (10): diffuse irradiance cubemap
	 *  - SAMPLER_BINDING_IBL_PREFILTER  (11): specular prefiltered cubemap
	 *  - SAMPLER_BINDING_IBL_BRDF_LUT   (12): shared 2D BRDF integration LUT
	 *
	 * When env is nullptr, binds 1x1 black fallback textures so samplers remain
	 * valid across desktop GL and OpenGL ES 3.0.
	 *
	 * @param env Active environment map, or nullptr.
	 */
	static void bindForSampling(const EnvironmentMap* env);

	/**
	 * Releases process-wide shared OpenGL resources (shared BRDF LUT, unit
	 * cube VAO, fallback textures). Call during GLPipeline::cleanUp().
	 */
	static void releaseSharedResources();

	/** Draws a unit cube [-1, 1]^3 using the shared internal VAO. */
	static void drawUnitCube();

   private:
	GLIBLEnvironment(uint32_t envTex, uint32_t irrTex, uint32_t prefTex,
					 const IBLQualitySettings& quality);

	static std::shared_ptr<GLIBLEnvironment> bakeFromEnvironmentCubemap(
		uint32_t envCubemap, const IBLQualitySettings& quality);

	static uint32_t ensureBrdfLut();
	static uint32_t ensureFallbackCubemap();
	static uint32_t ensureFallback2D();

	uint32_t m_environmentCubemap = 0;
	uint32_t m_irradianceCubemap = 0;
	uint32_t m_prefilterCubemap = 0;
};

}  // namespace bird::gl

#endif	// BIRD_GL_IBL_ENVIRONMENT_H
