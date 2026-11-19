//
// Created by student on 2/7/23.
//

#include "GLShader.h"

#include <GL/glew.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

namespace bird::gl {

namespace {

GLenum glStageFor(format::ShaderStageKind stage) {
	switch (stage) {
		case format::SHADER_STAGE_VERTEX:
			return GL_VERTEX_SHADER;
		case format::SHADER_STAGE_FRAGMENT:
			return GL_FRAGMENT_SHADER;
		case format::SHADER_STAGE_GEOMETRY:
			return GL_GEOMETRY_SHADER;
		case format::SHADER_STAGE_TESS_CONTROL:
			return GL_TESS_CONTROL_SHADER;
		case format::SHADER_STAGE_TESS_EVALUATION:
			return GL_TESS_EVALUATION_SHADER;
		case format::SHADER_STAGE_COMPUTE:
			break;	// GL 4.1 has no compute; not supported on this path
	}
	throw std::runtime_error("Shader stage unsupported by the GL backend");
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
		throw std::runtime_error(
			"BIRD_GL_SHADER_TARGET must be \"410\" or \"460\"");
	}
	int32_t major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	const bool has46 = major > 4 || (major == 4 && minor >= 6);
	return has46 ? format::SHADER_TARGET_GLSL_460
				 : format::SHADER_TARGET_GLSL_410;
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
		stages.push_back(readShaderBinaryFile(path + ".bird"));
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

	if (target == format::SHADER_TARGET_GLSL_410) {
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
	// GLSL 410 cannot express layout(binding); apply the baked table.
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
					glProgramUniform1i(m_programId, location,
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
				glProgramUniform1f(m_programId, loc, floats(0));
				break;
			case format::PUSH_CONSTANT_VEC2:
				glProgramUniform2f(m_programId, loc, floats(0), floats(1));
				break;
			case format::PUSH_CONSTANT_VEC3:
				glProgramUniform3f(m_programId, loc, floats(0), floats(1),
								   floats(2));
				break;
			case format::PUSH_CONSTANT_VEC4:
				glProgramUniform4f(m_programId, loc, floats(0), floats(1),
								   floats(2), floats(3));
				break;
			case format::PUSH_CONSTANT_INT:
				glProgramUniform1i(m_programId, loc, ints(0));
				break;
			case format::PUSH_CONSTANT_IVEC2:
				glProgramUniform2i(m_programId, loc, ints(0), ints(1));
				break;
			case format::PUSH_CONSTANT_IVEC3:
				glProgramUniform3i(m_programId, loc, ints(0), ints(1), ints(2));
				break;
			case format::PUSH_CONSTANT_IVEC4:
				glProgramUniform4i(m_programId, loc, ints(0), ints(1), ints(2),
								   ints(3));
				break;
			case format::PUSH_CONSTANT_UINT:
				glProgramUniform1ui(m_programId, loc, uints(0));
				break;
			case format::PUSH_CONSTANT_UVEC2:
				glProgramUniform2ui(m_programId, loc, uints(0), uints(1));
				break;
			case format::PUSH_CONSTANT_UVEC3:
				glProgramUniform3ui(m_programId, loc, uints(0), uints(1),
									uints(2));
				break;
			case format::PUSH_CONSTANT_UVEC4:
				glProgramUniform4ui(m_programId, loc, uints(0), uints(1),
									uints(2), uints(3));
				break;
			case format::PUSH_CONSTANT_MAT3: {
				// std430 pads each vec3 column to 16 bytes; GL wants 9 floats.
				float packed[9];
				for (size_t column = 0; column < 3; column++) {
					std::memcpy(&packed[column * 3], src + column * 16, 12);
				}
				glProgramUniformMatrix3fv(m_programId, loc, 1, GL_FALSE, packed);
				break;
			}
			case format::PUSH_CONSTANT_MAT4: {
				float matrix[16];
				std::memcpy(matrix, src, sizeof(matrix));
				glProgramUniformMatrix4fv(m_programId, loc, 1, GL_FALSE, matrix);
				break;
			}
		}
	}
}

GLShader::~GLShader() {
	glDeleteProgram(m_programId);
}

void GLShader::bind() const {
	glUseProgram(m_programId);
}

void GLShader::unbind() const {
	glUseProgram(0);
}

}  // namespace bird::gl
