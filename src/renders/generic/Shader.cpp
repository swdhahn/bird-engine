//
// Created by Seth Hahn on 12/3/22.
//

#include "Shader.h"
#include "../GraphicsPipeline.h"
#include "GLShader.h"

namespace bird {

    std::unique_ptr<Shader> ShaderBuilder::create() {

        std::unique_ptr<Shader> ptr = nullptr;

        switch(GraphicsPipeline::getGraphicsPipelineType()) {
            case GRAPHICS_PIPELINE_OPENGL:
                ptr = std::make_unique<gl::GLShader>(m_shaderFiles);

                break;
            default:
                throw std::runtime_error("No other graphics pipelines currently support shaders...");
        }

        return std::move(ptr);
    }

    ShaderBuilder& ShaderBuilder::attachShaderFile(std::string fileName, const ShaderPipeline shaderStage) {
        m_shaderFiles.push_back(std::pair<std::string, ShaderPipeline>(fileName, shaderStage));
        return *this;
    }

    ShaderBuilder& ShaderBuilder::addUniformBuffer(const ShaderPipeline shaderStage) {
        throw std::runtime_error("Uniform Buffers are not yet implemented properly.");
        return *this;
    }

    ShaderBuilder& ShaderBuilder::enableTextures(const uint8_t textureCount) {
        m_textureCount = textureCount;
        return *this;
    }

    Shader::Shader() {

    }

    Shader::~Shader() {

    }


    std::string preprocessShader(const std::string& source_name, shaderc_shader_kind kind,
                                 const std::string& source) {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        // Like -DMY_DEFINE=1
        options.AddMacroDefinition("MY_DEFINE", "1");

        shaderc::PreprocessedSourceCompilationResult result =
                compiler.PreprocessGlsl(source, kind, source_name.c_str(), options);



        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::cerr << result.GetErrorMessage();
            return "";
        }

        return {result.cbegin(), result.cend()};
    }

    std::vector<uint32_t> compileShader(const std::string& source_name,
                                        shaderc_shader_kind kind,
                                        bool optimize) {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        // Like -DMY_DEFINE=1
        //options.AddMacroDefinition("MY_DEFINE", "1");
        if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

        shaderc::SpvCompilationResult module =
                compiler.CompileGlslToSpv(readFileToString(source_name), kind, (source_name).c_str(), options);

        if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::cerr << module.GetErrorMessage();
            return std::vector<uint32_t>();
        }

        return {module.cbegin(), module.cend()};
    }

    std::string decompileShader_glsl(std::vector<uint32_t> spirv_binary) {
        spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

        // The SPIR-V is now parsed, and we can perform reflection on it.
        spirv_cross::ShaderResources resources = glsl.get_shader_resources();

        // Set some options.
        spirv_cross::CompilerGLSL::Options options;
        options.enable_420pack_extension = false;
        options.version = 330;
        options.es = false;
        glsl.set_common_options(options);

        // Compile to GLSL, ready to give to GL driver.
        return glsl.compile();
    }

} // bird