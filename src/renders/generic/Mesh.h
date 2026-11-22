//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MESH_H
#define BIRD_MESH_H

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "Bounds.h"
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
	 * @param tangentBuffer - xyz tangent plus w handedness, or nullptr
	 */
	Mesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
		 std::unique_ptr<Buffer<float>> vertexBuffer,
		 std::unique_ptr<Buffer<float>> normalBuffer,
		 std::unique_ptr<Buffer<float, 2>> textureCoordBuffer,
		 std::unique_ptr<Buffer<float, 4>> tangentBuffer = nullptr);
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
	Buffer<float, 4>* getTangentBuffer();

	/**
	 * Attaches skinning influences: four joint indices and four weights
	 * per vertex. A skinned mesh is drawn with the skinned shader variants
	 * when its object has a SkeletalAnimationComponent, and in its bind
	 * pose otherwise.
	 *
	 * @param joints Joint indices, 4 per vertex.
	 * @param weights Normalized weights, 4 per vertex.
	 * @throws std::invalid_argument when either buffer is null or the
	 *         element counts do not match the vertex count.
	 */
	void setSkin(std::unique_ptr<Buffer<uint16_t, 4>> joints,
				 std::unique_ptr<Buffer<float, 4>> weights);
	/** True once setSkin() succeeded. */
	bool isSkinned() const { return m_jointBuffer != nullptr; }
	Buffer<uint16_t, 4>* getJointBuffer() { return m_jointBuffer.get(); }
	/**
	 * Per-joint influence radii for animated culling bounds (see
	 * bird::skinnedBounds); empty for rigid meshes.
	 */
	void setJointRadii(std::vector<float> radii) { m_jointRadii = std::move(radii); }
	const std::vector<float>& getJointRadii() const { return m_jointRadii; }
	Buffer<float, 4>* getWeightBuffer() { return m_weightBuffer.get(); }

	/** Object-space bounds computed from the vertex positions at creation. */
	const AABB& getLocalBounds() const;

	void setMaterial(const std::shared_ptr<Material>& material);
	const std::shared_ptr<Material>& getMaterial() const;

   protected:
	std::unique_ptr<Buffer<uint32_t>> m_indexBuffer = nullptr;
	std::unique_ptr<Buffer<float>> m_vertexBuffer = nullptr;
	std::unique_ptr<Buffer<float>> m_normalBuffer = nullptr;
	std::unique_ptr<Buffer<float, 2>> m_textureCoordBuffer = nullptr;
	std::unique_ptr<Buffer<float, 4>> m_tangentBuffer = nullptr;
	std::unique_ptr<Buffer<uint16_t, 4>> m_jointBuffer = nullptr;
	std::unique_ptr<Buffer<float, 4>> m_weightBuffer = nullptr;
	std::vector<float> m_jointRadii;

	std::shared_ptr<Material> m_material = nullptr;
	AABB m_localBounds;

	/** Backend hook: expose the new skin streams to the vertex stage. */
	virtual void onSkinAttached() {}
};

/**
 * Fluent builder assembling a Mesh from vertex streams built in code.
 * Model files on disk load through ResourceManager::loadMesh instead.
 */
class MeshBuilder {
   public:
	MeshBuilder();
	~MeshBuilder();

	MeshBuilder& setIndexBuffer(std::unique_ptr<Buffer<uint32_t>> indexBuffer);
	MeshBuilder& setVertexBuffer(std::unique_ptr<Buffer<float>> vertexBuffer);
	MeshBuilder& setNormalBuffer(std::unique_ptr<Buffer<float>> normalBuffer);
	MeshBuilder& setTextureCoordBuffer(
		std::unique_ptr<Buffer<float, 2>> textureCoordBuffer);
	MeshBuilder& setTangentBuffer(std::unique_ptr<Buffer<float, 4>> tangentBuffer);
	/** Skinning streams; see Mesh::setSkin for the contract. */
	MeshBuilder& setSkinBuffers(std::unique_ptr<Buffer<uint16_t, 4>> joints,
								std::unique_ptr<Buffer<float, 4>> weights);
	MeshBuilder& setMaterial(std::shared_ptr<Material> material);

	/**
	 * Creates the mesh for the active backend and uploads its streams.
	 * @return The new mesh.
	 * @throws std::runtime_error when the backend lacks mesh support.
	 */
	std::shared_ptr<Mesh> build();

   private:
	std::unique_ptr<Buffer<uint32_t>> m_indexBuffer = nullptr;
	std::unique_ptr<Buffer<float>> m_vertexBuffer = nullptr;
	std::unique_ptr<Buffer<float>> m_normalBuffer = nullptr;
	std::unique_ptr<Buffer<float, 2>> m_textureCoordBuffer = nullptr;
	std::unique_ptr<Buffer<float, 4>> m_tangentBuffer = nullptr;
	std::unique_ptr<Buffer<uint16_t, 4>> m_jointBuffer = nullptr;
	std::unique_ptr<Buffer<float, 4>> m_weightBuffer = nullptr;
	std::shared_ptr<Material> m_material = nullptr;
};
}  // namespace bird

#endif	// BIRD_MESH_H
