#include "GLIBLEnvironment.h"

#include "GLApi.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

#include "../../Application.h"
#include "../../platform/VFS.h"
#include "../../util/ResourceManager.h"
#include "../../util/stb_image.h"
#include "../generic/Buffer.h"
#include "GLDrawCommands.h"
#include "GLShader.h"

namespace bird::gl {

namespace {

// 36 position-only vertices for a unit cube [-1, 1]^3.
// clang-format off
constexpr float kUnitCubeVertices[] = {
	// -Z face
	-1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
	// -X face
	-1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
	// +X face
	 1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
	// +Z face
	-1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
	// +Y face
	-1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
	// -Y face
	-1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
};
// clang-format on

struct SharedIBLResources {
	uint32_t cubeVao = 0;
	uint32_t cubeVbo = 0;
	uint32_t brdfLut = 0;
	uint32_t fallbackCubemap = 0;
	uint32_t fallback2D = 0;
};

SharedIBLResources s_shared;

void ensureCubeGeometry() {
	if (s_shared.cubeVao != 0) return;
	glGenVertexArrays(1, &s_shared.cubeVao);
	glGenBuffers(1, &s_shared.cubeVbo);
	glBindVertexArray(s_shared.cubeVao);
	glBindBuffer(GL_ARRAY_BUFFER, s_shared.cubeVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(kUnitCubeVertices), kUnitCubeVertices,
				 GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/** Saves and restores GL framebuffer, viewport, and key raster state. */
struct ScopedBakeState {
	GLint prevFbo = 0;
	GLint prevViewport[4] = {0, 0, 0, 0};
	GLboolean depthEnabled = GL_FALSE;
	GLboolean cullEnabled = GL_FALSE;
	GLboolean blendEnabled = GL_FALSE;

	ScopedBakeState() {
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
		glGetIntegerv(GL_VIEWPORT, prevViewport);
		depthEnabled = glIsEnabled(GL_DEPTH_TEST);
		cullEnabled = glIsEnabled(GL_CULL_FACE);
		blendEnabled = glIsEnabled(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
#ifdef GL_TEXTURE_CUBE_MAP_SEAMLESS
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
	}

	~ScopedBakeState() {
		glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
		glViewport(prevViewport[0], prevViewport[1], prevViewport[2],
				   prevViewport[3]);
		if (depthEnabled) glEnable(GL_DEPTH_TEST);
		if (cullEnabled) glEnable(GL_CULL_FACE);
		if (blendEnabled) glEnable(GL_BLEND);
	}
};

uint32_t allocateCubemap(uint32_t size, bool mipmapped) {
	uint32_t tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	for (uint32_t face = 0; face < 6; face++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F,
					 static_cast<GLsizei>(size), static_cast<GLsizei>(size), 0,
					 GL_RGBA, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
					mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (mipmapped) {
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	return tex;
}

ResourceManager& requireResourceManager() {
	if (RESOURCE_MANAGER == nullptr) {
		throw std::logic_error(
			"GLIBLEnvironment: ResourceManager must be initialized first.");
	}
	return *RESOURCE_MANAGER;
}

}  // namespace

GLIBLEnvironment::GLIBLEnvironment(uint32_t envTex, uint32_t irrTex,
								   uint32_t prefTex,
								   const IBLQualitySettings& quality)
	: EnvironmentMap(quality.environmentSize, quality.irradianceSize,
					 quality.prefilterSize, quality.prefilterMipLevels),
	  m_environmentCubemap(envTex),
	  m_irradianceCubemap(irrTex),
	  m_prefilterCubemap(prefTex) {}

GLIBLEnvironment::~GLIBLEnvironment() {
	if (m_environmentCubemap != 0) glDeleteTextures(1, &m_environmentCubemap);
	if (m_irradianceCubemap != 0) glDeleteTextures(1, &m_irradianceCubemap);
	if (m_prefilterCubemap != 0) glDeleteTextures(1, &m_prefilterCubemap);
}

void GLIBLEnvironment::drawUnitCube() {
	ensureCubeGeometry();
	glBindVertexArray(s_shared.cubeVao);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

uint32_t GLIBLEnvironment::ensureBrdfLut() {
	if (s_shared.brdfLut != 0) return s_shared.brdfLut;

	ResourceManager& resources = requireResourceManager();
	auto shader = resources.loadShader("assets/shaders/passthrough.vert",
									   "assets/shaders/ibl_brdf_lut.frag");

	ScopedBakeState saved;
	uint32_t lut = 0;
	glGenTextures(1, &lut);
	glBindTexture(GL_TEXTURE_2D, lut);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, ibl::kBrdfLutSize,
				 ibl::kBrdfLutSize, 0, GL_RG, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	uint32_t fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
						   lut, 0);
	glViewport(0, 0, ibl::kBrdfLutSize, ibl::kBrdfLutSize);
	glClear(GL_COLOR_BUFFER_BIT);

	static_cast<GLShader&>(*shader).bind();
	drawFullscreenQuad();

	glDeleteFramebuffers(1, &fbo);
	glBindTexture(GL_TEXTURE_2D, 0);

	s_shared.brdfLut = lut;
	return s_shared.brdfLut;
}

uint32_t GLIBLEnvironment::ensureFallbackCubemap() {
	if (s_shared.fallbackCubemap != 0) return s_shared.fallbackCubemap;
	const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	uint32_t tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	for (uint32_t face = 0; face < 6; face++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F, 1, 1,
					 0, GL_RGBA, GL_FLOAT, black);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	s_shared.fallbackCubemap = tex;
	return s_shared.fallbackCubemap;
}

uint32_t GLIBLEnvironment::ensureFallback2D() {
	if (s_shared.fallback2D != 0) return s_shared.fallback2D;
	const float zero[2] = {0.0f, 0.0f};
	uint32_t tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 1, 1, 0, GL_RG, GL_FLOAT, zero);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	s_shared.fallback2D = tex;
	return s_shared.fallback2D;
}

void GLIBLEnvironment::bindForSampling(const EnvironmentMap* env) {
	if (env != nullptr) {
		const auto& glEnv = static_cast<const GLIBLEnvironment&>(*env);
		const uint32_t brdfLut = ensureBrdfLut();
		glActiveTexture(GL_TEXTURE0 + SAMPLER_BINDING_IBL_IRRADIANCE);
		glBindTexture(GL_TEXTURE_CUBE_MAP, glEnv.m_irradianceCubemap);
		glActiveTexture(GL_TEXTURE0 + SAMPLER_BINDING_IBL_PREFILTER);
		glBindTexture(GL_TEXTURE_CUBE_MAP, glEnv.m_prefilterCubemap);
		glActiveTexture(GL_TEXTURE0 + SAMPLER_BINDING_IBL_BRDF_LUT);
		glBindTexture(GL_TEXTURE_2D, brdfLut);
	} else {
		const uint32_t fallbackCube = ensureFallbackCubemap();
		const uint32_t fallback2D = ensureFallback2D();
		glActiveTexture(GL_TEXTURE0 + SAMPLER_BINDING_IBL_IRRADIANCE);
		glBindTexture(GL_TEXTURE_CUBE_MAP, fallbackCube);
		glActiveTexture(GL_TEXTURE0 + SAMPLER_BINDING_IBL_PREFILTER);
		glBindTexture(GL_TEXTURE_CUBE_MAP, fallbackCube);
		glActiveTexture(GL_TEXTURE0 + SAMPLER_BINDING_IBL_BRDF_LUT);
		glBindTexture(GL_TEXTURE_2D, fallback2D);
	}
}

void GLIBLEnvironment::releaseSharedResources() {
	if (s_shared.cubeVao != 0) {
		glDeleteVertexArrays(1, &s_shared.cubeVao);
		s_shared.cubeVao = 0;
	}
	if (s_shared.cubeVbo != 0) {
		glDeleteBuffers(1, &s_shared.cubeVbo);
		s_shared.cubeVbo = 0;
	}
	if (s_shared.brdfLut != 0) {
		glDeleteTextures(1, &s_shared.brdfLut);
		s_shared.brdfLut = 0;
	}
	if (s_shared.fallbackCubemap != 0) {
		glDeleteTextures(1, &s_shared.fallbackCubemap);
		s_shared.fallbackCubemap = 0;
	}
	if (s_shared.fallback2D != 0) {
		glDeleteTextures(1, &s_shared.fallback2D);
		s_shared.fallback2D = 0;
	}
}

std::shared_ptr<GLIBLEnvironment> GLIBLEnvironment::bakeFromEnvironmentCubemap(
	uint32_t envCubemap, const IBLQualitySettings& rawQuality) {
	const IBLQualitySettings quality = ibl::sanitizeQualitySettings(rawQuality);
	ResourceManager& resources = requireResourceManager();

	// Ensure the environment cubemap has a complete mip chain so GGX prefilter
	// importance sampling can sample filtered mips and avoid fireflies.
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
					GL_LINEAR_MIPMAP_LINEAR);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	const auto captureVPs = ibl::cubemapCaptureViewProjections();
	auto irradianceShader =
		resources.loadShader("assets/shaders/ibl_cubemap.vert",
							 "assets/shaders/ibl_irradiance.frag");
	auto prefilterShader =
		resources.loadShader("assets/shaders/ibl_cubemap.vert",
							 "assets/shaders/ibl_prefilter.frag");

	ScopedBakeState saved;
	uint32_t fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// 1. Diffuse irradiance cubemap convolution.
	const uint32_t irrCubemap = allocateCubemap(quality.irradianceSize, false);
	glViewport(0, 0, static_cast<GLsizei>(quality.irradianceSize),
			   static_cast<GLsizei>(quality.irradianceSize));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	auto& irrGL = static_cast<GLShader&>(*irradianceShader);
	irrGL.bind();
	for (uint32_t face = 0; face < 6; face++) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
							   irrCubemap, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		const IBLCubemapPushConstants push{captureVPs[face]};
		irrGL.setPushConstants(&push, sizeof(push));
		drawUnitCube();
	}

	// 2. Specular prefiltered cubemap (mipmapped by perceptual roughness).
	const uint32_t prefCubemap = allocateCubemap(quality.prefilterSize, true);
	auto& prefGL = static_cast<GLShader&>(*prefilterShader);
	prefGL.bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	for (uint32_t mip = 0; mip < quality.prefilterMipLevels; mip++) {
		const uint32_t mipSize = std::max(1u, quality.prefilterSize >> mip);
		glViewport(0, 0, static_cast<GLsizei>(mipSize),
				   static_cast<GLsizei>(mipSize));
		const float roughness =
			ibl::prefilterRoughnessForMip(mip, quality.prefilterMipLevels);
		for (uint32_t face = 0; face < 6; face++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
								   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
								   prefCubemap, static_cast<GLint>(mip));
			glClear(GL_COLOR_BUFFER_BIT);
			const IBLPrefilterPushConstants push{
				captureVPs[face],
				Vector4(roughness, static_cast<float>(quality.environmentSize),
						0.0f, 0.0f)};
			prefGL.setPushConstants(&push, sizeof(push));
			drawUnitCube();
		}
	}

	glDeleteFramebuffers(1, &fbo);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	// Warm the shared BRDF LUT once while baking.
	ensureBrdfLut();

	return std::shared_ptr<GLIBLEnvironment>(
		new GLIBLEnvironment(envCubemap, irrCubemap, prefCubemap, quality));
}

std::shared_ptr<GLIBLEnvironment> GLIBLEnvironment::createProcedural(
	const SkyboxProceduralParams& params, const IBLQualitySettings& rawQuality) {
	const IBLQualitySettings quality = ibl::sanitizeQualitySettings(rawQuality);
	ResourceManager& resources = requireResourceManager();
	auto skyShader =
		resources.loadShader("assets/shaders/ibl_cubemap.vert",
							 "assets/shaders/ibl_procedural_sky.frag");

	auto uboData = std::make_unique<ProceduralSkyUBOData[]>(1);
	uboData[0] = ibl::packProceduralSkyUBO(params);
	auto skyUBO = BufferBuilder<ProceduralSkyUBOData>(BUFFER_UNIFORM |
													  BUFFER_STAGED)
					  .data(std::move(uboData), 1)
					  .bindingPoint(UBO_BINDING_MATERIAL)
					  .build();
	skyUBO->bindToBindingPoint();

	const uint32_t envCubemap = allocateCubemap(quality.environmentSize, true);
	const auto captureVPs = ibl::cubemapCaptureViewProjections();

	{
		ScopedBakeState saved;
		uint32_t fbo = 0;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(0, 0, static_cast<GLsizei>(quality.environmentSize),
				   static_cast<GLsizei>(quality.environmentSize));
		auto& skyGL = static_cast<GLShader&>(*skyShader);
		skyGL.bind();
		for (uint32_t face = 0; face < 6; face++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
								   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
								   envCubemap, 0);
			glClear(GL_COLOR_BUFFER_BIT);
			const IBLCubemapPushConstants push{captureVPs[face]};
			skyGL.setPushConstants(&push, sizeof(push));
			drawUnitCube();
		}
		glDeleteFramebuffers(1, &fbo);
	}

	return bakeFromEnvironmentCubemap(envCubemap, quality);
}

std::shared_ptr<GLIBLEnvironment>
GLIBLEnvironment::createFromEquirectangularPixels(
	const float* floatPixels, uint32_t width, uint32_t height,
	uint32_t channels, const IBLQualitySettings& rawQuality) {
	if (floatPixels == nullptr || width == 0 || height == 0) {
		throw std::invalid_argument(
			"GLIBLEnvironment::createFromEquirectangularPixels: valid pixel "
			"buffer and non-zero dimensions are required.");
	}
	if (channels != 3 && channels != 4) {
		throw std::invalid_argument(
			"GLIBLEnvironment::createFromEquirectangularPixels: channels must "
			"be 3 (RGB) or 4 (RGBA).");
	}

	const IBLQualitySettings quality = ibl::sanitizeQualitySettings(rawQuality);
	ResourceManager& resources = requireResourceManager();
	auto equirectShader =
		resources.loadShader("assets/shaders/ibl_cubemap.vert",
							 "assets/shaders/ibl_equirect_to_cube.frag");

	// Expand RGB -> RGBA so GL_RGBA16F + GL_RGBA + GL_FLOAT works uniformly on
	// both desktop OpenGL 4.1 and OpenGL ES 3.0 / WebGL 2.
	std::vector<float> rgba;
	const float* uploadPtr = floatPixels;
	if (channels == 3) {
		const size_t pixelCount = static_cast<size_t>(width) * height;
		rgba.resize(pixelCount * 4);
		for (size_t i = 0; i < pixelCount; i++) {
			rgba[i * 4 + 0] = floatPixels[i * 3 + 0];
			rgba[i * 4 + 1] = floatPixels[i * 3 + 1];
			rgba[i * 4 + 2] = floatPixels[i * 3 + 2];
			rgba[i * 4 + 3] = 1.0f;
		}
		uploadPtr = rgba.data();
	}

	uint32_t equirectTex = 0;
	glGenTextures(1, &equirectTex);
	glBindTexture(GL_TEXTURE_2D, equirectTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width),
				 static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, uploadPtr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	const uint32_t envCubemap = allocateCubemap(quality.environmentSize, true);
	const auto captureVPs = ibl::cubemapCaptureViewProjections();

	{
		ScopedBakeState saved;
		uint32_t fbo = 0;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(0, 0, static_cast<GLsizei>(quality.environmentSize),
				   static_cast<GLsizei>(quality.environmentSize));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, equirectTex);
		auto& eqGL = static_cast<GLShader&>(*equirectShader);
		eqGL.bind();
		for (uint32_t face = 0; face < 6; face++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
								   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
								   envCubemap, 0);
			glClear(GL_COLOR_BUFFER_BIT);
			const IBLCubemapPushConstants push{captureVPs[face]};
			eqGL.setPushConstants(&push, sizeof(push));
			drawUnitCube();
		}
		glDeleteFramebuffers(1, &fbo);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	glDeleteTextures(1, &equirectTex);
	return bakeFromEnvironmentCubemap(envCubemap, quality);
}

std::shared_ptr<GLIBLEnvironment> GLIBLEnvironment::createFromEquirectangular(
	const std::string& hdrImagePath, const IBLQualitySettings& quality) {
	const std::optional<std::vector<uint8_t>> bytes =
		platform::VFS::get().readFile(hdrImagePath);
	if (!bytes || bytes->empty()) {
		throw std::runtime_error(
			"GLIBLEnvironment::createFromEquirectangular: failed to read " +
			hdrImagePath);
	}

	stbi_set_flip_vertically_on_load(true);
	int width = 0;
	int height = 0;
	int channels = 0;
	float* pixels = stbi_loadf_from_memory(
		bytes->data(), static_cast<int>(bytes->size()), &width, &height,
		&channels, 4);
	stbi_set_flip_vertically_on_load(false);
	if (pixels == nullptr || width <= 0 || height <= 0) {
		throw std::runtime_error(
			"GLIBLEnvironment::createFromEquirectangular: failed to decode " +
			hdrImagePath);
	}

	std::shared_ptr<GLIBLEnvironment> result;
	try {
		result = createFromEquirectangularPixels(
			pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
			4, quality);
	} catch (...) {
		stbi_image_free(pixels);
		throw;
	}
	stbi_image_free(pixels);
	return result;
}

std::shared_ptr<GLIBLEnvironment> GLIBLEnvironment::createFromCubemapFaces(
	const std::array<std::string, 6>& facePaths,
	const IBLQualitySettings& rawQuality) {
	IBLQualitySettings quality = ibl::sanitizeQualitySettings(rawQuality);

	uint32_t envCubemap = 0;
	glGenTextures(1, &envCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	uint32_t faceSize = 0;
	stbi_set_flip_vertically_on_load(false);
	for (uint32_t face = 0; face < 6; face++) {
		const std::optional<std::vector<uint8_t>> bytes =
			platform::VFS::get().readFile(facePaths[face]);
		if (!bytes || bytes->empty()) {
			glDeleteTextures(1, &envCubemap);
			throw std::runtime_error(
				"GLIBLEnvironment::createFromCubemapFaces: failed to read " +
				facePaths[face]);
		}
		int width = 0;
		int height = 0;
		int channels = 0;
		float* pixels = stbi_loadf_from_memory(
			bytes->data(), static_cast<int>(bytes->size()), &width, &height,
			&channels, 4);
		if (pixels == nullptr || width <= 0 || height <= 0 || width != height) {
			if (pixels) stbi_image_free(pixels);
			glDeleteTextures(1, &envCubemap);
			throw std::runtime_error(
				"GLIBLEnvironment::createFromCubemapFaces: face image must be "
				"valid and square: " +
				facePaths[face]);
		}
		if (face == 0) {
			faceSize = static_cast<uint32_t>(width);
		} else if (static_cast<uint32_t>(width) != faceSize) {
			stbi_image_free(pixels);
			glDeleteTextures(1, &envCubemap);
			throw std::runtime_error(
				"GLIBLEnvironment::createFromCubemapFaces: all six faces must "
				"have identical dimensions.");
		}
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F,
					 width, height, 0, GL_RGBA, GL_FLOAT, pixels);
		stbi_image_free(pixels);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
					GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	quality.environmentSize = faceSize;
	return bakeFromEnvironmentCubemap(envCubemap, quality);
}

}  // namespace bird::gl
