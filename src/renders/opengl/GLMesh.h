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
 * Configures vertex attribute pointers for position (location 0), normal
 * (location 1), and UV coordinates (location 2), backed by GLBuffer handles.
 */
class GLMesh : public Mesh {
   public:
	GLMesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
		   std::unique_ptr<Buffer<float>> vertexBuffer,
		   std::unique_ptr<Buffer<float>> normalBuffer,
		   std::unique_ptr<Buffer<float, 2>> textureCoordBuffer);
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
