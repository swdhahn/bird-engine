//
// Created by student on 3/8/24.
//

#include "ResourceManager.h"

#include "../animation/skeletal/PoseOps.h"

namespace bird {

ResourceManager::ResourceManager() {
	m_resources = new std::unordered_map<
		std::string, std::shared_ptr<Resource>>[ASSET_TYPE_CUSTOM + 1];
}

ResourceManager::~ResourceManager() {
	delete[] m_resources;
}

void ResourceManager::init() {
	m_defaultShader = loadShader("assets/shaders/default.vert",
								 "assets/shaders/default.frag");
}

std::shared_ptr<Texture> ResourceManager::loadTexture(const std::string& path) {
	return loadTextureWithColorSpace(path, false);
}

std::shared_ptr<Texture> ResourceManager::loadColorTexture(
	const std::string& path) {
	return loadTextureWithColorSpace(path, true);
}

std::shared_ptr<Texture> ResourceManager::loadTextureWithColorSpace(
	const std::string& path, bool srgb) {
	const std::string cacheKey = srgb ? path + "#srgb" : path;
	auto& textures = m_resources[ASSET_TYPE_TEXTURE];
	if (textures.find(cacheKey) != textures.end()) {
		return std::static_pointer_cast<Texture>(textures.at(cacheKey));
	}

	std::fstream file(RESOURCE_PATH + path + ".bird",
					  std::ios::in | std::ios::binary);

	if (!file.is_open()) {
		std::cout << "Error loading file. File does not exist: " << path
				  << std::endl;
		std::cerr << std::strerror(errno) << std::endl;
		file.close();
		return std::shared_ptr<Texture>(0);
	}

	int32_t width = 0, height = 0, channels = 0;

	file.read(reinterpret_cast<char*>(&width), sizeof(int32_t));
	file.read(reinterpret_cast<char*>(&height), sizeof(int32_t));
	file.read(reinterpret_cast<char*>(&channels), sizeof(int32_t));
	uint8_t* data = new uint8_t[width * height * channels];
	file.read(reinterpret_cast<char*>(data),
			  width * height * channels * sizeof(uint8_t));
	file.close();

	if (channels == 0) {
		std::cout << "Error loading file: " << path << std::endl;
		std::cout << "There should be more than zero channels" << std::endl;
		return std::shared_ptr<Texture>(0);
	}
	TextureFormat form = (TextureFormat)(1 << (channels));
	// std::cout << "Img: " << width << " " << height << " " << channels << " "
	//		  << form << std::endl;

	TextureBuilder builder(data, width, height, form);
	builder.generateMipMaps();
	if (srgb) builder.srgb();
	std::shared_ptr<Texture> tex = builder.build();
	addResource(cacheKey, tex, ASSET_TYPE_TEXTURE);
	return tex;
}

std::shared_ptr<Font> ResourceManager::loadFont(const std::string& path) {
	auto& fonts = m_resources[ASSET_TYPE_FONT];
	if (fonts.find(path) != fonts.end()) {
		return std::static_pointer_cast<Font>(fonts.at(path));
	}

	std::fstream file(RESOURCE_PATH + path + ".bird",
					  std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Error loading font. File does not exist: " << path
				  << ".bird (run BuildTools)" << std::endl;
		return nullptr;
	}

	format::FontHeader header{};
	file.read(reinterpret_cast<char*>(&header), sizeof(header));
	std::vector<format::FontGlyph> glyphs(header.glyphCount);
	file.read(reinterpret_cast<char*>(glyphs.data()),
			  glyphs.size() * sizeof(format::FontGlyph));
	const size_t texelCount =
		static_cast<size_t>(header.atlasWidth) * header.atlasHeight;
	std::vector<uint8_t> distance(texelCount);
	file.read(reinterpret_cast<char*>(distance.data()), distance.size());
	if (!file) {
		throw std::runtime_error("Truncated font file: " + path + ".bird");
	}
	file.close();

	// Expand the distance field into the alpha of white RGBA texels; the UI
	// shader converts distance to coverage for glyphs and solid quads alike.
	uint8_t* rgba = new uint8_t[texelCount * 4];
	for (size_t i = 0; i < texelCount; i++) {
		rgba[i * 4 + 0] = 0xFF;
		rgba[i * 4 + 1] = 0xFF;
		rgba[i * 4 + 2] = 0xFF;
		rgba[i * 4 + 3] = distance[i];
	}
	std::shared_ptr<Texture> atlas =
		TextureBuilder(rgba, header.atlasWidth, header.atlasHeight,
					   FORMAT_8RGBA)
			.generateMipMaps()
			.build();

	auto font = std::make_shared<Font>(header, std::move(glyphs), atlas);
	addResource(path, font, ASSET_TYPE_FONT);
	return font;
}

namespace {

/**
 * Moves one mesh's skin stream (if the bake has one) into GPU buffers.
 * @return Per-joint influence radii for culling, or empty when rigid.
 */
std::vector<float> attachSkin(MeshBuilder& builder, const SkeletalAsset& asset,
							  uint32_t meshIndex, uint32_t vertexCount,
							  const float* positions, const std::string& path) {
	for (const SkinStream& skin : asset.skins) {
		if (skin.meshIndex != meshIndex) continue;
		if (skin.joints.size() != vertexCount) {
			throw std::runtime_error("Skin of mesh " + std::to_string(meshIndex) +
									 " in " + path +
									 " does not match its vertex count; "
									 "re-run BuildTools");
		}
		std::vector<float> radii =
			computeJointRadii(*asset.skeleton, skin, positions, vertexCount);
		const size_t count = static_cast<size_t>(vertexCount) * 4;
		auto joints = std::make_unique<uint16_t[]>(count);
		auto weights = std::make_unique<float[]>(count);
		for (size_t v = 0; v < vertexCount; ++v) {
			for (size_t k = 0; k < 4; ++k) {
				joints[v * 4 + k] = skin.joints[v][k];
				weights[v * 4 + k] = skin.weights[v][k];
			}
		}
		builder.setSkinBuffers(
			BufferBuilder<uint16_t, 4>(BUFFER_STAGED | BUFFER_ARRAY)
				.data(std::move(joints), count)
				.build(),
			BufferBuilder<float, 4>(BUFFER_STAGED | BUFFER_ARRAY)
				.data(std::move(weights), count)
				.build());
		return radii;
	}
	return {};
}

}  // namespace

std::shared_ptr<const SkeletalAsset> ResourceManager::loadSkeletalAsset(
	const std::string& path) {
	const auto cached = m_skeletalAssets.find(path);
	if (cached != m_skeletalAssets.end()) return cached->second;
	const std::string baked = RESOURCE_PATH + path + ".skel.bird";
	if (!std::filesystem::exists(baked)) return nullptr;
	auto asset = std::make_shared<const SkeletalAsset>(
		bird::loadSkeletalAsset(baked));
	m_skeletalAssets.emplace(path, asset);
	return asset;
}

std::vector<std::shared_ptr<Mesh>> ResourceManager::loadMesh(
	const std::string& path) {
	if (m_meshes.find(path) != m_meshes.end()) {
		return m_meshes.at(path);
	}
	// load mesh from file
	std::fstream file(RESOURCE_PATH + path + ".bird",
					  std::ios::in | std::ios::binary);

	if (!file.is_open()) {
		std::cout << "Error loading file: " << path << std::endl;
		file.close();
		return std::vector<std::shared_ptr<Mesh>>(0);
	}

	uint16_t meshCount = 0;
	file.read(reinterpret_cast<char*>(&meshCount), sizeof(uint16_t));
	// Skinned models carry their influences beside the skeleton bake.
	const std::shared_ptr<const SkeletalAsset> skeletal =
		loadSkeletalAsset(path);
	std::vector<std::shared_ptr<Mesh>> meshes(meshCount);
	for (int i = 0; i < meshCount; i++) {
		std::unique_ptr<uint32_t[]> indices = nullptr;
		uint32_t indexCount = 0;
		std::unique_ptr<float[]> vertices = nullptr;
		uint32_t vertexCount = 0;
		std::unique_ptr<float[]> normals = nullptr;
		uint32_t normalCount = 0;
		std::unique_ptr<float[]> textureCoords = nullptr;
		uint32_t textureCoordCount = 0;

		uint16_t materialIndex = 0;
		file.read(reinterpret_cast<char*>(&materialIndex), sizeof(uint16_t));
		uint32_t extraBufferPoints = 0;
		file.read(reinterpret_cast<char*>(&extraBufferPoints),
				  sizeof(uint32_t));

		file.read(reinterpret_cast<char*>(&indexCount), sizeof(uint32_t));
		indices = std::make_unique<uint32_t[]>(indexCount);
		file.read(reinterpret_cast<char*>(indices.get()),
				  indexCount * sizeof(uint32_t));

		file.read(reinterpret_cast<char*>(&vertexCount), sizeof(uint32_t));
		uint8_t vertexSize = 0;
		file.read(reinterpret_cast<char*>(&vertexSize), sizeof(uint8_t));
		vertices = std::make_unique<float[]>(vertexCount * vertexSize);
		file.read(reinterpret_cast<char*>(vertices.get()),
				  vertexCount * vertexSize * sizeof(float));

		file.read(reinterpret_cast<char*>(&normalCount), sizeof(uint32_t));
		uint8_t normalSize = 0;
		file.read(reinterpret_cast<char*>(&normalSize), sizeof(uint8_t));
		normals = std::make_unique<float[]>(normalCount * normalSize);
		file.read(reinterpret_cast<char*>(normals.get()),
				  normalCount * normalSize * sizeof(float));

		file.read(reinterpret_cast<char*>(&textureCoordCount),
				  sizeof(uint32_t));
		uint8_t textureCoordSize = 0;
		file.read(reinterpret_cast<char*>(&textureCoordSize), sizeof(uint8_t));
		textureCoords =
			std::make_unique<float[]>(textureCoordCount * textureCoordSize);
		file.read(reinterpret_cast<char*>(textureCoords.get()),
				  textureCoordCount * textureCoordSize * sizeof(float));

		// Extra streams follow the fixed ones; stream 0 holds tangents
		// (xyz + handedness). Unknown streams are skipped.
		std::unique_ptr<Buffer<float, 4>> tangents = nullptr;
		for (uint32_t extra = 0; extra < extraBufferPoints; extra++) {
			uint32_t count = 0;
			uint8_t floatsPer = 0;
			file.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
			file.read(reinterpret_cast<char*>(&floatsPer), sizeof(uint8_t));
			const size_t floatCount = static_cast<size_t>(count) * floatsPer;
			auto values = std::make_unique<float[]>(floatCount);
			file.read(reinterpret_cast<char*>(values.get()),
					  floatCount * sizeof(float));
			if (extra == 0 && floatsPer == 4 && count == vertexCount) {
				tangents = BufferBuilder<float, 4>(BUFFER_STAGED | BUFFER_ARRAY)
							   .data(std::move(values), floatCount)
							   .build();
			}
		}
		if (!file) {
			throw std::runtime_error("Truncated mesh file: " + path + ".bird");
		}

		std::unique_ptr<bird::Buffer<uint32_t>> indexBuffer =
			BufferBuilder<uint32_t>(bird::BUFFER_STAGED |
									bird::BUFFER_ELEMENT_ARRAY)
				.data(std::move(indices), indexCount)
				.build();
		std::unique_ptr<bird::Buffer<float>> vertexBuffer =
			BufferBuilder<float>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY)
				.data(std::move(vertices), vertexCount * vertexSize)
				.build();
		// Host copy stays alive inside the buffer while the builder holds it.
		const float* positions = vertexBuffer->m_pData.get();
		std::unique_ptr<bird::Buffer<float>> normalBuffer =
			BufferBuilder<float>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY)
				.data(std::move(normals), normalCount * normalSize)
				.build();

		std::unique_ptr<bird::Buffer<float, 2>> textureCoordBuffer =
			BufferBuilder<float, 2>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY)
				.data(std::move(textureCoords),
					  textureCoordCount * textureCoordSize)
				.build();

		bird::MeshBuilder mb = bird::MeshBuilder();

		mb.setIndexBuffer(std::move(indexBuffer))
			.setVertexBuffer(std::move(vertexBuffer))
			.setNormalBuffer(std::move(normalBuffer))
			.setTextureCoordBuffer(std::move(textureCoordBuffer));

		if (tangents) mb.setTangentBuffer(std::move(tangents));
		std::vector<float> jointRadii;
		if (skeletal && vertexSize == 3) {
			jointRadii =
				attachSkin(mb, *skeletal, i, vertexCount, positions, path);
		}

		std::shared_ptr<Material> mat =
			loadMaterial(path + std::to_string(materialIndex) + ".mat");
		if (!mat) mat = std::make_shared<Material>();
		mb.setMaterial(mat);
		meshes.data()[i] = mb.build();
		if (!jointRadii.empty()) {
			meshes.data()[i]->setJointRadii(std::move(jointRadii));
		}
		// addResource(path + std::to_string(i), meshes.data()[i], MESH);
	}

	file.close();

	addMesh(path, meshes);

	return meshes;
}

std::shared_ptr<Material> ResourceManager::loadMaterial(
	const std::string& path) {
	if (m_resources[ASSET_TYPE_MATERIAL].find(path) !=
		m_resources[ASSET_TYPE_MATERIAL].end()) {
		return std::static_pointer_cast<Material>(
			m_resources[ASSET_TYPE_MATERIAL].at(path));
	}

	// load material from file
	std::fstream file(RESOURCE_PATH + path + ".bird",
					  std::ios::in | std::ios::binary);

	if (!file.is_open()) {
		std::cout << "Error loading file: " << path << std::endl;
		file.close();
		return std::shared_ptr<Material>(0);
	}

	const format::MaterialFileData data = format::readMaterialFile(file);

	auto mat = std::make_shared<Material>();
	mat->setBaseColor(Vector4(data.baseColor[0], data.baseColor[1],
							  data.baseColor[2], data.baseColor[3]))
		.setEmissive(
			Vector3(data.emissive[0], data.emissive[1], data.emissive[2]))
		.setEmissiveStrength(data.emissiveStrength)
		.setMetallic(data.metallic)
		.setRoughness(data.roughness)
		.setAlphaCutoff(data.alphaCutoff)
		.setNormalScale(data.normalScale)
		.setOcclusionStrength(data.occlusionStrength)
		.setBlendMode(static_cast<BlendMode>(data.blendMode))
		.setDomain(data.unlit ? MaterialDomain::Unlit : MaterialDomain::Lit)
		.setDoubleSided(data.doubleSided != 0);

	const std::string directory =
		std::filesystem::path(path).remove_filename().string();
	for (const auto& [slot, relativePath] : data.textures) {
		const auto textureSlot = static_cast<TextureSlot>(slot);
		// Color data is authored in sRGB; data maps (normals, roughness)
		// are linear and must not be gamma decoded.
		const bool isColor = textureSlot == TextureSlot::BaseColor ||
							 textureSlot == TextureSlot::Emissive;
		const std::string texturePath = directory + relativePath;
		std::shared_ptr<Texture> texture = isColor
											   ? loadColorTexture(texturePath)
											   : loadTexture(texturePath);
		if (texture) mat->setTexture(textureSlot, texture);
	}
	// glTF semantics: a texture multiplies its factor, so a legacy diffuse
	// texture replaces the diffuse color the old shader ignored.
	if (data.legacy && mat->getTexture(TextureSlot::BaseColor)) {
		mat->setBaseColor(Vector4(1.0f, 1.0f, 1.0f, data.baseColor[3]));
	}

	file.close();

	addResource(path, mat, ASSET_TYPE_MATERIAL);

	return mat;
}

std::shared_ptr<Shader> ResourceManager::loadShader(
	const ShaderBuilder& builder) {
	const std::string cacheKey = builder.cacheKey();
	auto& shaders = m_resources[ASSET_TYPE_SHADER];
	if (auto cached = shaders.find(cacheKey); cached != shaders.end()) {
		return std::static_pointer_cast<Shader>(cached->second);
	}
	std::shared_ptr<Shader> shader = builder.build();
	addResource(cacheKey, shader, ASSET_TYPE_SHADER);
	return shader;
}

std::shared_ptr<Shader> ResourceManager::loadShader(
	const std::string& vertexPath, const std::string& fragmentPath) {
	return loadShader(ShaderBuilder()
						  .attachShaderFile(vertexPath, VERTEX)
						  .attachShaderFile(fragmentPath, FRAGMENT));
}

void ResourceManager::loadSound(std::string path) {}

void ResourceManager::loadCustom(std::string path) {}

void ResourceManager::addResource(std::string path,
								  std::shared_ptr<Resource> resource,
								  AssetType type) {
	if (type == ASSET_TYPE_MESH) {
		// throw RuntimeException("Error: Cannot add mesh to resource manager
		// using addResource function. ");
		std::cout << "Error: Cannot add mesh to resource manager using "
					 "addResource function. "
				  << std::endl;
		return;
	}
	m_resources[type].insert(
		std::pair<std::string, std::shared_ptr<Resource>>(path, resource));
}

void ResourceManager::addMesh(std::string path,
							  std::vector<std::shared_ptr<Mesh>> meshes) {
	m_meshes.insert(std::pair<std::string, std::vector<std::shared_ptr<Mesh>>>(
		path, meshes));
}

std::shared_ptr<Shader> ResourceManager::getDefaultShader() {
	return m_defaultShader;
}

}  // namespace bird
