//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MESH_H
#define BIRD_MESH_H

#include <array>
#include <cstdint>
#include <memory>

#include "Buffer.h"
#include "Material.h"

namespace bird {

class Mesh : public Resource {
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
	Mesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
		 std::unique_ptr<Buffer<float>> vertexBuffer,
		 std::unique_ptr<Buffer<float>> normalBuffer,
		 std::unique_ptr<Buffer<float, 2>> textureCoordBuffer);
	virtual ~Mesh();

	/**
	 * This reloads the mesh into the graphics library. And does any
	 * needed calculations for the mesh. Call this when you have
	 * changed the mesh data.
	 */
	virtual void updateMesh() = 0;

	void read();
	void write();

	Buffer<uint32_t>* getIndexBuffer();
	Buffer<float>* getVertexBuffer();
	Buffer<float>* getNormalBuffer();
	Buffer<float, 2>* getTextureCoordBuffer();

	void setMaterial(const std::shared_ptr<Material>& material);
	const std::shared_ptr<Material>& getMaterial() const;

   protected:
	std::unique_ptr<Buffer<uint32_t>> m_indexBuffer = nullptr;
	std::unique_ptr<Buffer<float>> m_vertexBuffer = nullptr;
	std::unique_ptr<Buffer<float>> m_normalBuffer = nullptr;
	std::unique_ptr<Buffer<float, 2>> m_textureCoordBuffer = nullptr;

	std::shared_ptr<Material> m_material = nullptr;
	//	std::shared_ptr<ColliderShape> m_colliderShape;
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
	MeshBuilder& setTextureCoordBuffer(
		std::unique_ptr<Buffer<float, 2>> textureCoordBuffer);
	MeshBuilder& setMaterial(std::shared_ptr<Material> material);

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
	std::shared_ptr<Material> m_material = nullptr;
};
}  // namespace bird

#endif	// BIRD_MESH_H
