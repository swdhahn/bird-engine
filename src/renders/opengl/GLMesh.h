//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_GLMESH_H
#define BIRD_GLMESH_H

#include "../generic/Mesh.h"
#include "GLBuffer.h"

namespace bird::gl {

/**
 * OpenGL Vertex Array Object (VAO) mesh implementation.
 *
 * Binds vertex streams to fixed attribute locations shared by every engine
 * shader: position 0, normal 1, uv 2, tangent 3.
 */
class GLMesh : public Mesh {
   public:
	static constexpr uint32_t ATTRIBUTE_POSITION = 0;
	static constexpr uint32_t ATTRIBUTE_NORMAL = 1;
	static constexpr uint32_t ATTRIBUTE_TEXCOORD = 2;
	static constexpr uint32_t ATTRIBUTE_TANGENT = 3;

	GLMesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
		   std::unique_ptr<Buffer<float>> vertexBuffer,
		   std::unique_ptr<Buffer<float>> normalBuffer,
		   std::unique_ptr<Buffer<float, 2>> textureCoordBuffer,
		   std::unique_ptr<Buffer<float, 4>> tangentBuffer);
	~GLMesh();

	/** Re-uploads and reconfigures attribute pointers on the VAO. */
	void updateMesh() override;

	/** Returns native OpenGL VAO identifier. */
	const uint32_t& getVAO() const;

   private:
	uint32_t m_vao;
};

}  // namespace bird::gl

#endif	// BIRD_GLMESH_H
