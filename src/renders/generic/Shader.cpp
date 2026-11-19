//
// Created by Seth Hahn on 12/3/22.
//

#include "Shader.h"

#include <stdexcept>

#include "GLShader.h"

namespace bird {

std::string ShaderBuilder::cacheKey() const {
	std::string key;
	for (const auto& stage : m_shaderFiles) {
		key += stage.first + "|";
	}
	return key;
}

std::shared_ptr<Shader> ShaderBuilder::build() const {
	if (m_shaderFiles.empty()) {
		throw std::logic_error("ShaderBuilder: no shader stage attached.");
	}
	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			return std::make_shared<gl::GLShader>(m_shaderFiles);
		default:
			throw std::runtime_error(
				"ShaderBuilder: the active graphics pipeline lacks shader "
				"support.");
	}
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
