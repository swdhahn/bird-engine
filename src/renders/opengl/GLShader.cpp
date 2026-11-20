//
// Created by student on 2/7/23.
//

#include "GLShader.h"

#include "../../platform/VFS.h"
#include "GLApi.h"
#include "GLCapabilities.h"

#include <algorithm>
#include <optional>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

namespace bird::gl {

namespace {

GLenum glStageFor(format::ShaderStageKind stage) {
	const GLCapabilities& caps = GLCapabilities::current();
	switch (stage) {
		case format::SHADER_STAGE_VERTEX:
			return GL_VERTEX_SHADER;
		case format::SHADER_STAGE_FRAGMENT:
			return GL_FRAGMENT_SHADER;
		case format::SHADER_STAGE_GEOMETRY:
			if (!caps.geometryShaders) break;
			return GL_GEOMETRY_SHADER;
		case format::SHADER_STAGE_TESS_CONTROL:
			if (!caps.tessellationShaders) break;
			return GL_TESS_CONTROL_SHADER;
		case format::SHADER_STAGE_TESS_EVALUATION:
			if (!caps.tessellationShaders) break;
			return GL_TESS_EVALUATION_SHADER;
		case format::SHADER_STAGE_COMPUTE:
			break;	// GL 4.1 has no compute; not supported on this path
	}
	throw std::runtime_error(
		"Shader stage unsupported by this GL context (stage " +
		std::to_string(static_cast<int>(stage)) + ")");
}

/** Program bound through GLShader::bind; 0 when none. */
GLuint s_boundProgram = 0;

/**
 * Makes a program current for glUniform* calls and restores the previous
 * one afterwards. glProgramUniform* would avoid the switch but needs
 * GL 4.1 or ES 3.1; this form works on every context, WebGL included.
 */
class ScopedProgram {
   public:
	explicit ScopedProgram(GLuint program)
		: m_previous(s_boundProgram), m_switched(program != s_boundProgram) {
		if (m_switched) glUseProgram(program);
	}
	~ScopedProgram() {
		if (m_switched) glUseProgram(m_previous);
	}
	ScopedProgram(const ScopedProgram&) = delete;
	ScopedProgram& operator=(const ScopedProgram&) = delete;

   private:
	GLuint m_previous;
	bool m_switched;
};

/**
 * Loads a baked stage through the platform VFS, so shaders inside an APK or
 * an app bundle load like files on a desktop.
 * @throws ShaderBinaryError when the file is missing or invalid.
 */
ShaderBinary readBakedStage(const std::string& path) {
	std::optional<std::vector<uint8_t>> bytes =
		platform::VFS::get().readFile(path);
	if (!bytes) {
		throw ShaderBinaryError("Missing shader binary " + path +
								" (run BuildTools)");
	}
	try {
		return parseShaderBinary(*bytes);
	} catch (const ShaderBinaryError& error) {
		throw ShaderBinaryError(path + ": " + error.what());
	}
}

format::ShaderStageKind expectedStage(ShaderPipeline pipeline) {
	switch (pipeline) {
		case VERTEX:
			return format::SHADER_STAGE_VERTEX;
		case FRAGMENT:
			return format::SHADER_STAGE_FRAGMENT;
		case GEOMETRY:
			return format::SHADER_STAGE_GEOMETRY;
		default:
			throw std::runtime_error("Invalid shader stage.");
	}
}

std::string infoLog(uint32_t object, bool isProgram) {
	int32_t length = 0;
	if (isProgram) {
		glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
	} else {
		glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
	}
	std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
	if (isProgram) {
		glGetProgramInfoLog(object, length, nullptr, log.data());
	} else {
		glGetShaderInfoLog(object, length, nullptr, log.data());
	}
	return log;
}

format::ShaderTarget selectTarget() {
	if (const char* forced = std::getenv("BIRD_GL_SHADER_TARGET")) {
		if (std::strcmp(forced, "410") == 0) return format::SHADER_TARGET_GLSL_410;
		if (std::strcmp(forced, "460") == 0) return format::SHADER_TARGET_GLSL_460;
		if (std::strcmp(forced, "es300") == 0) {
			return format::SHADER_TARGET_GLSL_ES_300;
		}
		throw std::runtime_error(
			"BIRD_GL_SHADER_TARGET must be \"410\", \"460\" or \"es300\"");
	}
	return chooseShaderTarget(GLCapabilities::current());
}

}  // namespace

format::ShaderTarget GLShader::activeTarget() {
	// Requires a current context; the first program creation happens after
	// the window and GLEW are initialized.
	static const format::ShaderTarget target = selectTarget();
	return target;
}

GLShader::GLShader(
	std::vector<std::pair<std::string, ShaderPipeline>> shaderFiles) {
	const format::ShaderTarget target = activeTarget();
	std::vector<ShaderBinary> stages;
	stages.reserve(shaderFiles.size());
	for (const auto& [path, pipeline] : shaderFiles) {
		stages.push_back(readBakedStage(path + ".bird"));
		if (stages.back().stage != expectedStage(pipeline)) {
			throw std::runtime_error(path +
									 ": baked stage does not match the "
									 "requested shader stage");
		}
	}

	m_programId = glCreateProgram();
	std::vector<uint32_t> shaderIds;
	for (size_t i = 0; i < stages.size(); i++) {
		shaderIds.push_back(
			compileStage(stages[i], shaderFiles[i].first, target));
		glAttachShader(m_programId, shaderIds.back());
	}

	glLinkProgram(m_programId);
	for (uint32_t id : shaderIds) {
		glDetachShader(m_programId, id);
		glDeleteShader(id);
	}
	int32_t linked = 0;
	glGetProgramiv(m_programId, GL_LINK_STATUS, &linked);
	if (!linked) {
		const std::string log = infoLog(m_programId, true);
		glDeleteProgram(m_programId);
		throw std::runtime_error("Shader link failed (" +
								 shaderFiles.front().first + ", ...):\n" +
								 log);
	}

	if (target != format::SHADER_TARGET_GLSL_460) {
		applyResourceBindings(stages);
	}
	resolvePushConstants(stages);
}

uint32_t GLShader::compileStage(const ShaderBinary& binary,
								const std::string& path,
								format::ShaderTarget target) {
	const std::string source = binary.variantText(target);
	const char* sourcePtr = source.c_str();
	const uint32_t id = glCreateShader(glStageFor(binary.stage));
	glShaderSource(id, 1, &sourcePtr, nullptr);
	glCompileShader(id);
	int32_t compiled = 0;
	glGetShaderiv(id, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		const std::string log = infoLog(id, false);
		glDeleteShader(id);
		throw std::runtime_error(path + ": shader compile failed:\n" + log);
	}
	return id;
}

void GLShader::applyResourceBindings(const std::vector<ShaderBinary>& stages) {
	// GLSL 410 and ES 300 cannot express layout(binding); apply the baked
	// table.
	ScopedProgram scoped(m_programId);
	for (const ShaderBinary& stage : stages) {
		for (const ShaderResource& resource : stage.resources) {
			if (resource.kind == format::SHADER_RESOURCE_UNIFORM_BUFFER) {
				const uint32_t index =
					glGetUniformBlockIndex(m_programId, resource.name.c_str());
				if (index != GL_INVALID_INDEX) {
					glUniformBlockBinding(m_programId, index, resource.binding);
				}
			} else {
				const int32_t location =
					glGetUniformLocation(m_programId, resource.name.c_str());
				if (location != -1) {
					glUniform1i(location,
								static_cast<int32_t>(resource.binding));
				}
			}
		}
	}
}

void GLShader::resolvePushConstants(const std::vector<ShaderBinary>& stages) {
	// Stages share members by name; each uniform is resolved once.
	std::unordered_set<std::string> seen;
	for (const ShaderBinary& stage : stages) {
		m_pushConstantSize =
			std::max(m_pushConstantSize, stage.pushConstantSize);
		for (const PushConstantMember& member : stage.pushConstants) {
			if (!seen.insert(member.name).second) continue;
			m_pushConstants.push_back(
				{glGetUniformLocation(m_programId, member.name.c_str()),
				 member.offset, member.type});
		}
	}
}

void GLShader::setPushConstants(const void* data, uint32_t size,
								uint32_t offset) {
	if (static_cast<uint64_t>(offset) + size > m_pushConstantSize) {
		throw std::out_of_range("setPushConstants exceeds the block size");
	}
	const auto* bytes = static_cast<const uint8_t*>(data);
	ScopedProgram scoped(m_programId);
	for (const PushConstantSlot& slot : m_pushConstants) {
		const uint32_t end = slot.offset + pushConstantTypeSize(slot.type);
		if (slot.location == -1 || slot.offset < offset ||
			end > offset + size) {
			continue;  // member outside the written range, or unused
		}
		const uint8_t* src = bytes + (slot.offset - offset);
		auto floats = [src](size_t i) {
			float value;
			std::memcpy(&value, src + i * 4, 4);
			return value;
		};
		auto ints = [src](size_t i) {
			int32_t value;
			std::memcpy(&value, src + i * 4, 4);
			return value;
		};
		auto uints = [src](size_t i) {
			uint32_t value;
			std::memcpy(&value, src + i * 4, 4);
			return value;
		};
		const int32_t loc = slot.location;
		switch (slot.type) {
			case format::PUSH_CONSTANT_FLOAT:
				glUniform1f(loc, floats(0));
				break;
			case format::PUSH_CONSTANT_VEC2:
				glUniform2f(loc, floats(0), floats(1));
				break;
			case format::PUSH_CONSTANT_VEC3:
				glUniform3f(loc, floats(0), floats(1),
								   floats(2));
				break;
			case format::PUSH_CONSTANT_VEC4:
				glUniform4f(loc, floats(0), floats(1),
								   floats(2), floats(3));
				break;
			case format::PUSH_CONSTANT_INT:
				glUniform1i(loc, ints(0));
				break;
			case format::PUSH_CONSTANT_IVEC2:
				glUniform2i(loc, ints(0), ints(1));
				break;
			case format::PUSH_CONSTANT_IVEC3:
				glUniform3i(loc, ints(0), ints(1), ints(2));
				break;
			case format::PUSH_CONSTANT_IVEC4:
				glUniform4i(loc, ints(0), ints(1), ints(2),
								   ints(3));
				break;
			case format::PUSH_CONSTANT_UINT:
				glUniform1ui(loc, uints(0));
				break;
			case format::PUSH_CONSTANT_UVEC2:
				glUniform2ui(loc, uints(0), uints(1));
				break;
			case format::PUSH_CONSTANT_UVEC3:
				glUniform3ui(loc, uints(0), uints(1),
									uints(2));
				break;
			case format::PUSH_CONSTANT_UVEC4:
				glUniform4ui(loc, uints(0), uints(1),
									uints(2), uints(3));
				break;
			case format::PUSH_CONSTANT_MAT3: {
				// std430 pads each vec3 column to 16 bytes; GL wants 9 floats.
				float packed[9];
				for (size_t column = 0; column < 3; column++) {
					std::memcpy(&packed[column * 3], src + column * 16, 12);
				}
				glUniformMatrix3fv(loc, 1, GL_FALSE, packed);
				break;
			}
			case format::PUSH_CONSTANT_MAT4: {
				float matrix[16];
				std::memcpy(matrix, src, sizeof(matrix));
				glUniformMatrix4fv(loc, 1, GL_FALSE, matrix);
				break;
			}
		}
	}
}

GLShader::~GLShader() {
	if (s_boundProgram == m_programId) s_boundProgram = 0;
	glDeleteProgram(m_programId);
}

void GLShader::bind() const {
	glUseProgram(m_programId);
	s_boundProgram = m_programId;
}

void GLShader::unbind() const {
	glUseProgram(0);
	s_boundProgram = 0;
}

}  // namespace bird::gl
