//
// Created by Seth Hahn on 11/18/22.
//

#ifndef BIRD_GLBUFFER_H
#define BIRD_GLBUFFER_H

#include <typeindex>
#include <cstdint>
#include <cstddef>
#include <iostream>

#include "../generic/RawBuffer.h"

namespace bird::gl {

/**
 * OpenGL buffer object implementation (VBO, EBO, UBO).
 *
 * Maps BufferMode flags to OpenGL targets (GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER,
 * GL_UNIFORM_BUFFER) and handles GPU uploads via glBufferData and glBufferSubData.
 */
class GLBuffer : public RawBuffer {
   public:
	GLBuffer(BufferMode bufferMode, size_t size, uint16_t dataSize);
	GLBuffer(BufferMode bufferMode, void* pData, size_t size,
			 uint16_t dataSize);
	~GLBuffer();

	/** Allocates GPU storage via glBufferData. */
	void initialize() override;

	/** Allocates GPU storage and uploads initial memory block. */
	void initialize(void* pData, size_t size) override;

	/** Streams updated data into sub-region via glBufferSubData. */
	void update(void* pData, size_t size, uint32_t offset) override;

   private:
	uint32_t m_bufferId;
	uint32_t m_bufferTarget;
};

}  // namespace bird::gl

#endif	// BIRD_GLBUFFER_H
