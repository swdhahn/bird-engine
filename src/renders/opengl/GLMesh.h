//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_GLMESH_H
#define BIRD_GLMESH_H

#include "../generic/Mesh.h"
#include "GLBuffer.h"

namespace bird::gl {

class GLMesh : public Mesh {
   public:
	GLMesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
		   std::unique_ptr<Buffer<float>> vertexBuffer,
		   std::unique_ptr<Buffer<float>> normalBuffer,
		   std::unique_ptr<Buffer<float, 2>> textureCoordBuffer);
	~GLMesh();

	void updateMesh() override;

	const uint32_t& getVAO() const;

   private:
	uint32_t m_vao;
};

}  // namespace bird::gl

#endif	// BIRD_GLMESH_H
