//
// Created by Seth Hahn on 11/18/22.
//

#include "GLBuffer.h"

#include <GL/glew.h>

namespace bird::gl {

GLBuffer::GLBuffer(BufferMode bufferMode, size_t size)
	: GLBuffer(bufferMode, nullptr, size) {}

GLBuffer::GLBuffer(BufferMode bufferMode, void* pData, size_t size)
	: RawBuffer(bufferMode, pData, size), m_bufferId(0), m_bufferTarget(0) {
	glGenBuffers(1, &m_bufferId);

	if (bufferMode & BUFFER_ARRAY) {
		m_bufferTarget = GL_ARRAY_BUFFER;
	} else if (bufferMode & BUFFER_ELEMENT_ARRAY) {
		m_bufferTarget = GL_ELEMENT_ARRAY_BUFFER;
	} else if (bufferMode & BUFFER_TEXTURE) {
		m_bufferTarget = BUFFER_TEXTURE;
	} else if (bufferMode & BUFFER_UNIFORM) {
		m_bufferTarget = GL_UNIFORM_BUFFER;
	} else {
		throw std::invalid_argument(
			"You must declare buffer's target in bufferMode parameter: "
			"BUFFER_ARRAY, BUFFER_ELEMENT_ARRAY, BUFFER_TEXTURE, or "
			"BUFFER_UNIFORM.");
	}
	if (bufferMode & BUFFER_MAPPED) {
		// Buffer<T, t_attributeSize>::m_pData = glMapBuffer(m_bufferTarget,
		// GL_WRITE_ONLY);
		std::cout << "Sorry! Buffer mapping is not implemented!!" << std::endl;
	} else if (!(bufferMode & BUFFER_STAGED | bufferMode & BUFFER_MAPPED)) {
		throw std::invalid_argument(
			"You must declare buffer as BUFFER_MAPPED or BUFFER_STAGED in "
			"bufferMode parameter.");
	}
}

GLBuffer::~GLBuffer() {
	glDeleteBuffers(1, &m_bufferId);
}

void GLBuffer::initialize() {
	glBindBuffer(m_bufferTarget, m_bufferId);
	glBufferData(m_bufferTarget, m_size * m_dataSize, m_pData, GL_STATIC_DRAW);
	if (m_bufferTarget == GL_UNIFORM_BUFFER) {
		glBindBufferRange(m_bufferTarget, m_bindingPoint, m_bufferId, 0,
						  m_size * m_dataSize);
	}
}

void GLBuffer::initialize(void* pData, size_t size) {
	glBindBuffer(m_bufferTarget, m_bufferId);
	glBufferData(m_bufferTarget, m_size * m_dataSize, m_pData, GL_STATIC_DRAW);

	if (m_bufferTarget == GL_UNIFORM_BUFFER) {
		glBindBufferRange(m_bufferTarget, m_bindingPoint, m_bufferId, 0,
						  m_size * m_dataSize);
	}
}

void GLBuffer::update(void* pData, size_t size, uint32_t offset) {
	glBindBuffer(m_bufferTarget, m_bufferId);

	if (m_bufferMode & BUFFER_STAGED) {
		glBufferSubData(m_bufferTarget, offset, size * m_dataSize, pData);
	} else if (m_bufferMode & BUFFER_MAPPED) {
		// memcpy(Buffer<T, t_attributeSize>::m_pData, pData + offset, size *
		// sizeof(T));
	}
}

}  // namespace bird::gl
