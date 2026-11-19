//
// Created by Seth Hahn on 11/30/22.
//

#include "Mesh.h"

#include "../GraphicsPipeline.h"
#include "GLMesh.h"

namespace bird {

Mesh::Mesh(std::unique_ptr<Buffer<uint32_t>> indexBuffer,
		   std::unique_ptr<Buffer<float>> vertexBuffer,
		   std::unique_ptr<Buffer<float>> normalBuffer,
		   std::unique_ptr<Buffer<float, 2>> textureCoordBuffer,
		   std::unique_ptr<Buffer<float, 4>> tangentBuffer)
	: m_indexBuffer(std::move(indexBuffer)),
	  m_vertexBuffer(std::move(vertexBuffer)),
	  m_normalBuffer(std::move(normalBuffer)),
	  m_textureCoordBuffer(std::move(textureCoordBuffer)),
	  m_tangentBuffer(std::move(tangentBuffer)) {
	if (m_vertexBuffer && m_vertexBuffer->m_pData &&
		m_vertexBuffer->getAttributeSize() == 3) {
		m_localBounds = AABB::fromPositions(
			m_vertexBuffer->m_pData.get(), m_vertexBuffer->getBufferSize() / 3);
	}
}

Mesh::~Mesh() {}

void Mesh::read() {}

void Mesh::write() {}

Buffer<uint32_t>* Mesh::getIndexBuffer() {
	return m_indexBuffer.get();
}

Buffer<float>* Mesh::getVertexBuffer() {
	return m_vertexBuffer.get();
}

Buffer<float>* Mesh::getNormalBuffer() {
	return m_normalBuffer.get();
}

Buffer<float, 2>* Mesh::getTextureCoordBuffer() {
	return m_textureCoordBuffer.get();
}

Buffer<float, 4>* Mesh::getTangentBuffer() {
	return m_tangentBuffer.get();
}

const AABB& Mesh::getLocalBounds() const {
	return m_localBounds;
}

void Mesh::setMaterial(const std::shared_ptr<Material>& material) {
	m_material = material;
}

const std::shared_ptr<Material>& Mesh::getMaterial() const {
	return m_material;
}

MeshBuilder::MeshBuilder() {}

MeshBuilder::~MeshBuilder() {}

MeshBuilder& MeshBuilder::setIndexBuffer(
	std::unique_ptr<Buffer<uint32_t>> indexBuffer) {
	m_indexBuffer = std::move(indexBuffer);
	return *this;
}

MeshBuilder& MeshBuilder::setVertexBuffer(
	std::unique_ptr<Buffer<float>> vertexBuffer) {
	m_vertexBuffer = std::move(vertexBuffer);
	return *this;
}

MeshBuilder& MeshBuilder::setNormalBuffer(
	std::unique_ptr<Buffer<float>> normalBuffer) {
	m_normalBuffer = std::move(normalBuffer);
	return *this;
}

MeshBuilder& MeshBuilder::setTextureCoordBuffer(
	std::unique_ptr<Buffer<float, 2>> textureCoordBuffer) {
	m_textureCoordBuffer = std::move(textureCoordBuffer);
	return *this;
}

MeshBuilder& MeshBuilder::setTangentBuffer(
	std::unique_ptr<Buffer<float, 4>> tangentBuffer) {
	m_tangentBuffer = std::move(tangentBuffer);
	return *this;
}

MeshBuilder& MeshBuilder::setMaterial(std::shared_ptr<Material> material) {
	m_material = material;
	return *this;
}

std::shared_ptr<Mesh> MeshBuilder::build() {
	std::shared_ptr<Mesh> ptr = nullptr;

	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			ptr = std::make_shared<gl::GLMesh>(
				std::move(m_indexBuffer), std::move(m_vertexBuffer),
				std::move(m_normalBuffer), std::move(m_textureCoordBuffer),
				std::move(m_tangentBuffer));
			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support shaders...");
	}
	if (m_material != nullptr) ptr->setMaterial(m_material);

	return ptr;
}

}  // namespace bird
