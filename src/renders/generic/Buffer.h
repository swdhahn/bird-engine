#ifndef BIRD_BUFFERTEMPLATE_H
#define BIRD_BUFFERTEMPLATE_H

#include <cstddef>
#include <memory>
#include <optional>
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
		m_rawBuffer->initialize((void*)m_pData.get(), size);
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
 * Fluent builder for typed GPU buffers.
 *
 * Upload timing depends on the buffer's role:
 * - Uniform buffers (BUFFER_UNIFORM) upload during build() and receive their
 *   binding point, so the result is immediately usable.
 * - Vertex and index streams stay CPU-side until the Mesh receiving them
 *   uploads them, because attribute setup requires the mesh's vertex array
 *   object to be bound.
 *
 * @tparam T Buffer element type.
 * @tparam t_attributeSize Component count per vertex attribute.
 */
template <typename T, uint8_t t_attributeSize = 3>
class BufferBuilder {
   public:
	/**
	 * @param mode BufferMode flags, for example BUFFER_ARRAY | BUFFER_STAGED.
	 */
	explicit BufferBuilder(uint32_t mode)
		: m_mode(static_cast<BufferMode>(mode)) {}

	/**
	 * Supplies initial contents; ownership passes to the buffer.
	 * @param pData Host elements.
	 * @param count Number of T elements in pData.
	 */
	BufferBuilder& data(std::unique_ptr<T[]> pData, size_t count) {
		m_pData = std::move(pData);
		m_count = count;
		return *this;
	}

	/**
	 * Reserves storage without initial contents.
	 * @param count Number of T elements.
	 */
	BufferBuilder& capacity(size_t count) {
		m_pData.reset();
		m_count = count;
		return *this;
	}

	/**
	 * Sets the uniform block binding index.
	 * @param bindingPoint Index matching the shader's binding qualifier.
	 * @throws std::logic_error when the mode lacks BUFFER_UNIFORM.
	 */
	BufferBuilder& bindingPoint(uint32_t bindingPoint) {
		if ((m_mode & BUFFER_UNIFORM) == 0) {
			throw std::logic_error(
				"BufferBuilder: a binding point requires BUFFER_UNIFORM.");
		}
		m_bindingPoint = bindingPoint;
		return *this;
	}

	/**
	 * Creates the buffer for the active backend, consuming the staged data.
	 * @return The buffer; uploaded already when it is a uniform buffer.
	 * @throws std::runtime_error when the backend lacks buffer support.
	 */
	std::unique_ptr<Buffer<T, t_attributeSize>> build() {
		std::unique_ptr<RawBuffer> raw;
		switch (CURRENT_GRAPHICS_PIPELINE) {
			case GRAPHICS_PIPELINE_OPENGL:
				raw = m_pData ? std::make_unique<gl::GLBuffer>(
									m_mode, m_pData.get(), m_count, sizeof(T))
							  : std::make_unique<gl::GLBuffer>(m_mode, m_count,
															   sizeof(T));
				break;
			default:
				throw std::runtime_error(
					"BufferBuilder: the active graphics pipeline lacks buffer "
					"support.");
		}
		auto buffer =
			m_pData
				? std::make_unique<Buffer<T, t_attributeSize>>(
					  std::move(m_pData), std::move(raw))
				: std::make_unique<Buffer<T, t_attributeSize>>(std::move(raw));
		if ((m_mode & BUFFER_UNIFORM) != 0) {
			if (m_bindingPoint) buffer->setBindingPoint(*m_bindingPoint);
			buffer->initialize();
		}
		return buffer;
	}

   private:
	BufferMode m_mode;
	std::unique_ptr<T[]> m_pData;
	size_t m_count = 0;
	std::optional<uint32_t> m_bindingPoint;
};

}  // namespace bird

#endif	// BIRD_BUFFERTEMPLATE_H
