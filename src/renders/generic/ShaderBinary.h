#ifndef BIRD_SHADERBINARY_H
#define BIRD_SHADERBINARY_H

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../build-tools/FormatHeaders.h"

namespace bird {

/** Raised when a .bird shader is missing, truncated or malformed. */
class ShaderBinaryError : public std::runtime_error {
   public:
	using std::runtime_error::runtime_error;
};

/** A uniform block or sampler, with the binding the shader expects. */
struct ShaderResource {
	format::ShaderResourceKind kind;
	uint32_t binding;
	std::string name;  // block name or sampler name in the GLSL variants
};

/** One push-constant member as it appears in the GLSL variants. */
struct PushConstantMember {
	std::string name;  // e.g. "pc.model"
	uint32_t offset;   // std430 byte offset within the block
	format::PushConstantType type;
};

/** A file pulled in through #include, as it was when baked. */
struct ShaderDependency {
	std::string path;  // normalized, relative to the working directory
	uint64_t contentHash;
};

/**
 * In-memory form of a baked shader stage. Produced by BuildTools, consumed
 * by the graphics backends; independent of any graphics API.
 */
struct ShaderBinary {
	format::ShaderStageKind stage = format::SHADER_STAGE_VERTEX;
	uint64_t sourceHash = 0;
	std::map<format::ShaderTarget, std::vector<uint8_t>> variants;
	std::vector<ShaderResource> resources;
	uint32_t pushConstantSize = 0;
	std::vector<PushConstantMember> pushConstants;
	std::vector<ShaderDependency> dependencies;

	/**
	 * @return the raw bytes of a variant.
	 * @throws ShaderBinaryError when the variant was not baked.
	 */
	const std::vector<uint8_t>& variant(format::ShaderTarget target) const;
	/** @return a GLSL variant as source text. */
	std::string variantText(format::ShaderTarget target) const;
};

/** @return the 64-bit FNV-1a hash used for staleness checks. O(n). */
uint64_t hashShaderSource(const std::string& source);

/** @return bytes occupied by a push-constant member under std430. */
uint32_t pushConstantTypeSize(format::PushConstantType type);

std::vector<uint8_t> serializeShaderBinary(const ShaderBinary& binary);
/**
 * Parses a serialized shader stage.
 * @throws ShaderBinaryError on bad magic, version mismatch or truncation.
 */
ShaderBinary parseShaderBinary(const std::vector<uint8_t>& bytes);

/** @throws ShaderBinaryError when the file is missing or invalid. */
ShaderBinary readShaderBinaryFile(const std::string& path);
/** @throws ShaderBinaryError when the file cannot be written. */
void writeShaderBinaryFile(const std::string& path, const ShaderBinary& binary);

}  // namespace bird

#endif	// !BIRD_SHADERBINARY_H
