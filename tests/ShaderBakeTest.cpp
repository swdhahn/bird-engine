#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
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

TEST(ShaderBake, ProducesEveryVariant) {
	const ShaderBinary binary = bakeShader("test.vert", kVertex);
	EXPECT_EQ(binary.stage, format::SHADER_STAGE_VERTEX);
	ASSERT_EQ(binary.variants.size(), 4u);
	const auto& spirv = binary.variant(format::SHADER_TARGET_SPIRV);
	ASSERT_GE(spirv.size(), 4u);
	EXPECT_EQ(*reinterpret_cast<const uint32_t*>(spirv.data()), 0x07230203u);
	EXPECT_EQ(text(binary, format::SHADER_TARGET_GLSL_460).rfind("#version 460", 0), 0u);
	EXPECT_EQ(text(binary, format::SHADER_TARGET_GLSL_410).rfind("#version 410", 0), 0u);
	EXPECT_EQ(text(binary, format::SHADER_TARGET_GLSL_ES_300).rfind("#version 300 es", 0), 0u);
}

TEST(ShaderBake, GlslEs300StripsBindingsAndUsesHighPrecision) {
	const std::string es =
		text(bakeShader("test.frag", kTwoSamplers),
			 format::SHADER_TARGET_GLSL_ES_300);
	EXPECT_EQ(es.find("binding"), std::string::npos) << es;
	EXPECT_NE(es.find("precision highp float;"), std::string::npos) << es;
}

TEST(ShaderBake, GlslEs300PairsVaryingsByLocationName) {
	// ES 3.0 links vertex outputs to fragment inputs by name, so both
	// stages must name a varying after its location, whatever the source.
	const std::string vertex =
		text(bakeShader("test.vert", kVertex), format::SHADER_TARGET_GLSL_ES_300);
	const std::string fragment = text(bakeShader("test.frag", kTwoSamplers),
									  format::SHADER_TARGET_GLSL_ES_300);
	EXPECT_NE(vertex.find("out vec2 bird_varying_0;"), std::string::npos)
		<< vertex;
	EXPECT_NE(fragment.find("in vec2 bird_varying_0;"), std::string::npos)
		<< fragment;
	// Vertex inputs and fragment outputs keep their names and locations.
	EXPECT_NE(vertex.find("layout(location = 0) in"), std::string::npos);
	EXPECT_NE(fragment.find("layout(location = 0) out"), std::string::npos);
}

TEST(ShaderBake, DesktopVariantsKeepSourceVaryingNames) {
	const std::string desktop =
		text(bakeShader("test.vert", kVertex), format::SHADER_TARGET_GLSL_410);
	EXPECT_EQ(desktop.find("bird_varying_"), std::string::npos);
	EXPECT_NE(desktop.find(" uv;"), std::string::npos) << desktop;
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
						format::SHADER_TARGET_GLSL_460,
						format::SHADER_TARGET_GLSL_ES_300}) {
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
	EXPECT_EQ(parsed.dependencies.size(), original.dependencies.size());
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

namespace {

/** Temporary shader tree: main.frag -> lib/tint.glsl -> lib/palette.glsl. */
class IncludeTree : public ::testing::Test {
   protected:
	void SetUp() override {
		m_root = std::filesystem::temp_directory_path() / "bird_include_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root / "lib");
		write("lib/palette.glsl", "const vec3 PALETTE_RED = vec3(1, 0, 0);\n");
		write("lib/tint.glsl",
			  "#include \"palette.glsl\"\n"
			  "vec4 tint(vec4 c) { return c * vec4(PALETTE_RED, 1.0); }\n");
		write("main.frag",
			  "#version 450\n"
			  "#include \"lib/tint.glsl\"\n"
			  "layout(location = 0) out vec4 color;\n"
			  "void main() { color = tint(vec4(1.0)); }\n");
	}
	void TearDown() override { std::filesystem::remove_all(m_root); }

	void write(const std::string& relative, const std::string& text) {
		std::ofstream(m_root / relative, std::ios::trunc) << text;
	}
	std::string path(const std::string& relative) const {
		return (m_root / relative).lexically_normal().generic_string();
	}
	std::string mainSource() const {
		std::ifstream file(m_root / "main.frag");
		return std::string(std::istreambuf_iterator<char>(file), {});
	}
	void bakeMain() {
		bird::writeShaderBinaryFile(path("main.frag") + ".bird",
									bakeShader(path("main.frag"), mainSource()));
	}

	std::filesystem::path m_root;
};

}  // namespace

TEST_F(IncludeTree, RecordsTransitiveIncludes) {
	const ShaderBinary binary = bakeShader(path("main.frag"), mainSource());
	ASSERT_EQ(binary.dependencies.size(), 2u);
	EXPECT_EQ(binary.dependencies[0].path, path("lib/tint.glsl"));
	EXPECT_EQ(binary.dependencies[1].path, path("lib/palette.glsl"));
	// The included function really reached the generated GLSL.
	EXPECT_NE(text(binary, format::SHADER_TARGET_GLSL_410).find("tint("),
			  std::string::npos);
}

TEST_F(IncludeTree, UpToDateWhenNothingChanged) {
	bakeMain();
	EXPECT_FALSE(bird::bake::needsShaderBake(path("main.frag"), mainSource()));
}

TEST_F(IncludeTree, NestedIncludeEditTriggersRebake) {
	bakeMain();
	write("lib/palette.glsl", "const vec3 PALETTE_RED = vec3(0.5, 0, 0);\n");
	EXPECT_TRUE(bird::bake::needsShaderBake(path("main.frag"), mainSource()));
}

TEST_F(IncludeTree, DeletedIncludeTriggersRebake) {
	bakeMain();
	std::filesystem::remove(m_root / "lib/palette.glsl");
	EXPECT_TRUE(bird::bake::needsShaderBake(path("main.frag"), mainSource()));
}

TEST_F(IncludeTree, MissingIncludeFailsBake) {
	std::filesystem::remove(m_root / "lib/palette.glsl");
	expectRejected(path("main.frag"), mainSource(), "cannot open include");
}

TEST_F(IncludeTree, AngleBracketIncludeIsRejected) {
	expectRejected(path("main.frag"),
				   "#version 450\n#include <lib/tint.glsl>\n"
				   "layout(location = 0) out vec4 color;\n"
				   "void main() { color = vec4(1.0); }\n",
				   "not supported");
}

// ------------------------------------------------- engine shader regression

namespace {

std::string readFile(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), {});
}

uint32_t bindingOf(const ShaderBinary& binary, const std::string& name) {
	for (const auto& resource : binary.resources) {
		if (resource.name == name) return resource.binding;
	}
	ADD_FAILURE() << "resource " << name << " not reflected";
	return ~0u;
}

}  // namespace

class EngineShader : public ::testing::TestWithParam<const char*> {};

TEST_P(EngineShader, BakesForEveryTarget) {
	const std::filesystem::path path =
		std::filesystem::path(BIRD_SOURCE_DIR) / "assets/shaders" / GetParam();
	ASSERT_TRUE(std::filesystem::exists(path)) << path;
	const ShaderBinary binary = bakeShader(path.string(), readFile(path));
	EXPECT_FALSE(text(binary, format::SHADER_TARGET_GLSL_410).empty());
	EXPECT_FALSE(text(binary, format::SHADER_TARGET_GLSL_460).empty());
	EXPECT_FALSE(text(binary, format::SHADER_TARGET_GLSL_ES_300).empty());
	EXPECT_LE(binary.pushConstantSize, 128u);
}

INSTANTIATE_TEST_SUITE_P(
	RenderingPipeline, EngineShader,
	::testing::Values("mesh.vert", "gbuffer.frag", "forward.frag",
					  "light_directional.frag", "light_volume.vert",
					  "light_volume.frag", "shadow_depth.vert",
					  "shadow_depth.frag", "tonemap.frag", "passthrough.vert",
					  "fxaa.frag", "ui.vert", "ui.frag", "debug_line.vert",
					  "mesh_skinned.vert", "shadow_depth_skinned.vert"));

TEST(EngineShaderBindings, MatchGpuLayoutConventions) {
	const auto root = std::filesystem::path(BIRD_SOURCE_DIR) / "assets/shaders";
	const ShaderBinary forward =
		bakeShader((root / "forward.frag").string(),
				   readFile(root / "forward.frag"));
	// Values mirror renders/generic/GpuLayouts.h.
	EXPECT_EQ(bindingOf(forward, "Global"), 0u);
	EXPECT_EQ(bindingOf(forward, "Material"), 1u);
	EXPECT_EQ(bindingOf(forward, "Lights"), 2u);
	EXPECT_EQ(bindingOf(forward, "Shadows"), 3u);
	EXPECT_EQ(bindingOf(forward, "normalMap"), 2u);
	EXPECT_EQ(bindingOf(forward, "localShadowAtlas"), 9u);

	const ShaderBinary volume =
		bakeShader((root / "light_volume.frag").string(),
				   readFile(root / "light_volume.frag"));
	EXPECT_EQ(volume.pushConstantSize, 128u);
	EXPECT_EQ(bindingOf(volume, "gDepth"), 7u);
}

TEST(EngineShaderBindings, SkinnedVariantsMatchTheirRigidCounterparts) {
	const auto root = std::filesystem::path(BIRD_SOURCE_DIR) / "assets/shaders";
	auto bake = [&root](const char* name) {
		return bakeShader((root / name).string(), readFile(root / name));
	};
	const ShaderBinary rigid = bake("mesh.vert");
	const ShaderBinary skinned = bake("mesh_skinned.vert");
	// Values mirror UBO_BINDING_SKINNING in renders/generic/GpuLayouts.h.
	EXPECT_EQ(bindingOf(skinned, "Skinning"), 4u);
	EXPECT_EQ(bindingOf(skinned, "Global"), 0u);
	// The passes fill both programs with the same push constants.
	EXPECT_EQ(skinned.pushConstantSize, rigid.pushConstantSize);
	EXPECT_EQ(bake("shadow_depth_skinned.vert").pushConstantSize,
			  bake("shadow_depth.vert").pushConstantSize);
	EXPECT_EQ(bindingOf(bake("shadow_depth_skinned.vert"), "Skinning"), 4u);
}
