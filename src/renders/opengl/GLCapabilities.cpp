#include "GLCapabilities.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "GLApi.h"

namespace bird::gl {

namespace {

GLApiFamily compiledFamily() {
#if defined(BIRD_GL_WEBGL)
	return GLApiFamily::WebGL;
#elif defined(BIRD_GL_ES)
	return GLApiFamily::ES;
#else
	return GLApiFamily::Desktop;
#endif
}

const char* familyName(GLApiFamily family) {
	switch (family) {
		case GLApiFamily::Desktop:
			return "OpenGL";
		case GLApiFamily::ES:
			return "OpenGL ES";
		case GLApiFamily::WebGL:
			return "WebGL";
	}
	return "unknown";
}

std::string glString(GLenum name) {
	const auto* text = reinterpret_cast<const char*>(glGetString(name));
	return text ? std::string(text) : std::string();
}

GLContextInfo queryContext() {
	if (glString(GL_VERSION).empty()) {
		throw std::runtime_error(
			"GLCapabilities::current: no current OpenGL context");
	}
	GLContextInfo info;
	info.family = compiledFamily();
	glGetIntegerv(GL_MAJOR_VERSION, &info.major);
	glGetIntegerv(GL_MINOR_VERSION, &info.minor);
	GLint count = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &count);
	for (GLint i = 0; i < count; i++) {
		const auto* name = reinterpret_cast<const char*>(
			glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
		if (name) info.extensions.insert(name);
	}
	return info;
}

}  // namespace

bool GLCapabilities::atLeast(int32_t wantMajor, int32_t wantMinor) const {
	return major > wantMajor || (major == wantMajor && minor >= wantMinor);
}

GLCapabilities GLCapabilities::derive(const GLContextInfo& info) {
	GLCapabilities caps;
	caps.family = info.family;
	caps.major = info.major;
	caps.minor = info.minor;
	auto has = [&info](const char* extension) {
		return info.extensions.count(extension) > 0;
	};

	if (info.family == GLApiFamily::Desktop) {
		caps.depthClamp = caps.atLeast(3, 2) || has("GL_ARB_depth_clamp");
		caps.srgbWriteControl = true;  // core since 3.0
		caps.debugOutput = caps.atLeast(4, 3) || has("GL_KHR_debug");
		caps.geometryShaders = caps.atLeast(3, 2);
		caps.tessellationShaders =
			caps.atLeast(4, 0) || has("GL_ARB_tessellation_shader");
		caps.halfFloatRenderTargets = true;	 // core since 3.0
		caps.linearDepthFiltering = true;
		return caps;
	}

	// OpenGL ES and WebGL share the ES 3.0 baseline.
	const bool es32 = caps.atLeast(3, 2);
	caps.depthClamp = has("GL_EXT_depth_clamp");
	caps.srgbWriteControl = has("GL_EXT_sRGB_write_control");
	caps.debugOutput = es32 || has("GL_KHR_debug");
	caps.geometryShaders = es32 || has("GL_EXT_geometry_shader");
	caps.tessellationShaders = es32 || has("GL_EXT_tessellation_shader");
	caps.halfFloatRenderTargets = es32 ||
								  has("GL_EXT_color_buffer_half_float") ||
								  has("GL_EXT_color_buffer_float");
	caps.linearDepthFiltering = false;
	if (info.family == GLApiFamily::WebGL) {
		// WebGL 2 offers neither debug output nor extra shader stages.
		caps.debugOutput = false;
		caps.geometryShaders = false;
		caps.tessellationShaders = false;
	}
	return caps;
}

void GLCapabilities::disable(const std::string& commaSeparatedFlags) {
	std::stringstream stream(commaSeparatedFlags);
	std::string flag;
	while (std::getline(stream, flag, ',')) {
		if (flag.empty()) continue;
		if (flag == "depthClamp") {
			depthClamp = false;
		} else if (flag == "srgbWriteControl") {
			srgbWriteControl = false;
		} else if (flag == "debugOutput") {
			debugOutput = false;
		} else if (flag == "geometryShaders") {
			geometryShaders = false;
		} else if (flag == "tessellationShaders") {
			tessellationShaders = false;
		} else if (flag == "halfFloatRenderTargets") {
			halfFloatRenderTargets = false;
		} else if (flag == "linearDepthFiltering") {
			linearDepthFiltering = false;
		} else {
			throw std::invalid_argument("Unknown GL capability '" + flag +
										"' in BIRD_GL_CAPS_DISABLE");
		}
	}
}

const GLCapabilities& GLCapabilities::current() {
	static const GLCapabilities caps = [] {
		GLCapabilities detected = derive(queryContext());
		if (const char* disabled = std::getenv("BIRD_GL_CAPS_DISABLE")) {
			detected.disable(disabled);
		}
		std::cout << "GL context: " << familyName(detected.family) << " "
				  << detected.major << "." << detected.minor << " ("
				  << glString(GL_RENDERER) << ")" << std::endl;
		return detected;
	}();
	return caps;
}

format::ShaderTarget chooseShaderTarget(const GLCapabilities& caps) {
	if (caps.isEmbedded()) return format::SHADER_TARGET_GLSL_ES_300;
	return caps.atLeast(4, 6) ? format::SHADER_TARGET_GLSL_460
							  : format::SHADER_TARGET_GLSL_410;
}

}  // namespace bird::gl
