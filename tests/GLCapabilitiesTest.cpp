#include <gtest/gtest.h>

#include <stdexcept>

#include "renders/opengl/GLCapabilities.h"

using bird::gl::chooseShaderTarget;
using bird::gl::GLApiFamily;
using bird::gl::GLCapabilities;
using bird::gl::GLContextInfo;
namespace format = bird::format;

namespace {

GLContextInfo context(GLApiFamily family, int major, int minor,
					  std::unordered_set<std::string> extensions = {}) {
	GLContextInfo info;
	info.family = family;
	info.major = major;
	info.minor = minor;
	info.extensions = std::move(extensions);
	return info;
}

}  // namespace

TEST(GLCapabilities, Desktop41HasCoreFeaturesButNoDebugOutput) {
	const auto caps =
		GLCapabilities::derive(context(GLApiFamily::Desktop, 4, 1));
	EXPECT_FALSE(caps.isEmbedded());
	EXPECT_TRUE(caps.depthClamp);
	EXPECT_TRUE(caps.srgbWriteControl);
	EXPECT_TRUE(caps.halfFloatRenderTargets);
	EXPECT_TRUE(caps.linearDepthFiltering);
	EXPECT_TRUE(caps.geometryShaders);
	EXPECT_TRUE(caps.tessellationShaders);
	EXPECT_FALSE(caps.debugOutput);
	EXPECT_TRUE(GLCapabilities::derive(context(GLApiFamily::Desktop, 4, 6))
					.debugOutput);
}

TEST(GLCapabilities, Es30BaselineNeedsExtensionsForOptionalFeatures) {
	const auto caps = GLCapabilities::derive(context(GLApiFamily::ES, 3, 0));
	EXPECT_TRUE(caps.isEmbedded());
	EXPECT_FALSE(caps.depthClamp);
	EXPECT_FALSE(caps.srgbWriteControl);
	EXPECT_FALSE(caps.halfFloatRenderTargets);
	EXPECT_FALSE(caps.linearDepthFiltering);
	EXPECT_FALSE(caps.geometryShaders);
	EXPECT_FALSE(caps.debugOutput);

	const auto extended = GLCapabilities::derive(
		context(GLApiFamily::ES, 3, 0,
				{"GL_EXT_depth_clamp", "GL_EXT_color_buffer_half_float",
				 "GL_EXT_sRGB_write_control", "GL_KHR_debug"}));
	EXPECT_TRUE(extended.depthClamp);
	EXPECT_TRUE(extended.halfFloatRenderTargets);
	EXPECT_TRUE(extended.srgbWriteControl);
	EXPECT_TRUE(extended.debugOutput);
}

TEST(GLCapabilities, Es32MakesHalfFloatTargetsAndStagesCore) {
	const auto caps = GLCapabilities::derive(context(GLApiFamily::ES, 3, 2));
	EXPECT_TRUE(caps.halfFloatRenderTargets);
	EXPECT_TRUE(caps.geometryShaders);
	EXPECT_TRUE(caps.tessellationShaders);
	EXPECT_TRUE(caps.debugOutput);
	EXPECT_FALSE(caps.depthClamp);	// still an extension in ES 3.2
}

TEST(GLCapabilities, WebGlNeverReportsDebugOutputOrExtraStages) {
	const auto caps = GLCapabilities::derive(context(
		GLApiFamily::WebGL, 3, 0,
		{"GL_KHR_debug", "GL_EXT_geometry_shader", "GL_EXT_color_buffer_float"}));
	EXPECT_FALSE(caps.debugOutput);
	EXPECT_FALSE(caps.geometryShaders);
	EXPECT_TRUE(caps.halfFloatRenderTargets);
}

TEST(GLCapabilities, DisableClearsNamedFlagsAndRejectsUnknownOnes) {
	auto caps = GLCapabilities::derive(context(GLApiFamily::Desktop, 4, 6));
	caps.disable("depthClamp,,halfFloatRenderTargets");
	EXPECT_FALSE(caps.depthClamp);
	EXPECT_FALSE(caps.halfFloatRenderTargets);
	EXPECT_TRUE(caps.srgbWriteControl);
	EXPECT_THROW(caps.disable("depthClamp,noSuchFlag"), std::invalid_argument);
}

TEST(GLCapabilities, ShaderTargetFollowsApiFamilyAndVersion) {
	EXPECT_EQ(chooseShaderTarget(
				  GLCapabilities::derive(context(GLApiFamily::Desktop, 4, 6))),
			  format::SHADER_TARGET_GLSL_460);
	EXPECT_EQ(chooseShaderTarget(
				  GLCapabilities::derive(context(GLApiFamily::Desktop, 4, 1))),
			  format::SHADER_TARGET_GLSL_410);
	EXPECT_EQ(chooseShaderTarget(
				  GLCapabilities::derive(context(GLApiFamily::ES, 3, 2))),
			  format::SHADER_TARGET_GLSL_ES_300);
	EXPECT_EQ(chooseShaderTarget(
				  GLCapabilities::derive(context(GLApiFamily::WebGL, 3, 0))),
			  format::SHADER_TARGET_GLSL_ES_300);
}
