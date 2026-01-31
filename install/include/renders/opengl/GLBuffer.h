//
// Created by Seth Hahn on 11/18/22.
//

#ifndef BIRD_GLBUFFER_H
#define BIRD_GLBUFFER_H

#include <typeindex>

#include "../generic/RawBuffer.h"

namespace bird::gl {

class GLBuffer : public RawBuffer {
   public:
	GLBuffer(BufferMode bufferMode, size_t size);
	GLBuffer(BufferMode bufferMode, void* pData, size_t size);
	~GLBuffer();

	void initialize() override;
	void initialize(void* pData, size_t size) override;
	void update(void* pData, size_t size, uint32_t offset) override;

   private:
	uint32_t m_bufferId;
	uint32_t m_bufferTarget;
};

}  // namespace bird::gl

#endif	// BIRD_GLBUFFER_H
