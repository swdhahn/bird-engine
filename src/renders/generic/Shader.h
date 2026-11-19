//
// Created by Seth Hahn on 12/3/22.
//

#ifndef BIRD_SHADER_H
#define BIRD_SHADER_H

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

	/**
	 * Writes push-constant bytes, laid out exactly like the shader's
	 * push_constant block (std430). Vulkan forwards them to
	 * vkCmdPushConstants; OpenGL writes the matching uniforms.
	 * @param data - source bytes, typically a struct mirroring the block.
	 * @param size - number of bytes to write.
	 * @param offset - byte offset within the block.
	 * @throws std::out_of_range when the range exceeds the block.
	 */
	virtual void setPushConstants(const void* data, uint32_t size,
								  uint32_t offset = 0) = 0;

   protected:
	ShaderPipeline m_pipeline;
	uint8_t m_textureCount;
	std::vector<std::string> m_variableNames;
};

}  // namespace bird

#endif	// BIRD_SHADER_H
