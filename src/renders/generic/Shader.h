//
// Created by Seth Hahn on 12/3/22.
//

#ifndef BIRD_SHADER_H
#define BIRD_SHADER_H

#include <string>
#include <vector>
#include <memory>
#include <utility>

#include "../../util/File.h"
#include "../../util/Resource.h"
#include "../GraphicsConstants.h"

namespace bird {

/**
 * Shader pipeline stage bitmask flags.
 */
enum ShaderPipeline { VERTEX = 1, FRAGMENT = 2, GEOMETRY = 4, TESSELATION = 8 };

class Shader;

/**
 * Fluent builder for linking multi-stage GPU shader programs.
 */
class ShaderBuilder {
   public:
	/** Compiles and links the configured shader files into a GPU Shader program. */
	std::unique_ptr<Shader> create();

	/**
	 * Attaches a shader stage file to be compiled into the program pipeline.
	 * @param fileName Path to the shader source file (or baked .bird shader).
	 * @param shaderStage Pipeline stage (VERTEX, FRAGMENT, etc.).
	 */
	ShaderBuilder& attachShaderFile(std::string fileName,
									const ShaderPipeline shaderStage);

	std::vector<std::pair<std::string, ShaderPipeline>> m_shaderFiles;
};

/**
 * GPU shader program pipeline abstraction.
 *
 * Encapsulates compiled vertex, fragment, and geometry stages. Manages uniform
 * block bindings and emulated push-constant uploads across Vulkan and OpenGL backends.
 */
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
	 * @param data Source bytes, typically a struct mirroring the block.
	 * @param size Number of bytes to write.
	 * @param offset Byte offset within the block.
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
