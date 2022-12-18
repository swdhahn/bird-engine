//
// Created by Seth Hahn on 11/18/22.
//

#ifndef BIRD_GLBUFFER_H
#define BIRD_GLBUFFER_H

#include <typeindex>
#include "../generic/Buffer.h"

namespace bird::gl {

    template <typename T, uint8_t t_attributeSize = 3> class GLBuffer : public Buffer<T, t_attributeSize> {
    public:
        GLBuffer(BufferMode bufferMode, size_t size);
        GLBuffer(BufferMode bufferMode, std::unique_ptr<T[]> pData, size_t size);
        ~GLBuffer();

        void initialize() override;
        void update(T* pData, size_t size, uint32_t offset) override;

        const uint32_t& getGLBasicBufferType() const;
    private:
        uint32_t m_bufferId;
        uint32_t m_bufferTarget;
        uint32_t m_glBasicBufferType;

    };
    template <typename T, uint8_t t_attributeSize> GLBuffer<T, t_attributeSize>::GLBuffer(BufferMode bufferMode, size_t size)
            : GLBuffer(bufferMode, nullptr, size) {}
    template <typename T, uint8_t t_attributeSize> GLBuffer<T, t_attributeSize>::GLBuffer(BufferMode bufferMode, std::unique_ptr<T[]> pData, size_t size)
            : Buffer<T, t_attributeSize>(bufferMode, std::move(pData), size), m_bufferId(0) {
        glGenBuffers(1, &m_bufferId);

        if(typeid(T) == typeid(int8_t)) {
            m_glBasicBufferType = GL_BYTE;
        } else if(typeid(T) == typeid(uint8_t)) {
            m_glBasicBufferType = GL_UNSIGNED_BYTE;
        } else if(typeid(T) == typeid(int16_t)) {
            m_glBasicBufferType = GL_SHORT;
        } else if(typeid(T) == typeid(uint16_t)) {
            m_glBasicBufferType = GL_UNSIGNED_SHORT;
        } else if(typeid(T) == typeid(int32_t)) {
            m_glBasicBufferType = GL_INT;
        } else if(typeid(T) == typeid(uint32_t)) {
            m_glBasicBufferType = GL_UNSIGNED_INT;
        } else if(typeid(T) == typeid(float)) {
            m_glBasicBufferType = GL_FLOAT;
        } else if(typeid(T) == typeid(double)) {
            m_glBasicBufferType = GL_DOUBLE;
        } else {
            std::cout << "You did not choose an OpenGL Buffer type! Functionality is limited, I hope you know what you're doing!" << std::endl;
        }


        if(bufferMode & BUFFER_ARRAY) {
            m_bufferTarget = GL_ARRAY_BUFFER;
        } else if(bufferMode & BUFFER_ELEMENT_ARRAY) {
            m_bufferTarget = GL_ELEMENT_ARRAY_BUFFER;
        } else if(bufferMode & BUFFER_TEXTURE) {
            m_bufferTarget = BUFFER_TEXTURE;
        } else if(bufferMode & BUFFER_UNIFORM) {
            m_bufferTarget = BUFFER_UNIFORM;
        } else {
            throw std::invalid_argument("You must declare buffer's target in bufferMode parameter: BUFFER_ARRAY, etc..");
        }
        if(bufferMode & BUFFER_MAPPED) {
            //Buffer<T, t_attributeSize>::m_pData = glMapBuffer(m_bufferTarget, GL_WRITE_ONLY);
        } else if(!(bufferMode & BUFFER_STAGED | bufferMode & BUFFER_MAPPED)) {
            throw std::invalid_argument("You must declare buffer as BUFFER_MAPPED or BUFFER_STAGED in bufferMode parameter.");
        }
    }

    template <typename T, uint8_t t_attributeSize> GLBuffer<T, t_attributeSize>::~GLBuffer() {
        glDeleteBuffers(1, &m_bufferId);
    }

    template <typename T, uint8_t t_attributeSize> void GLBuffer<T, t_attributeSize>::initialize() {
        glBindBuffer(m_bufferTarget, m_bufferId);
        glBufferData(m_bufferTarget, Buffer<T, t_attributeSize>::m_size * sizeof(T), Buffer<T, t_attributeSize>::m_pData.get(), GL_STATIC_DRAW);
    }

    template <typename T, uint8_t t_attributeSize> void GLBuffer<T, t_attributeSize>::update(T* pData, size_t size, uint32_t offset) {
        glBindBuffer(m_bufferTarget, m_bufferId);

        if(Buffer<T, t_attributeSize>::m_bufferMode & BUFFER_STAGED) {
            glBufferSubData(m_bufferTarget, offset, size * sizeof(T), pData);
        } else if(Buffer<T, t_attributeSize>::m_bufferMode & BUFFER_MAPPED) {
            //memcpy(Buffer<T, t_attributeSize>::m_pData, pData + offset, size * sizeof(T));
        }

    }

    template <typename T, uint8_t t_attributeSize> const uint32_t& GLBuffer<T, t_attributeSize>::getGLBasicBufferType() const {
        return m_glBasicBufferType;
    }

} // bird

#endif //BIRD_GLBUFFER_H
