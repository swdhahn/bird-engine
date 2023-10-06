//
// Created by Seth Hahn on 12/3/22.
//

#ifndef BIRD_SHADER_H
#define BIRD_SHADER_H

#include <vector>
#include <string>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include "../GraphicsPipeline.h"
#include "../../util/File.h"

namespace bird {

    enum ShaderPipeline {
        VERTEX = 1,
        FRAGMENT = 2,
        GEOMETRY = 4,
        TESSELATION = 8
    };

    class Shader;

    /*
     * TODO: UBO turn into uint32_t bc all u need is index?
     */
    class ShaderBuilder {
    public:
        std::unique_ptr<Shader> create();
        ShaderBuilder& attachShaderFile(std::string fileName, const ShaderPipeline shaderStage);
        ShaderBuilder& addUniformBuffer(const ShaderPipeline shaderStage);
        ShaderBuilder& enableTextures(const uint8_t textureCount);
    private:
        std::vector<std::pair<std::string, ShaderPipeline>> m_shaderFiles;
        uint8_t m_textureCount;

    };

    class Shader {
    public:
        Shader();
        virtual ~Shader();

    protected:
        ShaderPipeline m_pipeline;
        uint8_t m_textureCount;

    };

    std::string preprocessShader(const std::string& source_name, shaderc_shader_kind kind,
                                  const std::string& source);
    std::vector<uint32_t> compileShader(const std::string& source_name,
                                       shaderc_shader_kind kind,
                                       bool optimize = true);
    std::string decompileShader_glsl(std::vector<uint32_t> spirv_binary);

} // bird

#endif //BIRD_SHADER_H
