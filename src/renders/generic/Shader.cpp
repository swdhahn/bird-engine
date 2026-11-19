//
// Created by Seth Hahn on 12/3/22.
//

#include "Shader.h"

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

}  // namespace bird
