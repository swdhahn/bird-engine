//
// Created by Seth Hahn on 12/3/22.
//

#include "Shader.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "GLShader.h"

namespace bird {

std::unique_ptr<Shader> ShaderBuilder::create() {
	std::unique_ptr<Shader> ptr = nullptr;

	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			ptr = std::make_unique<gl::GLShader>(m_shaderFiles);

			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support shaders...");
	}

	m_shaderFiles.clear();

	return std::move(ptr);
}

ShaderBuilder& ShaderBuilder::attachShaderFile(
	std::string fileName, const ShaderPipeline shaderStage) {
	m_shaderFiles.push_back(
		std::pair<std::string, ShaderPipeline>(fileName, shaderStage));
	return *this;
}

Shader::Shader() {}

Shader::~Shader() {}

void Shader::write() {}

void Shader::read() {}

void Shader::initResources(std::vector<uint32_t> spirv_binary) {
	spirv_cross::Compiler comp(std::move(spirv_binary));

	spirv_cross::ShaderResources res = comp.get_shader_resources();

	spirv_cross::BufferRange ranges =
		comp.get_active_buffer_ranges(res.push_constant_buffers.front().id)
			.front();

	/*for (int i = 0; i < res.push_constant_buffers.size(); i++) {
		std::cout << res.push_constant_buffers[i].name << "  "
				  << res.push_constant_buffers[i].type_id << "   "
				  << res.push_constant_buffers[i].id << "   " << std::endl;
	}
	//*/
}

std::string preprocessShader(const std::string& source_name, int32_t kind,
							 const std::string& source) {
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;

	// Like -DMY_DEFINE=1
	// options.AddMacroDefinition("MY_DEFINE", "1");

	shaderc::PreprocessedSourceCompilationResult result =
		compiler.PreprocessGlsl(source, (shaderc_shader_kind)kind,
								source_name.c_str(), options);

	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << result.GetErrorMessage();
		return "";
	}

	return {result.cbegin(), result.cend()};
}

std::vector<uint32_t> compileShader(const std::string& source_name,
									const std::string& source,
									const std::string& prevSource, int32_t kind,
									bool optimize) {
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;

	// std::string source = readFileToString(source_name);

	// Like -DMY_DEFINE=1
	// options.AddMacroDefinition("MY_DEFINE", "1");
	if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);
	options.SetPreserveBindings(true);
	options.SetGenerateDebugInfo();

	shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
		source, (shaderc_shader_kind)kind, (source_name).c_str(), options);

	if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << module.GetErrorMessage();
		return std::vector<uint32_t>();
	}

	return {module.cbegin(), module.cend()};
}

std::pair<std::string, std::vector<std::pair<uint32_t, std::string>>>
decompileShader_glsl(std::vector<uint32_t> spirv_binary,
					 const std::string& prevShaderSource,
					 ShaderPipeline pipeline) {
	spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));
	auto resources = glsl.get_shader_resources();

	std::vector<std::pair<uint32_t, std::string>> uniform_buffers(
		resources.uniform_buffers.size());

	for (int i = 0; i < resources.uniform_buffers.size(); i++) {
		uniform_buffers[i] = std::pair<uint32_t, std::string>(
			glsl.get_decoration(resources.uniform_buffers[i].id,
								spv::DecorationBinding),
			resources.uniform_buffers[i].name);
	}

	// Set some options.
	spirv_cross::CompilerGLSL::Options options;
	options.enable_420pack_extension = false;
	options.version = 330;
	options.es = false;
	// options.separate_shader_objects = true;
	glsl.set_common_options(options);

	// Compile to GLSL, ready to give to GL driver.
	return std::pair<std::string,
					 std::vector<std::pair<uint32_t, std::string>>>(
		glsl.compile(), uniform_buffers);
}

}  // namespace bird
