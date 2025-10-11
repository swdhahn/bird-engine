//
// Created by Seth Hahn on 11/18/22.
//

#ifndef BIRD_BUFFERTEMPLATE_H
#define BIRD_BUFFERTEMPLATE_H

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

template <typename T, uint8_t t_attributeSize = 3>
class Buffer {
   public:
	Buffer(BufferMode mode, size_t size);
	Buffer(BufferMode mode, std::unique_ptr<T[]> pData, size_t size);
	virtual ~Buffer();

	virtual void initialize() = 0;
	virtual void initialize(std::unique_ptr<T[]> pData, size_t size) = 0;
	virtual void update(T* pData, size_t size, uint32_t offset) = 0;

	const size_t& getBufferSize() const;
	const BufferMode& getBufferMode() const;
	/**
	 * @return the size of each attribute. The count of objects in
	 * buffer is [getBufferSize() / getAttributeSize();]
	 */
	uint8_t getAttributeSize() const {
		return t_attributeSize;
	}
	void setBindingPoint(uint32_t bindingPoint) {
		m_bindingPoint = bindingPoint;
	}

	std::unique_ptr<T[]> m_pData;

   protected:
	size_t m_size;
	uint32_t m_bindingPoint;
	BufferMode m_bufferMode;
};

template <typename T, uint8_t t_attributeSize>
Buffer<T, t_attributeSize>::Buffer(BufferMode mode, size_t size)
	: Buffer(mode, std::make_unique<T[]>(new T[size]), size) {}
template <typename T, uint8_t t_attributeSize>
Buffer<T, t_attributeSize>::Buffer(BufferMode mode, std::unique_ptr<T[]> pData,
								   size_t size)
	: m_size(size),
	  m_bufferMode(mode),
	  m_pData(std::move(pData)),
	  m_bindingPoint(0) {}
template <typename T, uint8_t t_attributeSize>
Buffer<T, t_attributeSize>::~Buffer() {}

template <typename T, uint8_t t_attributeSize>
const size_t& Buffer<T, t_attributeSize>::getBufferSize() const {
	return m_size;
}

template <typename T, uint8_t t_attributeSize>
const BufferMode& Buffer<T, t_attributeSize>::getBufferMode() const {
	return m_bufferMode;
}

}  // namespace bird

#endif	// BIRD_BUFFERTEMPLATE_H
