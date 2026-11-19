//
// Created by Seth Hahn on 11/18/22.
//

#include "RawBuffer.h"

#include "../opengl/GLBuffer.h"
#include "Buffer.h"
namespace bird {

RawBuffer::RawBuffer(BufferMode mode, size_t size, uint16_t dataSize)
	: RawBuffer(mode, nullptr, size, dataSize) {}
RawBuffer::RawBuffer(BufferMode mode, void* pData, size_t size,
					 uint16_t dataSize)
	: m_pData(pData), m_size(size), m_bufferMode(mode), m_dataSize(dataSize) {}
RawBuffer::~RawBuffer() {}

}  // namespace bird
