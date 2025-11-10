//
// Created by Seth Hahn on 12/3/22.
//

#ifndef BIRD_SHADER_H
#define BIRD_SHADER_H

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <string>
#include <vector>

#include "../../util/File.h"
#include "../../util/Resource.h"
#include "../GraphicsConstants.h"

namespace bird {

enum ShaderPipeline { VERTEX = 1, FRAGMENT = 2, GEOMETRY = 4, TESSELATION = 8 };

class Shader;

/*
 * TODO: UBO turn into uint32_t bc all u need is index?
 */
class ShaderBuilder {
   public:
	std::unique_ptr<Shader> create();
	ShaderBuilder& attachShaderFile(std::string fileName,
									const ShaderPipeline shaderStage);

	std::vector<std::pair<std::string, ShaderPipeline>> m_shaderFiles;
};

class Shader : public Resource {
   public:
	Shader();
	virtual ~Shader();

	void write() override;
	void read() override;

	void initResources(std::vector<uint32_t> spirv_binary);
	virtual void loadPerspectiveMatrix(Matrix4 perspectiveMatrix) = 0;
	virtual void loadViewMatrix(Matrix4 viewMatrix) = 0;
	virtual void loadModelMatrix(Matrix4 modelMatrix) = 0;

   protected:
	ShaderPipeline m_pipeline;
	uint8_t m_textureCount;
	std::vector<std::string> m_variableNames;
};

std::string preprocessShader(const std::string& source_name,
							 shaderc_shader_kind kind,
							 const std::string& source);
std::vector<uint32_t> compileShader(const std::string& source,
									const std::string& source_name,
									const std::string& prevSource,
									shaderc_shader_kind kind,
									bool optimize = false);
std::pair<std::string, std::vector<std::pair<uint32_t, std::string>>>
decompileShader_glsl(std::vector<uint32_t> spirv_binary,
					 const std::string& prevShaderSource,
					 ShaderPipeline pipeline);

}  // namespace bird

#endif	// BIRD_SHADER_H
