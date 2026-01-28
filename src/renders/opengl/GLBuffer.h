//
// Created by Seth Hahn on 11/18/22.
//

#ifndef BIRD_GLBUFFER_H
#define BIRD_GLBUFFER_H

#include <typeindex>

#include "../generic/Buffer.h"
#include <GL/glew.h>

namespace bird::gl {

template <typename T, uint8_t t_attributeSize = 3>
class GLBuffer : public Buffer<T, t_attributeSize> {
   public:
	GLBuffer(BufferMode bufferMode, size_t size);
	GLBuffer(BufferMode bufferMode, std::unique_ptr<T[]> pData, size_t size);
	~GLBuffer();

	void initialize() override;
	void initialize(std::unique_ptr<T[]> pData, size_t size) override;
	void update(T* pData, size_t size, uint32_t offset) override;

   private:
	uint32_t m_bufferId;
	uint32_t m_bufferTarget;
};
template <typename T, uint8_t t_attributeSize>
GLBuffer<T, t_attributeSize>::GLBuffer(BufferMode bufferMode, size_t size)
	: GLBuffer(bufferMode, nullptr, size) {}
template <typename T, uint8_t t_attributeSize>
GLBuffer<T, t_attributeSize>::GLBuffer(BufferMode bufferMode,
									   std::unique_ptr<T[]> pData, size_t size)
	: Buffer<T, t_attributeSize>(bufferMode, std::move(pData), size),
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

template <typename T, uint8_t t_attributeSize>
GLBuffer<T, t_attributeSize>::~GLBuffer() {
	glDeleteBuffers(1, &m_bufferId);
}

template <typename T, uint8_t t_attributeSize>
void GLBuffer<T, t_attributeSize>::initialize() {
	glBindBuffer(m_bufferTarget, m_bufferId);
	glBufferData(m_bufferTarget, Buffer<T, t_attributeSize>::m_size * sizeof(T),
				 Buffer<T, t_attributeSize>::m_pData.get(), GL_STATIC_DRAW);
	if (m_bufferTarget == GL_UNIFORM_BUFFER) {
		glBindBufferRange(
			m_bufferTarget, Buffer<T, t_attributeSize>::m_bindingPoint,
			m_bufferId, 0, Buffer<T, t_attributeSize>::m_size * sizeof(T));
	}
}

template <typename T, uint8_t t_attributeSize>
void GLBuffer<T, t_attributeSize>::initialize(std::unique_ptr<T[]> pData,
											  size_t size) {
	Buffer<T, t_attributeSize>::m_pData = std::move(pData);
	glBindBuffer(m_bufferTarget, m_bufferId);
	glBufferData(m_bufferTarget, Buffer<T, t_attributeSize>::m_size * sizeof(T),
				 Buffer<T, t_attributeSize>::m_pData.get(), GL_STATIC_DRAW);

	if (m_bufferTarget == GL_UNIFORM_BUFFER) {
		glBindBufferRange(
			m_bufferTarget, Buffer<T, t_attributeSize>::m_bindingPoint,
			m_bufferId, 0, Buffer<T, t_attributeSize>::m_size * sizeof(T));
	}
}

template <typename T, uint8_t t_attributeSize>
void GLBuffer<T, t_attributeSize>::update(T* pData, size_t size,
										  uint32_t offset) {
	glBindBuffer(m_bufferTarget, m_bufferId);

	if (Buffer<T, t_attributeSize>::m_bufferMode & BUFFER_STAGED) {
		glBufferSubData(m_bufferTarget, offset, size * sizeof(T), pData);
	} else if (Buffer<T, t_attributeSize>::m_bufferMode & BUFFER_MAPPED) {
		// memcpy(Buffer<T, t_attributeSize>::m_pData, pData + offset, size *
		// sizeof(T));
	}
}

}  // namespace bird::gl

#endif	// BIRD_GLBUFFER_H
