//
// Created by Seth Hahn on 11/18/22.
//

#ifndef BIRD_RAWBUFFERTEMPLATE_H
#define BIRD_RAWBUFFERTEMPLATE_H

#include <memory>
#include <cstdint>
#include <cstddef>

#include "../GraphicsConstants.h"

namespace bird {

/**
 * GPU buffer target and memory usage flags.
 */
enum BufferMode {
	BUFFER_MAPPED = 1,
	BUFFER_STAGED = 2,
	BUFFER_ARRAY = 4,
	BUFFER_ELEMENT_ARRAY = 8,
	BUFFER_TEXTURE = 16,
	BUFFER_UNIFORM = 32,
	BUFFER_DYNAMIC = 64
};

/**
 * Low-level, non-templated GPU buffer interface.
 *
 * Implemented by concrete backend classes (e.g. GLBuffer) to handle native
 * GPU memory allocation, binding points, and sub-data streaming.
 */
class RawBuffer {
   public:
	RawBuffer(BufferMode mode, size_t size, uint16_t dataSize);
	RawBuffer(BufferMode mode, void* pData, size_t size, uint16_t dataSize);
	virtual ~RawBuffer();

	/** Allocates GPU storage matching m_size. */
	virtual void initialize() = 0;

	/** Allocates GPU storage and uploads initial memory block. */
	virtual void initialize(void* pData, size_t size) = 0;

	/**
	 * Streams updated data into a sub-region of the GPU buffer.
	 * @param pData Pointer to source bytes.
	 * @param size Number of elements to update.
	 * @param offset Element offset into the buffer.
	 */
	virtual void update(void* pData, size_t size, uint32_t offset) = 0;

	/**
	 * Attaches a uniform buffer to its binding point, so that several
	 * buffers may share one binding and be switched between draws.
	 */
	virtual void bindToBindingPoint() = 0;

	void* m_pData;

	size_t m_size;
	uint32_t m_bindingPoint;
	BufferMode m_bufferMode;
	uint16_t m_dataSize;
};

}  // namespace bird

#endif	// BIRD_RAWBUFFERTEMPLATE_H
