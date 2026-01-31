//
// Created by Seth Hahn on 11/18/22.
//

#ifndef BIRD_RAWBUFFERTEMPLATE_H
#define BIRD_RAWBUFFERTEMPLATE_H

#include <memory>

#include "../GraphicsConstants.h"

namespace bird {

enum BufferMode {
	// equivalent to
	BUFFER_MAPPED = 1,
	BUFFER_STAGED = 2,
	BUFFER_ARRAY = 4,
	BUFFER_ELEMENT_ARRAY = 8,
	BUFFER_TEXTURE = 16,
	BUFFER_UNIFORM = 32

};

class RawBuffer {
   public:
	RawBuffer(BufferMode mode, size_t size);
	RawBuffer(BufferMode mode, void* pData, size_t size);
	virtual ~RawBuffer();

	virtual void initialize() = 0;
	virtual void initialize(void* pData, size_t size) = 0;
	virtual void update(void* pData, size_t size, uint32_t offset) = 0;

	void* m_pData;

	size_t m_size;
	uint32_t m_bindingPoint;
	BufferMode m_bufferMode;
	uint16_t m_dataSize;
};

}  // namespace bird

#endif	// BIRD_RAWBUFFERTEMPLATE_H
