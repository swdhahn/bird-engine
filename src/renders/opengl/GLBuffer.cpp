//
// Created by Seth Hahn on 11/18/22.
//

#include "GLBuffer.h"

#include "GLApi.h"

#include <algorithm>
#include <stdexcept>

namespace bird::gl {

GLBuffer::GLBuffer(BufferMode bufferMode, size_t size, uint16_t dataSize)
	: GLBuffer(bufferMode, nullptr, size, dataSize) {}

GLBuffer::GLBuffer(BufferMode bufferMode, void* pData, size_t size,
				   uint16_t dataSize)
	: RawBuffer(bufferMode, pData, size, dataSize),
	  m_bufferId(0),
	  m_bufferTarget(0) {
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
	glBufferData(
		m_bufferTarget, m_size * m_dataSize, m_pData,
		m_bufferMode & BUFFER_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	if (m_bufferTarget == GL_UNIFORM_BUFFER) {
		glBindBufferRange(m_bufferTarget, m_bindingPoint, m_bufferId, 0,
						  m_size * m_dataSize);
	}
}

void GLBuffer::initialize(void* pData, size_t size) {
	glBindBuffer(m_bufferTarget, m_bufferId);
	m_size = size;
	glBufferData(
		m_bufferTarget, size * m_dataSize, pData,
		m_bufferMode & BUFFER_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

	if (m_bufferTarget == GL_UNIFORM_BUFFER) {
		glBindBufferRange(m_bufferTarget, m_bindingPoint, m_bufferId, 0,
						  size * m_dataSize);
	}
}

void GLBuffer::update(void* pData, size_t size, uint32_t offset) {
	glBindBuffer(m_bufferTarget, m_bufferId);

	if (m_bufferMode & BUFFER_STAGED) {
		const size_t requiredSize = static_cast<size_t>(offset) + size;
		if (requiredSize > m_size) {
			// Geometric growth amortizes reallocations to O(1) per element.
			// Reallocation discards prior contents, so only a write starting
			// at offset zero may trigger it.
			if (offset != 0) {
				throw std::out_of_range(
					"GLBuffer::update: offset write exceeds buffer capacity.");
			}
			m_size = std::max(requiredSize, m_size * 2);
			glBufferData(m_bufferTarget, m_size * m_dataSize, nullptr,
						 m_bufferMode & BUFFER_DYNAMIC ? GL_DYNAMIC_DRAW
													   : GL_STATIC_DRAW);
		}
		glBufferSubData(m_bufferTarget, offset * m_dataSize, size * m_dataSize,
						pData);
	} else if (m_bufferMode & BUFFER_MAPPED) {
		// memcpy(Buffer<T, t_attributeSize>::m_pData, pData + offset, size *
		// sizeof(T));
	}
}

void GLBuffer::bindToBindingPoint() {
	if (m_bufferTarget != GL_UNIFORM_BUFFER) {
		throw std::logic_error(
			"GLBuffer::bindToBindingPoint: only uniform buffers bind to "
			"indexed binding points.");
	}
	glBindBufferBase(GL_UNIFORM_BUFFER, m_bindingPoint, m_bufferId);
}

}  // namespace bird::gl
