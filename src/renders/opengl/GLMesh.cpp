//
// Created by Seth Hahn on 11/30/22.
//

#include "GLMesh.h"

#include "GLApi.h"

namespace bird::gl {

namespace {

/**
 * Uploads a vertex stream and binds the stream to a fixed attribute location
 * when present. Fixed locations let every shader assume the same layout.
 */
template <typename T, uint8_t N>
void bindAttribute(Buffer<T, N>* buffer, GLuint location) {
	if (buffer == nullptr || buffer->getBufferSize() == 0) return;
	buffer->initialize();
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location, N, GL_FLOAT, GL_FALSE, N * sizeof(float),
						  nullptr);
}

}  // namespace

GLMesh::GLMesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
			   std::unique_ptr<Buffer<float>> vertexBuffer,
			   std::unique_ptr<Buffer<float>> normalBuffer,
			   std::unique_ptr<Buffer<float, 2>> textureCoordBuffer,
			   std::unique_ptr<Buffer<float, 4>> tangentBuffer)
	: Mesh(std::move(indexBuffer), std::move(vertexBuffer),
		   std::move(normalBuffer), std::move(textureCoordBuffer),
		   std::move(tangentBuffer)) {
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	if (m_indexBuffer != nullptr && m_indexBuffer->getBufferSize() > 0) {
		m_indexBuffer->initialize();
	}

	bindAttribute(m_vertexBuffer.get(), ATTRIBUTE_POSITION);
	bindAttribute(m_normalBuffer.get(), ATTRIBUTE_NORMAL);
	bindAttribute(m_textureCoordBuffer.get(), ATTRIBUTE_TEXCOORD);
	bindAttribute(m_tangentBuffer.get(), ATTRIBUTE_TANGENT);

	// Absent streams read the default generic attribute (0, 0, 0, 1);
	// shaders detect a zero tangent and derive a frame from derivatives.
	glBindVertexArray(0);
}

GLMesh::~GLMesh() {
	glDeleteVertexArrays(1, &m_vao);
}

void GLMesh::updateMesh() {}

const uint32_t& GLMesh::getVAO() const {
	return m_vao;
}

}  // namespace bird::gl
