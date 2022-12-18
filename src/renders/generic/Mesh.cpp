//
// Created by Seth Hahn on 11/30/22.
//

#include "Mesh.h"

namespace bird {

    Mesh::Mesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer, std::unique_ptr<Buffer<float>> vertexBuffer,
            std::unique_ptr<Buffer<float>> normalBuffer, std::unique_ptr<Buffer<float, 2>> textureCoordBuffer)
            : m_indexBuffer(std::move(indexBuffer)), m_vertexBuffer(std::move(vertexBuffer)),
            m_normalBuffer(std::move(normalBuffer)), m_textureCoordBuffer(std::move(textureCoordBuffer)) {


    }

    Mesh::~Mesh() {}

    const Buffer<uint32_t>* Mesh::getIndexBuffer() const {
        return m_indexBuffer.get();
    }

    const Buffer<float>* Mesh::getVertexBuffer() const {
        return m_vertexBuffer.get();
    }

    const Buffer<float>* Mesh::getNormalBuffer() const {
        return m_normalBuffer.get();
    }

    const Buffer<float, 2>* Mesh::getTextureCoordBuffer() const {
        return m_textureCoordBuffer.get();
    }

}
