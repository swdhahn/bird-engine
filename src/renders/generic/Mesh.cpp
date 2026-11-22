//
// Created by Seth Hahn on 11/30/22.
//

#include "Mesh.h"

#include <stdexcept>

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
	recomputeBounds();
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

void Mesh::setSkin(std::unique_ptr<Buffer<uint16_t, 4>> joints,
				   std::unique_ptr<Buffer<float, 4>> weights) {
	if (!joints || !weights) {
		throw std::invalid_argument("Mesh::setSkin: skin buffers are required");
	}
	const size_t vertexCount =
		m_vertexBuffer ? m_vertexBuffer->getBufferSize() / 3 : 0;
	if (joints->getBufferSize() != vertexCount * 4 ||
		weights->getBufferSize() != vertexCount * 4) {
		throw std::invalid_argument(
			"Mesh::setSkin: skin streams must hold 4 values per vertex");
	}
	m_jointBuffer = std::move(joints);
	m_weightBuffer = std::move(weights);
	onSkinAttached();
}

const AABB& Mesh::getLocalBounds() const {
	return m_localBounds;
}

const BoundingSphere& Mesh::getLocalBoundingSphere() const {
	return m_localSphere;
}

void Mesh::recomputeBounds() {
	if (m_vertexBuffer && m_vertexBuffer->m_pData &&
		m_vertexBuffer->getAttributeSize() == 3) {
		const float* positions = m_vertexBuffer->m_pData.get();
		const size_t vertexCount = m_vertexBuffer->getBufferSize() / 3;
		m_localBounds = AABB::fromPositions(positions, vertexCount);
		m_localSphere = BoundingSphere::fromPositions(positions, vertexCount);
	} else {
		m_localBounds = AABB{};
		m_localSphere = BoundingSphere{};
	}
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

MeshBuilder& MeshBuilder::setSkinBuffers(
	std::unique_ptr<Buffer<uint16_t, 4>> joints,
	std::unique_ptr<Buffer<float, 4>> weights) {
	m_jointBuffer = std::move(joints);
	m_weightBuffer = std::move(weights);
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
	if (m_jointBuffer || m_weightBuffer) {
		ptr->setSkin(std::move(m_jointBuffer), std::move(m_weightBuffer));
	}

	return ptr;
}

}  // namespace bird
