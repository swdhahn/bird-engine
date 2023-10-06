//
// Created by student on 2/7/23.
//

#ifndef BIRD_GLSHADER_H
#define BIRD_GLSHADER_H

#include "../generic/Shader.h"

namespace bird::gl {

    class GLShader : public Shader {
    public:
        GLShader(std::vector<std::pair<std::string, ShaderPipeline>>& shaderFiles);
        ~GLShader() override;

        void bind() const;
        void unbind() const;

    private:
        uint32_t m_programId;

    };

}

#endif //BIRD_GLSHADER_H
