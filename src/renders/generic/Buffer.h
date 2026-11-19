#ifndef BIRD_BUFFERTEMPLATE_H
#define BIRD_BUFFERTEMPLATE_H

#include <cstddef>
#include <memory>
#include <variant>
#include <stdexcept>

#include "../opengl/GLBuffer.h"
#include "RawBuffer.h"

namespace bird {

/**
 * Strongly-typed GPU buffer wrapper.
 *
 * Encapsulates element type T and attribute stride. Owns an underlying
 * backend RawBuffer and optionally a host memory shadow copy.
 *
 * @tparam T Element struct or scalar type.
 * @tparam t_attributeSize Component count per vertex attribute.
 */
template <typename T, uint8_t t_attributeSize = 3>
class Buffer {
   public:
	Buffer(std::unique_ptr<RawBuffer> bufferData);
	Buffer(std::unique_ptr<T[]> m_pData, std::unique_ptr<RawBuffer> bufferData);
	~Buffer();

	/** Allocates buffer storage on the GPU. */
	void initialize() {
		m_rawBuffer->initialize();
	}

	/**
	 * Allocates buffer storage on the GPU and uploads data.
	 * Takes ownership of pData host buffer.
	 */
	void initialize(std::unique_ptr<T[]> pData, size_t size) {
		m_pData = std::move(pData);
		m_rawBuffer->initialize((void*)pData.get(), size);
	}

	/**
	 * Updates a region of GPU buffer data.
	 *
	 * If (offset + size > getBufferSize()), automatically reallocates the
	 * underlying GPU buffer to the new required capacity.
	 *
	 * @param pData Source array of T elements.
	 * @param size Number of T elements to copy.
	 * @param offset Element offset into the GPU buffer.
	 */
	void update(T* pData, size_t size, uint32_t offset) {
		if (size + offset > m_rawBuffer->m_size) {
			m_rawBuffer->initialize((void*)pData, size);
			return;
		}
		m_rawBuffer->update((void*)pData, size, offset);
	}

	const size_t& getBufferSize() const {
		return m_rawBuffer->m_size;
	}
	const BufferMode& getBufferMode() const {
		return m_rawBuffer->m_bufferMode;
	}

	/** Returns attribute component size (e.g. 3 for vec3). */
	uint8_t getAttributeSize() const {
		return t_attributeSize;
	}

	/** Attaches this uniform buffer to its binding point. */
	void bindToBindingPoint() {
		m_rawBuffer->bindToBindingPoint();
	}

	/** Sets uniform or shader storage binding index. */
	void setBindingPoint(uint32_t bindingPoint) {
		m_rawBuffer->m_bindingPoint = bindingPoint;
	}

	std::unique_ptr<T[]> m_pData;
	std::unique_ptr<RawBuffer> m_rawBuffer;
};

template <typename T, uint8_t t_attributeSize>
Buffer<T, t_attributeSize>::Buffer(std::unique_ptr<RawBuffer> bufferData)
	: Buffer(nullptr, std::move(bufferData)) {
	if (m_rawBuffer->m_pData != nullptr)
		m_pData = std::unique_ptr<T[]>((T*)m_rawBuffer->m_pData);

	m_rawBuffer->m_dataSize = sizeof(T);
}

template <typename T, uint8_t t_attributeSize>
Buffer<T, t_attributeSize>::Buffer(std::unique_ptr<T[]> pData,
								   std::unique_ptr<RawBuffer> bufferData)
	: m_rawBuffer(std::move(bufferData)), m_pData(std::move(pData)) {
	m_rawBuffer->m_dataSize = sizeof(T);
}

template <typename T, uint8_t t_attributeSize>
Buffer<T, t_attributeSize>::~Buffer() {}

/**
 * Factory creating a typed GPU buffer for the active graphics backend.
 *
 * @tparam T Buffer element type.
 * @param mode Buffer usage and binding mode flags.
 * @param size Initial capacity in element count.
 */
template <typename T, uint8_t t_attributeSize = 3>
std::unique_ptr<Buffer<T, t_attributeSize>> createBuffer(BufferMode mode,
														 size_t size) {
	std::unique_ptr<RawBuffer> rb = nullptr;

	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			rb = std::make_unique<bird::gl::GLBuffer>(mode, size, sizeof(T));
			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support shaders...");
	}

	return std::make_unique<bird::Buffer<T, t_attributeSize>>(std::move(rb));
}

/**
 * Factory creating a typed GPU buffer initialized with host memory.
 */
template <typename T, uint8_t t_attributeSize = 3>
std::unique_ptr<Buffer<T, t_attributeSize>> createBuffer(
	BufferMode mode, std::unique_ptr<T[]> pData, size_t size) {
	std::unique_ptr<RawBuffer> rb = nullptr;

	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			rb = std::make_unique<bird::gl::GLBuffer>(mode, pData.get(), size,
													  sizeof(T));
			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support shaders...");
	}
	return std::make_unique<bird::Buffer<T, t_attributeSize>>(std::move(pData),
															  std::move(rb));
}

}  // namespace bird

#endif	// BIRD_BUFFERTEMPLATE_H
