//
// Created by Seth Hahn on 12/3/22.
//

#ifndef BIRD_SHADER_H
#define BIRD_SHADER_H

#include <vector>
#include <string>

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
        Shader create();
        ShaderBuilder& attachShaderFile(std::string fileName, const ShaderPipeline shaderStage);
        ShaderBuilder& addUniformBuffer(const ShaderPipeline shaderStage);
        ShaderBuilder& enableTextures(const uint8_t textureCount);
    private:
        std::vector<std::pair<std::string, ShaderPipeline>> m_shaderFiles;
        uint8_t m_textureCount;


    };

    class Shader {
    public:

        virtual ~Shader();

    protected:
        ShaderPipeline m_pipeline;
        uint8_t m_textureCount;

    };

} // bird

#endif //BIRD_SHADER_H
