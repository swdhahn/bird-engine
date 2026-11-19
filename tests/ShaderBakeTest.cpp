#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "ShaderBaker.h"

using bird::ShaderBinary;
using bird::bake::bakeShader;
using bird::bake::ShaderBakeError;
namespace format = bird::format;

namespace {

const char* kVertex = R"(#version 450
layout(location = 0) in vec3 position;
layout(location = 0) out vec2 uv;
layout(std140, binding = 0) uniform Global { mat4 viewProjection; };
layout(push_constant) uniform Object {
    mat4 model;
    mat3 normalMatrix;
    vec2 tint;
    uint id;
} object;
void main() {
    uv = position.xy * float(object.id) + object.tint.xy;
    gl_Position = viewProjection * object.model *
                  vec4(object.normalMatrix * position, 1.0);
})";

const char* kTwoSamplers = R"(#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D screenTexture;
layout(binding = 1) uniform sampler2D depthTexture;
void main() { color = texture(screenTexture, uv) * texture(depthTexture, uv).r; })";

std::string text(const ShaderBinary& binary, format::ShaderTarget target) {
	return binary.variantText(target);
}

void expectRejected(const std::string& path, const std::string& source,
					const std::string& fragment) {
	try {
		bakeShader(path, source);
		FAIL() << "expected ShaderBakeError containing '" << fragment << "'";
	} catch (const ShaderBakeError& error) {
		EXPECT_NE(std::string(error.what()).find(fragment), std::string::npos)
			<< error.what();
	}
}

}  // namespace

TEST(ShaderBake, ProducesAllThreeVariants) {
	const ShaderBinary binary = bakeShader("test.vert", kVertex);
	EXPECT_EQ(binary.stage, format::SHADER_STAGE_VERTEX);
	ASSERT_EQ(binary.variants.size(), 3u);
	const auto& spirv = binary.variant(format::SHADER_TARGET_SPIRV);
	ASSERT_GE(spirv.size(), 4u);
	EXPECT_EQ(*reinterpret_cast<const uint32_t*>(spirv.data()), 0x07230203u);
	EXPECT_EQ(text(binary, format::SHADER_TARGET_GLSL_460).rfind("#version 460", 0), 0u);
	EXPECT_EQ(text(binary, format::SHADER_TARGET_GLSL_410).rfind("#version 410", 0), 0u);
}

TEST(ShaderBake, Glsl410StripsBindingsWhile460KeepsThem) {
	const ShaderBinary binary = bakeShader("test.frag", kTwoSamplers);
	EXPECT_EQ(text(binary, format::SHADER_TARGET_GLSL_410).find("binding"),
			  std::string::npos);
	EXPECT_NE(text(binary, format::SHADER_TARGET_GLSL_460).find("binding = 1"),
			  std::string::npos);
}

TEST(ShaderBake, ReflectsSamplerBindings) {
	const ShaderBinary binary = bakeShader("test.frag", kTwoSamplers);
	ASSERT_EQ(binary.resources.size(), 2u);
	for (const auto& resource : binary.resources) {
		EXPECT_EQ(resource.kind, format::SHADER_RESOURCE_SAMPLER);
		EXPECT_EQ(resource.binding, resource.name == "depthTexture" ? 1u : 0u);
	}
}

TEST(ShaderBake, ReflectsUniformBlockByDeclaredName) {
	const ShaderBinary binary = bakeShader("test.vert", kVertex);
	ASSERT_EQ(binary.resources.size(), 1u);
	EXPECT_EQ(binary.resources[0].kind, format::SHADER_RESOURCE_UNIFORM_BUFFER);
	EXPECT_EQ(binary.resources[0].name, "Global");
	EXPECT_NE(text(binary, format::SHADER_TARGET_GLSL_410)
				  .find("uniform " + binary.resources[0].name),
			  std::string::npos);
}

TEST(ShaderBake, ReflectsPushConstantLayout) {
	const ShaderBinary binary = bakeShader("test.vert", kVertex);
	// std430: mat4 @0, mat3 @64 (3 x 16-byte columns), vec2 @112, uint @120.
	EXPECT_EQ(binary.pushConstantSize, 124u);
	ASSERT_EQ(binary.pushConstants.size(), 4u);
	const auto& members = binary.pushConstants;
	EXPECT_EQ(members[0].name, "object.model");
	EXPECT_EQ(members[0].type, format::PUSH_CONSTANT_MAT4);
	EXPECT_EQ(members[1].offset, 64u);
	EXPECT_EQ(members[1].type, format::PUSH_CONSTANT_MAT3);
	EXPECT_EQ(members[2].offset, 112u);
	EXPECT_EQ(members[2].type, format::PUSH_CONSTANT_VEC2);
	EXPECT_EQ(members[3].offset, 120u);
	EXPECT_EQ(members[3].type, format::PUSH_CONSTANT_UINT);
	// On OpenGL the block becomes ordinary uniforms with those names.
	for (auto target : {format::SHADER_TARGET_GLSL_410,
						format::SHADER_TARGET_GLSL_460}) {
		EXPECT_NE(text(binary, target).find("uniform Object object;"),
				  std::string::npos);
	}
}

TEST(ShaderBake, AnonymousPushConstantGetsStableName) {
	const ShaderBinary binary = bakeShader("anon.vert", R"(#version 450
layout(location = 0) in vec3 position;
layout(push_constant) uniform PC { mat4 model; };
void main() { gl_Position = model * vec4(position, 1.0); })");
	ASSERT_EQ(binary.pushConstants.size(), 1u);
	EXPECT_EQ(binary.pushConstants[0].name, "pushConstants.model");
}

TEST(ShaderBake, RejectsOversizedPushConstants) {
	expectRejected("big.vert", R"(#version 450
layout(location = 0) in vec3 position;
layout(push_constant) uniform PC { mat4 a; mat4 b; vec4 c; } pc;
void main() { gl_Position = pc.a * pc.b * vec4(position, 1.0) + pc.c; })",
				   "limit is 128");
}

TEST(ShaderBake, RejectsPushConstantArrays) {
	expectRejected("array.vert", R"(#version 450
layout(location = 0) in vec3 position;
layout(push_constant) uniform PC { vec4 values[2]; } pc;
void main() { gl_Position = pc.values[0] + vec4(position, 1.0); })",
				   "no arrays");
}

TEST(ShaderBake, RejectsLooseUniforms) {
	expectRejected("loose.frag", R"(#version 450
layout(location = 0) out vec4 color;
uniform float brightness;
void main() { color = vec4(brightness); })",
				   "GLSL -> SPIR-V failed");
}

TEST(ShaderBake, RejectsMissingLocation) {
	expectRejected("noloc.frag", R"(#version 450
in vec2 uv;
layout(location = 0) out vec4 color;
void main() { color = vec4(uv, 0.0, 1.0); })",
				   "location");
}

TEST(ShaderBake, RejectsUnsupportedExtension) {
	EXPECT_THROW(bakeShader("shader.txt", kVertex), ShaderBakeError);
}

TEST(ShaderBinaryFormat, RoundTripsThroughSerialization) {
	const ShaderBinary original = bakeShader("test.vert", kVertex);
	const ShaderBinary parsed =
		bird::parseShaderBinary(bird::serializeShaderBinary(original));
	EXPECT_EQ(parsed.stage, original.stage);
	EXPECT_EQ(parsed.sourceHash, original.sourceHash);
	EXPECT_EQ(parsed.variants, original.variants);
	EXPECT_EQ(parsed.pushConstantSize, original.pushConstantSize);
	ASSERT_EQ(parsed.pushConstants.size(), original.pushConstants.size());
	EXPECT_EQ(parsed.pushConstants[1].name, original.pushConstants[1].name);
	ASSERT_EQ(parsed.resources.size(), 1u);
	EXPECT_EQ(parsed.resources[0].name, "Global");
}

TEST(ShaderBinaryFormat, TruncatedDataThrows) {
	auto bytes = bird::serializeShaderBinary(bakeShader("test.frag", kTwoSamplers));
	bytes.resize(bytes.size() - 3);
	EXPECT_THROW(bird::parseShaderBinary(bytes), bird::ShaderBinaryError);
}

TEST(ShaderBinaryFormat, StaleDetectionFollowsSourceHash) {
	const auto dir = std::filesystem::temp_directory_path() / "bird_bake_test";
	std::filesystem::create_directories(dir);
	const std::string path = (dir / "stale.frag").string();
	bird::writeShaderBinaryFile(path + ".bird", bakeShader(path, kTwoSamplers));

	EXPECT_FALSE(bird::bake::needsShaderBake(path, kTwoSamplers));
	EXPECT_TRUE(bird::bake::needsShaderBake(path, std::string(kTwoSamplers) + "\n"));
	std::filesystem::remove_all(dir);
	EXPECT_TRUE(bird::bake::needsShaderBake(path, kTwoSamplers));
}
