//
// Created by Seth Hahn on 11/30/22.
//

#include "GLMesh.h"

#include <GL/glew.h>

namespace bird::gl {

GLMesh::GLMesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
			   std::unique_ptr<Buffer<float>> vertexBuffer,
			   std::unique_ptr<Buffer<float>> normalBuffer,
			   std::unique_ptr<Buffer<float, 2>> textureCoordBuffer)
	: Mesh(std::move(indexBuffer), std::move(vertexBuffer),
		   std::move(normalBuffer), std::move(textureCoordBuffer)) {
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	if (m_indexBuffer != nullptr && m_indexBuffer->getBufferSize() > 0) {
		m_indexBuffer->initialize();
	}

	glEnableVertexAttribArray(0);

	m_vertexBuffer->initialize();
	glVertexAttribPointer(
		0, m_vertexBuffer->getAttributeSize(), GL_FLOAT, GL_FALSE,
		m_vertexBuffer->getAttributeSize() * sizeof(float), (void*)0);
	int nextIndex = 1;
	if (m_normalBuffer != nullptr && m_normalBuffer->getBufferSize() > 0) {
		glEnableVertexAttribArray(nextIndex);
		m_normalBuffer->initialize();
		glVertexAttribPointer(
			nextIndex, m_normalBuffer->getAttributeSize(), GL_FLOAT, GL_FALSE,
			m_normalBuffer->getAttributeSize() * sizeof(float), (void*)0);
		nextIndex++;
	}
	if (m_textureCoordBuffer != nullptr &&
		m_textureCoordBuffer->getBufferSize() > 0) {
		glEnableVertexAttribArray(nextIndex);
		m_textureCoordBuffer->initialize();
		glVertexAttribPointer(
			nextIndex, m_textureCoordBuffer->getAttributeSize(), GL_FLOAT,
			GL_FALSE, m_textureCoordBuffer->getAttributeSize() * sizeof(float),
			(void*)0);
		nextIndex++;
	}
}

GLMesh::~GLMesh() {
	glDeleteVertexArrays(1, &m_vao);
}

void GLMesh::updateMesh() {}

const uint32_t& GLMesh::getVAO() const {
	return m_vao;
}

}  // namespace bird::gl
