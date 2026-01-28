//
// Created by student on 2/7/23.
//

#include "GLShader.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_glsl.hpp>


namespace bird::gl {

GLShader::GLShader(
	std::vector<std::pair<std::string, ShaderPipeline>> shaderFiles) {
	m_programId = glCreateProgram();
	uint32_t shader_ids[shaderFiles.size()];
	int success;
	char infoLog[512];
	std::string prev_shader_source = "";
	std::vector<std::pair<uint32_t, std::string>> uniformBuffers;
	for (int i = 0; i < shaderFiles.size(); i++) {
		shaderc_shader_kind shaderc_shader_type;
		uint32_t gl_shader_type;

		switch (shaderFiles[i].second) {
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

		std::string raw_shader_source = readFileToString(shaderFiles[i].first);
		auto spirvComped =
			compileShader(shaderFiles[i].first, raw_shader_source,
						  prev_shader_source, shaderc_shader_type, true);

		// initResources(spirvComped);

		std::pair<std::string, std::vector<std::pair<uint32_t, std::string>>>
			decompShader = decompileShader_glsl(spirvComped, prev_shader_source,
												shaderFiles[i].second);
		std::string shader_source = decompShader.first;
		for (int i = 0; i < decompShader.second.size(); i++) {
			bool flag = false;
			for (int j = 0; j < uniformBuffers.size(); j++) {
				if (uniformBuffers[j].second == decompShader.second[i].second) {
					if (uniformBuffers[j].first !=
						decompShader.second[i].first) {
						throw std::runtime_error(
							"Uniform buffer index mismatch. Make sure all "
							"uniform buffers have the same index across all "
							"shaders.");
					}
					flag = true;
				}
			}
			if (flag) continue;
			uniformBuffers.push_back(decompShader.second[i]);
		}
		prev_shader_source = shader_source;
		// std::cout << "----\n" << shader_source << "\n----\n";

		const char* shader_source_c = shader_source.c_str();
		shader_ids[i] = glCreateShader(gl_shader_type);
		glShaderSource(shader_ids[i], 1, &shader_source_c, NULL);
		glCompileShader(shader_ids[i]);
		// check for shader compile errors

		glGetShaderiv(shader_ids[i], GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shader_ids[i], 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::COMPILATION_FAILED\n"
					  << infoLog << std::endl;
		}
		glAttachShader(m_programId, shader_ids[i]);
	}

	glLinkProgram(m_programId);
	// check for linking errors
	glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(m_programId, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
				  << infoLog << std::endl;
	}

	for (int i = 0; i < shaderFiles.size(); i++) {
		glDetachShader(m_programId, shader_ids[i]);
		glDeleteShader(shader_ids[i]);
	}
	glValidateProgram(m_programId);
	// bind();
	unbind();

	for (int i = 0; i < uniformBuffers.size(); i++) {
		uint32_t block_index = glGetUniformBlockIndex(
			m_programId, uniformBuffers[i].second.c_str());
		glUniformBlockBinding(m_programId, block_index,
							  uniformBuffers[i].first);
		/*std::cout << "Uniform block index: " << uniformBuffers[i].first << " "
				  << uniformBuffers[i].second << std::endl;*/
	}
}

GLShader::~GLShader() {
	glDeleteProgram(m_programId);
}

void GLShader::loadPerspectiveMatrix(Matrix4 perspectiveMatrix) {
	glUniformMatrix4fv(
		glGetUniformLocation(m_programId, "registerMapped.perspective"), 1,
		GL_FALSE, &perspectiveMatrix[0][0]);
}

void GLShader::loadViewMatrix(Matrix4 viewMatrix) {
	glUniformMatrix4fv(glGetUniformLocation(m_programId, "registerMapped.view"),
					   1, GL_FALSE, &viewMatrix[0][0]);
}

void GLShader::loadModelMatrix(Matrix4 modelMatrix) {
	glUniformMatrix4fv(
		glGetUniformLocation(m_programId, "registerMapped.model"), 1, GL_FALSE,
		&modelMatrix[0][0]);
}

void GLShader::bind() const {
	glUseProgram(m_programId);
}

void GLShader::unbind() const {
	glUseProgram(0);
}

}  // namespace bird::gl
