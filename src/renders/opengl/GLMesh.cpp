//
// Created by Seth Hahn on 11/30/22.
//

#include "GLMesh.h"


namespace bird::gl {

GLMesh::GLMesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
			   std::unique_ptr<Buffer<float>> vertexBuffer,
			   std::unique_ptr<Buffer<float>> normalBuffer,
			   std::unique_ptr<Buffer<float, 2>> textureCoordBuffer)
	: Mesh(std::move(indexBuffer), std::move(vertexBuffer),
		   std::move(normalBuffer), std::move(textureCoordBuffer)) {
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	m_indexBuffer->initialize();

	glEnableVertexAttribArray(0);

	m_vertexBuffer->initialize();
	glVertexAttribPointer(
		0, m_vertexBuffer->getAttributeSize(), GL_FLOAT, GL_FALSE,
		m_vertexBuffer->getAttributeSize() * sizeof(float), (void*)0);
	if (m_normalBuffer->getBufferSize() > 0) {
		glEnableVertexAttribArray(1);
		m_normalBuffer->initialize();
		glVertexAttribPointer(
			1, m_normalBuffer->getAttributeSize(), GL_FLOAT, GL_FALSE,
			m_normalBuffer->getAttributeSize() * sizeof(float), (void*)0);
	}
	if (m_textureCoordBuffer->getBufferSize() > 0) {
		glEnableVertexAttribArray(2);
		m_textureCoordBuffer->initialize();
		glVertexAttribPointer(
			2, m_textureCoordBuffer->getAttributeSize(), GL_FLOAT, GL_FALSE,
			m_textureCoordBuffer->getAttributeSize() * sizeof(float), (void*)0);
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
