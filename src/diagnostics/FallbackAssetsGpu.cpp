#include "FallbackAssets.h"

#include <cstring>
#include <memory>

#include "../renders/generic/Buffer.h"
#include "../renders/generic/Material.h"
#include "../renders/generic/Mesh.h"
#include "../renders/generic/Texture.h"

namespace bird::diag {

std::shared_ptr<bird::Texture> createFallbackTexture(bool srgb) {
	const FallbackTextureData data = generateCheckerboardTextureData();
	auto* rawPixels = new uint8_t[data.pixels.size()];
	std::memcpy(rawPixels, data.pixels.data(), data.pixels.size());

	bird::TextureBuilder builder(rawPixels, data.width, data.height,
								 bird::FORMAT_8RGBA);
	builder.filterNearest().generateMipMaps();
	if (srgb) {
		builder.srgb();
	}
	return builder.build();
}

std::shared_ptr<bird::Material> createFallbackMaterial(
	const std::shared_ptr<bird::Texture>& fallbackTexture) {
	auto mat = std::make_shared<bird::Material>();
	mat->setDomain(bird::MaterialDomain::Unlit);
	if (fallbackTexture) {
		mat->setBaseColor(bird::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		mat->setTexture(bird::TextureSlot::BaseColor, fallbackTexture);
	} else {
		mat->setBaseColor(bird::Vector4(1.0f, 0.0f, 1.0f, 1.0f));
	}
	return mat;
}

std::shared_ptr<bird::Mesh> createFallbackUnitCubeMesh(
	const std::shared_ptr<bird::Material>& fallbackMaterial) {
	const FallbackMeshData data = generateUnitCubeMeshData();

	auto indices = std::make_unique<uint32_t[]>(data.indices.size());
	std::memcpy(indices.get(), data.indices.data(),
				data.indices.size() * sizeof(uint32_t));

	auto positions = std::make_unique<float[]>(data.positions.size());
	std::memcpy(positions.get(), data.positions.data(),
				data.positions.size() * sizeof(float));

	auto normals = std::make_unique<float[]>(data.normals.size());
	std::memcpy(normals.get(), data.normals.data(),
				data.normals.size() * sizeof(float));

	auto texCoords = std::make_unique<float[]>(data.texCoords.size());
	std::memcpy(texCoords.get(), data.texCoords.data(),
				data.texCoords.size() * sizeof(float));

	bird::MeshBuilder mb;
	mb.setIndexBuffer(
		  bird::BufferBuilder<uint32_t>(bird::BUFFER_STAGED |
										bird::BUFFER_ELEMENT_ARRAY)
			  .data(std::move(indices), data.indices.size())
			  .build())
		.setVertexBuffer(
			bird::BufferBuilder<float>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY)
				.data(std::move(positions), data.positions.size())
				.build())
		.setNormalBuffer(
			bird::BufferBuilder<float>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY)
				.data(std::move(normals), data.normals.size())
				.build())
		.setTextureCoordBuffer(
			bird::BufferBuilder<float, 2>(bird::BUFFER_STAGED |
										  bird::BUFFER_ARRAY)
				.data(std::move(texCoords), data.texCoords.size())
				.build());

	if (fallbackMaterial) {
		mb.setMaterial(fallbackMaterial);
	} else {
		mb.setMaterial(createFallbackMaterial(nullptr));
	}
	return mb.build();
}

}  // namespace bird::diag
