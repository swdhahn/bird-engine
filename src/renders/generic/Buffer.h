#ifndef BIRD_BUFFERTEMPLATE_H
#define BIRD_BUFFERTEMPLATE_H

#include "../opengl/GLBuffer.h"
#include "RawBuffer.h"

namespace bird {
template <typename T, uint8_t t_attributeSize = 3>
class Buffer {
   public:
	Buffer(std::unique_ptr<RawBuffer> bufferData);
	Buffer(std::unique_ptr<T[]> m_pData, std::unique_ptr<RawBuffer> bufferData);
	~Buffer();

	void initialize() {
		m_rawBuffer->initialize();
	}

	void initialize(std::unique_ptr<T[]> pData, size_t size) {
		m_pData = std::move(pData);
		m_rawBuffer->initialize((void*)pData.get(), size);
	}

	void update(T* pData, size_t size, uint32_t offset) {
		m_rawBuffer->update((void*)pData, size, offset);
	}

	const size_t& getBufferSize() const {
		return m_rawBuffer->m_size;
	}
	const BufferMode& getBufferMode() const {
		return m_rawBuffer->m_bufferMode;
	}
	/**
	 * @return the size of each attribute. The count of objects in
	 * buffer is [getBufferSize() / getAttributeSize();]
	 */
	uint8_t getAttributeSize() const {
		return t_attributeSize;
	}
	void setBindingPoint(uint32_t bindingPoint) {
		m_rawBuffer->m_bindingPoint = bindingPoint;
	}

	std::unique_ptr<T[]> m_pData;

   protected:
	std::unique_ptr<RawBuffer> m_rawBuffer;
};

template <typename T, uint8_t t_attributeSize>

Buffer<T, t_attributeSize>::Buffer(std::unique_ptr<RawBuffer> bufferData)
	: Buffer(std::unique_ptr<T[]>(bufferData->m_pData), bufferData) {
	m_rawBuffer->m_dataSize = t_attributeSize;
}
template <typename T, uint8_t t_attributeSize>
Buffer<T, t_attributeSize>::Buffer(std::unique_ptr<T[]> pData,
								   std::unique_ptr<RawBuffer> bufferData)
	: m_rawBuffer(std::move(bufferData)), m_pData(std::move(pData)) {
	m_rawBuffer->m_dataSize = t_attributeSize;
}
template <typename T, uint8_t t_attributeSize>
Buffer<T, t_attributeSize>::~Buffer() {}

template <typename T, uint8_t t_attributeSize = 3>
std::unique_ptr<Buffer<T, t_attributeSize>> createBuffer(BufferMode mode,
														 size_t size) {
	std::unique_ptr<Buffer<T, t_attributeSize>> ptr = nullptr;

	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			ptr = std::make_unique<bird::Buffer<T, t_attributeSize>>(
				std::make_unique<bird::gl::GLBuffer>(mode, size));

			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support shaders...");
	}
	return ptr;
}

template <typename T, uint8_t t_attributeSize = 3>
std::unique_ptr<Buffer<T, t_attributeSize>> createBuffer(
	BufferMode mode, std::unique_ptr<T[]> pData, size_t size) {
	std::unique_ptr<Buffer<T, t_attributeSize>> ptr = nullptr;

	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			ptr = std::make_unique<bird::Buffer<T, t_attributeSize>>(
				std::move(pData),
				std::make_unique<bird::gl::GLBuffer>(mode, size));

			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support shaders...");
	}
	return std::move(ptr);
}

}  // namespace bird

#endif	// BIRD_BUFFERTEMPLATE_H
