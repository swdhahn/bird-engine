//
// Created by Seth Hahn on 11/18/22.
//

#include "RawBuffer.h"

#include "../opengl/GLBuffer.h"
#include "Buffer.h"
namespace bird {

RawBuffer::RawBuffer(BufferMode mode, void* pData, size_t size)
	: m_pData(pData), m_size(size), m_bufferMode(mode) {}
RawBuffer::~RawBuffer() {}

}  // namespace bird
