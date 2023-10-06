//
// Created by student on 2/7/23.
//

#include "GLShader.h"

namespace bird::gl {

    GLShader::GLShader(std::vector<std::pair<std::string, ShaderPipeline>>& shaderFiles) {

        m_programId = glCreateProgram();
        uint32_t shader_ids[shaderFiles.size()];
        int success;
        char infoLog[512];
        for(int i = 0; i < shaderFiles.size(); i++) {
            shaderc_shader_kind shaderc_shader_type;
            uint32_t gl_shader_type;

            switch(shaderFiles[i].second) {
                case VERTEX:
                    shaderc_shader_type = shaderc_vertex_shader;
                    gl_shader_type = GL_VERTEX_SHADER;
                    break;
                case FRAGMENT:
                    shaderc_shader_type = shaderc_fragment_shader;
                    gl_shader_type = GL_FRAGMENT_SHADER;
                    break;
                case GEOMETRY:
                    shaderc_shader_type = shaderc_geometry_shader;
                    gl_shader_type = GL_GEOMETRY_SHADER;
                    break;
                default:
                    throw std::runtime_error("Invalid shader stage.");
            }

            std::string shader_source = decompileShader_glsl(compileShader(shaderFiles[i].first, shaderc_shader_type, true));
            const char* shader_source_c = shader_source.c_str();
            shader_ids[i] = glCreateShader(gl_shader_type);
            glShaderSource(shader_ids[i], 1, &shader_source_c, NULL);
            glCompileShader(shader_ids[i]);
            // check for shader compile errors

            glGetShaderiv(shader_ids[i], GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader_ids[i], 512, NULL, infoLog);
                std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
            }
            glAttachShader(m_programId, shader_ids[i]);
        }

        glLinkProgram(m_programId);
        // check for linking errors
        glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
        if (!success) {
                glGetProgramInfoLog(m_programId, 512, NULL, infoLog);
                std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }

        for(int i = 0; i < shaderFiles.size(); i++) {
            glDeleteShader(shader_ids[i]);
        }



    }

    GLShader::~GLShader() {
        unbind();
        glDeleteProgram(m_programId);
    }

    void GLShader::bind() const {
        glUseProgram(m_programId);
    }

    void GLShader::unbind() const {
        glUseProgram(0);
    }

} // bird::gl
