#include "ShaderBaker.h"

#include <filesystem>
#include <shaderc/shaderc.hpp>
#include <spirv-tools/libspirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <unordered_map>
#include <vector>

namespace bird::bake {

namespace {

using Words = std::vector<uint32_t>;

struct StageInfo {
	format::ShaderStageKind stage;
	shaderc_shader_kind kind;
};

const std::unordered_map<std::string, StageInfo>& stageTable() {
	static const std::unordered_map<std::string, StageInfo> table = {
		{".vert", {format::SHADER_STAGE_VERTEX, shaderc_vertex_shader}},
		{".frag", {format::SHADER_STAGE_FRAGMENT, shaderc_fragment_shader}},
		{".geom", {format::SHADER_STAGE_GEOMETRY, shaderc_geometry_shader}},
		{".comp", {format::SHADER_STAGE_COMPUTE, shaderc_compute_shader}},
		{".tesc",
		 {format::SHADER_STAGE_TESS_CONTROL, shaderc_tess_control_shader}},
		{".tese",
		 {format::SHADER_STAGE_TESS_EVALUATION,
		  shaderc_tess_evaluation_shader}}};
	return table;
}

const StageInfo& stageInfoFor(const std::string& path) {
	const std::string ext = std::filesystem::path(path).extension().string();
	auto it = stageTable().find(ext);
	if (it == stageTable().end()) {
		throw ShaderBakeError(path + ": unsupported shader extension '" + ext +
							  "'");
	}
	return it->second;
}

[[noreturn]] void fail(const std::string& path, const std::string& message) {
	throw ShaderBakeError(path + ": " + message);
}

Words compileToSpirv(const std::string& path, const std::string& source,
					 shaderc_shader_kind kind) {
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_vulkan,
								 shaderc_env_version_vulkan_1_0);
	// No optimization: names must survive for reflection, and drivers
	// optimize anyway.
	options.SetOptimizationLevel(shaderc_optimization_level_zero);
	const auto result =
		compiler.CompileGlslToSpv(source, kind, path.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		fail(path, "GLSL -> SPIR-V failed:\n" + result.GetErrorMessage());
	}
	return Words(result.cbegin(), result.cend());
}

void validateSpirv(const std::string& path, const Words& words) {
	spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_0);
	std::string messages;
	tools.SetMessageConsumer([&](spv_message_level_t, const char*,
								 const spv_position_t&, const char* message) {
		messages += message;
		messages += '\n';
	});
	if (!tools.Validate(words)) {
		fail(path, "SPIR-V validation failed:\n" + messages);
	}
}

/** Syntax-checks generated GLSL by compiling it for OpenGL (discarded). */
void validateGlsl(const std::string& path, const std::string& glsl,
				  shaderc_shader_kind kind, const char* label) {
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_opengl,
								 shaderc_env_version_opengl_4_5);
	options.SetAutoMapLocations(true);
	options.SetAutoBindUniforms(true);
	const auto result =
		compiler.CompileGlslToSpv(glsl, kind, path.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		fail(path, std::string("generated ") + label + " is invalid:\n" +
					   result.GetErrorMessage() + "\n--- generated ---\n" +
					   glsl);
	}
}

uint32_t requireBinding(const std::string& path,
						const spirv_cross::Compiler& compiler,
						const spirv_cross::Resource& resource) {
	if (!compiler.has_decoration(resource.id, spv::DecorationBinding)) {
		fail(path, "'" + resource.name + "' needs an explicit binding");
	}
	if (compiler.get_decoration(resource.id, spv::DecorationDescriptorSet) !=
		0) {
		fail(path, "'" + resource.name + "' must use descriptor set 0");
	}
	return compiler.get_decoration(resource.id, spv::DecorationBinding);
}

void requireLocations(const std::string& path,
					  const spirv_cross::Compiler& compiler,
					  const spirv_cross::SmallVector<spirv_cross::Resource>&
						  variables) {
	for (const auto& variable : variables) {
		if (!compiler.has_decoration(variable.id, spv::DecorationLocation)) {
			fail(path, "'" + variable.name + "' needs an explicit location");
		}
	}
}

format::PushConstantType pushConstantTypeOf(const std::string& path,
											const std::string& name,
											const spirv_cross::SPIRType& type,
											uint32_t matrixStride) {
	using Base = spirv_cross::SPIRType::BaseType;
	if (!type.array.empty() || type.basetype == Base::Struct) {
		fail(path, "push constant '" + name +
					   "' must be a scalar, vector, mat3 or mat4 (no arrays "
					   "or structs)");
	}
	if (type.columns > 1) {
		if (type.basetype != Base::Float || type.vecsize != type.columns ||
			(type.columns != 3 && type.columns != 4) || matrixStride != 16) {
			fail(path, "push constant '" + name +
						   "' must be mat3 or mat4 with column-major layout");
		}
		return type.columns == 3 ? format::PUSH_CONSTANT_MAT3
								 : format::PUSH_CONSTANT_MAT4;
	}
	int base = 0;
	switch (type.basetype) {
		case Base::Float:
			base = format::PUSH_CONSTANT_FLOAT;
			break;
		case Base::Int:
			base = format::PUSH_CONSTANT_INT;
			break;
		case Base::UInt:
			base = format::PUSH_CONSTANT_UINT;
			break;
		default:
			fail(path, "push constant '" + name +
						   "' must be float, int or uint based");
	}
	return static_cast<format::PushConstantType>(base + type.vecsize - 1);
}

/** Interface facts gathered once from the SPIR-V module. */
struct Reflection {
	struct Bound {
		spirv_cross::ID id;
		spirv_cross::TypeID typeId;
		format::ShaderResourceKind kind;
		uint32_t binding;
	};
	std::vector<Bound> bound;
	bool hasPushConstants = false;
	spirv_cross::ID pushConstantId = 0;
	spirv_cross::TypeID pushConstantTypeId = 0;
	uint32_t pushConstantSize = 0;
	std::vector<std::pair<uint32_t, format::PushConstantType>> members;
};

Reflection reflect(const std::string& path, const Words& words,
				   format::ShaderStageKind stage) {
	spirv_cross::Compiler compiler(words);
	const auto resources = compiler.get_shader_resources();
	Reflection reflection;

	if (!resources.storage_buffers.empty() ||
		!resources.storage_images.empty() ||
		!resources.separate_images.empty() ||
		!resources.separate_samplers.empty() ||
		!resources.subpass_inputs.empty()) {
		fail(path,
			 "only uniform blocks, combined samplers and push constants are "
			 "supported");
	}
	for (const auto& ubo : resources.uniform_buffers) {
		reflection.bound.push_back({ubo.id, ubo.base_type_id,
									format::SHADER_RESOURCE_UNIFORM_BUFFER,
									requireBinding(path, compiler, ubo)});
	}
	for (const auto& sampler : resources.sampled_images) {
		reflection.bound.push_back({sampler.id, sampler.base_type_id,
									format::SHADER_RESOURCE_SAMPLER,
									requireBinding(path, compiler, sampler)});
	}
	if (stage != format::SHADER_STAGE_VERTEX) {
		requireLocations(path, compiler, resources.stage_inputs);
	}
	if (stage != format::SHADER_STAGE_FRAGMENT) {
		requireLocations(path, compiler, resources.stage_outputs);
	}

	if (resources.push_constant_buffers.size() > 1) {
		fail(path, "at most one push_constant block is allowed per stage");
	}
	if (!resources.push_constant_buffers.empty()) {
		const auto& block = resources.push_constant_buffers.front();
		const auto& type = compiler.get_type(block.base_type_id);
		reflection.hasPushConstants = true;
		reflection.pushConstantId = block.id;
		reflection.pushConstantTypeId = block.base_type_id;
		reflection.pushConstantSize =
			static_cast<uint32_t>(compiler.get_declared_struct_size(type));
		if (reflection.pushConstantSize > kMaxPushConstantBytes) {
			fail(path, "push constant block is " +
						   std::to_string(reflection.pushConstantSize) +
						   " bytes; the limit is " +
						   std::to_string(kMaxPushConstantBytes));
		}
		for (uint32_t i = 0; i < type.member_types.size(); i++) {
			const std::string memberName =
				compiler.get_member_name(block.base_type_id, i);
			// get_member_decoration(MatrixStride) only reports presence in
			// this SPIRV-Cross version; the typed accessor returns the value.
			const uint32_t stride =
				compiler.get_type(type.member_types[i]).columns > 1
					? compiler.type_struct_member_matrix_stride(type, i)
					: 0;
			reflection.members.emplace_back(
				compiler.type_struct_member_offset(type, i),
				pushConstantTypeOf(path, memberName,
								   compiler.get_type(type.member_types[i]),
								   stride));
		}
	}
	return reflection;
}

/**
 * Cross-compiles SPIR-V to desktop GLSL. Below 420, layout(binding) does not
 * exist, so bindings are omitted and recorded for the runtime instead.
 */
std::string emitGlsl(spirv_cross::CompilerGLSL& compiler, uint32_t version,
					 const Reflection& reflection) {
	spirv_cross::CompilerGLSL::Options options;
	options.version = version;
	options.es = false;
	options.vulkan_semantics = false;
	options.enable_420pack_extension = false;
	compiler.set_common_options(options);
	if (reflection.hasPushConstants &&
		compiler.get_name(reflection.pushConstantId).empty()) {
		// Stable, stage-independent name so vert and frag link as one.
		compiler.set_name(reflection.pushConstantId, "pushConstants");
	}
	if (version < 420) {
		for (const auto& bound : reflection.bound) {
			compiler.unset_decoration(bound.id, spv::DecorationBinding);
		}
	}
	return compiler.compile();
}

std::vector<uint8_t> toBytes(const Words& words) {
	const auto* begin = reinterpret_cast<const uint8_t*>(words.data());
	return std::vector<uint8_t>(begin, begin + words.size() * sizeof(uint32_t));
}

std::vector<uint8_t> toBytes(const std::string& text) {
	return std::vector<uint8_t>(text.begin(), text.end());
}

}  // namespace

format::ShaderStageKind stageFromPath(const std::string& path) {
	return stageInfoFor(path).stage;
}

ShaderBinary bakeShader(const std::string& path, const std::string& source) {
	const StageInfo& info = stageInfoFor(path);
	const Words spirv = compileToSpirv(path, source, info.kind);
	validateSpirv(path, spirv);
	const Reflection reflection = reflect(path, spirv, info.stage);

	spirv_cross::CompilerGLSL glsl460(spirv);
	const std::string source460 = emitGlsl(glsl460, 460, reflection);
	spirv_cross::CompilerGLSL glsl410(spirv);
	const std::string source410 = emitGlsl(glsl410, 410, reflection);
	validateGlsl(path, source460, info.kind, "GLSL 460");
	validateGlsl(path, source410, info.kind, "GLSL 410");

	ShaderBinary binary;
	binary.stage = info.stage;
	binary.sourceHash = hashShaderSource(source);
	binary.variants[format::SHADER_TARGET_SPIRV] = toBytes(spirv);
	binary.variants[format::SHADER_TARGET_GLSL_460] = toBytes(source460);
	binary.variants[format::SHADER_TARGET_GLSL_410] = toBytes(source410);

	// Names are read back after compile(), so they match the emitted text.
	for (const auto& bound : reflection.bound) {
		const std::string name =
			bound.kind == format::SHADER_RESOURCE_UNIFORM_BUFFER
				? glsl410.get_remapped_declared_block_name(bound.id)
				: glsl410.get_name(bound.id);
		binary.resources.push_back({bound.kind, bound.binding, name});
	}
	if (reflection.hasPushConstants) {
		binary.pushConstantSize = reflection.pushConstantSize;
		const std::string instance = glsl410.get_name(reflection.pushConstantId);
		for (uint32_t i = 0; i < reflection.members.size(); i++) {
			binary.pushConstants.push_back(
				{instance + "." +
					 glsl410.get_member_name(reflection.pushConstantTypeId, i),
				 reflection.members[i].first, reflection.members[i].second});
		}
	}
	return binary;
}

bool needsShaderBake(const std::string& path, const std::string& source) {
	return !isShaderBinaryCurrent(path + ".bird", hashShaderSource(source));
}

}  // namespace bird::bake
