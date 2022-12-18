//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MESH_H
#define BIRD_MESH_H

#include <cstdint>
#include "Buffer.h"

namespace bird {

    class Mesh {
    public:
        Mesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer, std::unique_ptr<Buffer<float>> vertexBuffer,
             std::unique_ptr<Buffer<float>> normalBuffer, std::unique_ptr<Buffer<float, 2>> textureCoordBuffer);
        virtual ~Mesh();

        virtual void updateMesh() = 0;

        const Buffer<uint32_t>* getIndexBuffer() const;
        const Buffer<float>* getVertexBuffer() const;
        const Buffer<float>* getNormalBuffer() const;
        const Buffer<float, 2>* getTextureCoordBuffer() const;

    protected:
        std::unique_ptr<Buffer<uint32_t>> m_indexBuffer = nullptr;
        std::unique_ptr<Buffer<float>> m_vertexBuffer = nullptr;
        std::unique_ptr<Buffer<float>> m_normalBuffer = nullptr;
        std::unique_ptr<Buffer<float, 2>> m_textureCoordBuffer = nullptr;

    };

}

#endif //BIRD_MESH_H
