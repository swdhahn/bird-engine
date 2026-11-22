//
// Created by student on 2/7/23.
//

#ifndef BIRD_GLSHADER_H
#define BIRD_GLSHADER_H

#include <string>
#include <utility>
#include <vector>

#include "../generic/Shader.h"
#include "../generic/ShaderBinary.h"
#include "GLBuffer.h"

namespace bird::gl {

/**
 * An OpenGL program linked from baked shader stages (<file>.bird). Uses the
 * GLSL ES 300 variant on OpenGL ES and WebGL, GLSL 460 on desktop 4.6+ and
 * GLSL 410 otherwise (macOS); the BIRD_GL_SHADER_TARGET environment
 * variable ("410", "460" or "es300") overrides.
 */
class GLShader : public Shader {
   public:
	/**
	 * @param shaderFiles - source paths and stages; each loads <path>.bird.
	 * @throws ShaderBinaryError when a .bird is missing or invalid.
	 * @throws std::runtime_error on a stage mismatch or compile/link error.
	 */
	GLShader(std::vector<std::pair<std::string, ShaderPipeline>> shaderFiles);
	~GLShader() override;

	void setPushConstants(const void* data, uint32_t size,
						  uint32_t offset = 0) override;

	/**
	 * Swaps the OpenGL program handle and push-constant uniform locations in
	 * place with another GLShader. Safe to call on the main GL thread during
	 * hot-reload.
	 */
	void swapFrom(Shader& other) override;

	void bind() const;
	void unbind() const;

	/** @return the variant this process uses for every GL program. */
	static format::ShaderTarget activeTarget();

   private:
	struct PushConstantSlot {
		int32_t location;  // -1 when the driver optimized the member away
		uint32_t offset;
		format::PushConstantType type;
	};

	uint32_t compileStage(const ShaderBinary& binary, const std::string& path,
						  format::ShaderTarget target);
	void applyResourceBindings(const std::vector<ShaderBinary>& stages);
	void resolvePushConstants(const std::vector<ShaderBinary>& stages);

	uint32_t m_programId;
	uint32_t m_pushConstantSize = 0;
	std::vector<PushConstantSlot> m_pushConstants;
};

}  // namespace bird::gl

#endif	// BIRD_GLSHADER_H
