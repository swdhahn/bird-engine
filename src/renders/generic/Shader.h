//
// Created by Seth Hahn on 12/3/22.
//

#ifndef BIRD_SHADER_H
#define BIRD_SHADER_H

#include <string>
#include <vector>
#include <memory>
#include <utility>

#include "../../util/Constants.h"
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
 *
 * Shader stages are disk assets, so programs are normally obtained through
 * ResourceManager::loadShader, which caches them by cacheKey(). Calling
 * build() directly yields an uncached program.
 */
class ShaderBuilder {
   public:
	/**
	 * Attaches a shader stage file to be compiled into the program pipeline.
	 * @param fileName Path to the shader source file (or baked .bird shader).
	 * @param shaderStage Pipeline stage (VERTEX, FRAGMENT, etc.).
	 */
	ShaderBuilder& attachShaderFile(std::string fileName,
									const ShaderPipeline shaderStage);

	/**
	 * Identifies the program by its ordered stage paths.
	 * @return Every stage path, each followed by '|'.
	 */
	std::string cacheKey() const;

	/**
	 * Compiles and links the attached stages using the active backend.
	 * @return The linked program.
	 * @throws std::logic_error when no stage is attached.
	 * @throws std::runtime_error when the backend lacks shader support.
	 */
	std::shared_ptr<Shader> build() const;

	/** @return Ordered list of attached (sourcePath, stage) pairs. */
	BIRD_NO_EXPORT const std::vector<std::pair<std::string, ShaderPipeline>>&
	stages() const {
		return m_shaderFiles;
	}

   private:
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

	/**
	 * Swaps underlying GPU program handles and reflection metadata in place
	 * with another Shader of the same backend type.
	 *
	 * Used by main-thread hot-reloading so every existing std::shared_ptr<Shader>
	 * immediately uses the newly linked program while the old program is freed
	 * when `other` is destroyed.
	 *
	 * @param other Freshly built Shader whose handles will be swapped into *this.
	 */
	BIRD_NO_EXPORT virtual void swapFrom(Shader& other);

   protected:
	ShaderPipeline m_pipeline;
	uint8_t m_textureCount;
	std::vector<std::string> m_variableNames;
};

}  // namespace bird

#endif	// BIRD_SHADER_H
