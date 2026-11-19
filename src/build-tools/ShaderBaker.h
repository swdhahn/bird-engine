#ifndef BIRD_SHADERBAKER_H
#define BIRD_SHADERBAKER_H

#include <stdexcept>
#include <string>

#include "../renders/generic/ShaderBinary.h"

namespace bird::bake {

/** Raised when a shader violates the authoring rules or fails to compile. */
class ShaderBakeError : public std::runtime_error {
   public:
	using std::runtime_error::runtime_error;
};

/** Largest push-constant block guaranteed by every Vulkan implementation. */
constexpr uint32_t kMaxPushConstantBytes = 128;

/**
 * @param path - shader file path; the extension selects the stage.
 * @throws ShaderBakeError for an unsupported extension.
 */
format::ShaderStageKind stageFromPath(const std::string& path);

/**
 * Bakes one Vulkan-style GLSL stage: GLSL -> SPIR-V (shaderc), validated,
 * reflected, then SPIR-V -> GLSL 460 and GLSL 410 (SPIRV-Cross).
 *
 * `#include "file"` resolves relative to the including file; every included
 * file is recorded with its content hash for staleness checks. Include
 * files should use an extension BuildTools does not bake (e.g. .glsl).
 *
 * Authoring rules: explicit location on every in/out, explicit binding on
 * every uniform block and sampler, descriptor set 0, at most one
 * push_constant block of at most kMaxPushConstantBytes holding scalars,
 * vectors, mat3 or mat4.
 *
 * @param path - used for the stage and in error messages.
 * @param source - GLSL source text.
 * @return the stage with SPIRV, GLSL_460 and GLSL_410 variants.
 * @throws ShaderBakeError naming the file and the violated rule.
 */
ShaderBinary bakeShader(const std::string& path, const std::string& source);

/**
 * @return true when path + ".bird" is missing, from an older format, or was
 *         baked from a different source or different included files.
 */
bool needsShaderBake(const std::string& path, const std::string& source);

}  // namespace bird::bake

#endif	// !BIRD_SHADERBAKER_H
