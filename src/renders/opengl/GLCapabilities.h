#ifndef BIRD_GLCAPABILITIES_H
#define BIRD_GLCAPABILITIES_H

#include <cstdint>
#include <string>
#include <unordered_set>

#include "../../build-tools/FormatHeaders.h"

namespace bird::gl {

/** Which OpenGL API family the context implements. */
enum class GLApiFamily : uint8_t {
	Desktop,  // OpenGL core profile
	ES,		  // OpenGL ES (Android, iOS, Linux testing)
	WebGL	  // WebGL 2: the ES 3.0 API with browser restrictions
};

/** Raw facts read from a context; the input to GLCapabilities::derive. */
struct GLContextInfo {
	GLApiFamily family = GLApiFamily::Desktop;
	int32_t major = 0;
	int32_t minor = 0;
	std::unordered_set<std::string> extensions;	 // e.g. "GL_EXT_depth_clamp"
};

/**
 * Optional features of the current context. Passes consult these flags
 * instead of #ifdef blocks, so one code path serves desktop OpenGL,
 * OpenGL ES and WebGL, and every fallback can be forced on a desktop for
 * testing (see BIRD_GL_CAPS_DISABLE).
 */
struct GLCapabilities {
	GLApiFamily family = GLApiFamily::Desktop;
	int32_t major = 0;
	int32_t minor = 0;

	/** GL_DEPTH_CLAMP: geometry beyond near/far still rasterizes. */
	bool depthClamp = false;
	/** GL_FRAMEBUFFER_SRGB toggles sRGB encoding; without it ES encodes always. */
	bool srgbWriteControl = false;
	/** GL_DEBUG_OUTPUT exists (GL 4.3, ES 3.2 or KHR_debug). */
	bool debugOutput = false;
	bool geometryShaders = false;
	bool tessellationShaders = false;
	/** RGBA16F and RG16F are color-renderable. */
	bool halfFloatRenderTargets = false;
	/** Depth textures accept LINEAR filtering without compare mode. */
	bool linearDepthFiltering = false;

	/** @return true for OpenGL ES and WebGL contexts. */
	bool isEmbedded() const { return family != GLApiFamily::Desktop; }
	/** @return true when the context version is at least major.minor. */
	bool atLeast(int32_t wantMajor, int32_t wantMinor) const;

	/**
	 * Derives the feature flags from a context description. Pure; O(1)
	 * expected per lookup.
	 */
	static GLCapabilities derive(const GLContextInfo& info);

	/**
	 * Clears every flag named in a comma-separated list (the field names
	 * above, e.g. "depthClamp,halfFloatRenderTargets") to force fallbacks.
	 *
	 * @throws std::invalid_argument naming an unknown flag.
	 */
	void disable(const std::string& commaSeparatedFlags);

	/**
	 * Capabilities of the current context, detected on first call and
	 * cached for the process (one GL context per process). Applies the
	 * BIRD_GL_CAPS_DISABLE environment variable.
	 *
	 * @throws std::runtime_error when no context is current.
	 */
	static const GLCapabilities& current();
};

/**
 * Shader variant for a context: GLSL ES 300 on ES and WebGL, GLSL 460 on
 * desktop 4.6+, GLSL 410 otherwise.
 */
format::ShaderTarget chooseShaderTarget(const GLCapabilities& caps);

}  // namespace bird::gl

#endif	// BIRD_GLCAPABILITIES_H
