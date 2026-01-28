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
		   std::unique_ptr<Buffer<float, 2>> textureCoordBuffer)
	: m_indexBuffer(std::move(indexBuffer)),
	  m_vertexBuffer(std::move(vertexBuffer)),
	  m_normalBuffer(std::move(normalBuffer)),
	  m_textureCoordBuffer(std::move(textureCoordBuffer)) {}

Mesh::~Mesh() {}

void Mesh::read() {}

void Mesh::write() {}

const Buffer<uint32_t>* Mesh::getIndexBuffer() const {
	return m_indexBuffer.get();
}

const Buffer<float>* Mesh::getVertexBuffer() const {
	return m_vertexBuffer.get();
}

const Buffer<float>* Mesh::getNormalBuffer() const {
	return m_normalBuffer.get();
}

const Buffer<float, 2>* Mesh::getTextureCoordBuffer() const {
	return m_textureCoordBuffer.get();
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
				std::move(m_normalBuffer), std::move(m_textureCoordBuffer));
			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support shaders...");
	}
	if (m_material != nullptr) ptr->setMaterial(m_material);

	return ptr;
}

}  // namespace bird
