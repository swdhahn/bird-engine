//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MESH_H
#define BIRD_MESH_H

#include <cstdint>
#include <array>
#include "Buffer.h"

namespace bird {

    class Mesh {
    public:
        /**
         * This creates a mesh object. DO NOT CALL THIS DIRECTLY.
         * Use MeshBuilder instead UNLESS you handle the mesh loading
         * for all graphics libraries.
         * @param indexBuffer - the index buffer
         * @param vertexBuffer - the vertex buffer
         * @param normalBuffer - the normal buffer
         * @param textureCoordBuffer - the texture coordinate buffer
         */
        Mesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer, std::unique_ptr<Buffer<float>> vertexBuffer,
             std::unique_ptr<Buffer<float>> normalBuffer, std::unique_ptr<Buffer<float, 2>> textureCoordBuffer);
        virtual ~Mesh();

        /**
         * This reloads the mesh into the graphics library. And does any
         * needed calculations for the mesh. Call this when you have
         * changed the mesh data.
         */
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

    class MeshBuilder {
    public:
        /**
         * This creates the mesh object.
         * @return - You can call build() when
         * you are ready to create the mesh.
         */
        MeshBuilder();
        ~MeshBuilder();

        MeshBuilder& setIndexBuffer(std::unique_ptr<Buffer<uint32_t>> indexBuffer);
        MeshBuilder& setVertexBuffer(std::unique_ptr<Buffer<float>> vertexBuffer);
        MeshBuilder& setNormalBuffer(std::unique_ptr<Buffer<float>> normalBuffer);
        MeshBuilder& setTextureCoordBuffer(std::unique_ptr<Buffer<float, 2>> textureCoordBuffer);

        /**
         * This sets all buffers. If the buffers have already been set,
         * they will be destroyed and replaced with the new buffers.
         * @param path - path to the file
         * @return - the MeshBuilder object, then you can call build()
         * to create the mesh.
         */
        MeshBuilder& loadOBJ(const char* path);
        /**
         * This sets all buffers. If the buffers have already been set,
         * they will be destroyed and replaced with the new buffers.
         * @param path - path to the file
         * @return - the MeshBuilder object, then you can call build()
         * to create the mesh.
         */
        MeshBuilder& loadGLTF(const char* path);

        /**
         * This creates the mesh object and returns it.
         * @return - the new Mesh object
         */
        std::shared_ptr<Mesh> build();

    private:
        std::unique_ptr<Buffer<uint32_t>> m_indexBuffer = nullptr;
        std::unique_ptr<Buffer<float>> m_vertexBuffer = nullptr;
        std::unique_ptr<Buffer<float>> m_normalBuffer = nullptr;
        std::unique_ptr<Buffer<float, 2>> m_textureCoordBuffer = nullptr;



    };
}

#endif //BIRD_MESH_H
