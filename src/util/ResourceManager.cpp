//
// Created by student on 3/8/24.
//

#include "ResourceManager.h"

#include <optional>
#include <sstream>
#include <vector>

#include "../animation/skeletal/PoseOps.h"
#include "../platform/Log.h"
#include "../platform/VFS.h"
#ifdef BIRD_ENABLE_HOT_RELOAD
#include "../hotreload/HotReloadManager.h"
#endif

namespace bird {

namespace {

/**
 * Opens a baked asset through the platform VFS as an in-memory stream, so
 * assets inside an APK or an app bundle load like files on a desktop.
 * @return nullptr when the file is missing. O(file size).
 */
std::unique_ptr<std::istringstream> openBaked(const std::string& path) {
	std::optional<std::vector<uint8_t>> bytes =
		platform::VFS::get().readFile(path);
	if (!bytes) return nullptr;
	return std::make_unique<std::istringstream>(
		std::string(bytes->begin(), bytes->end()), std::ios::binary);
}

}  // namespace

ResourceManager::ResourceManager() {
	m_resources = new std::unordered_map<
		std::string, std::shared_ptr<Resource>>[ASSET_TYPE_CUSTOM + 1];
}

ResourceManager::~ResourceManager() {
#ifdef BIRD_ENABLE_HOT_RELOAD
	disableHotReload();
#endif
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

std::shared_ptr<Texture> ResourceManager::buildTextureFromBaked(
	const std::string& path, bool srgb) {
	const auto file = openBaked(RESOURCE_PATH + path + ".bird");

	if (!file) {
		std::cout << "Error loading file. File does not exist: " << path
				  << std::endl;
		return std::shared_ptr<Texture>(0);
	}

	int32_t width = 0, height = 0, channels = 0;

	file->read(reinterpret_cast<char*>(&width), sizeof(int32_t));
	file->read(reinterpret_cast<char*>(&height), sizeof(int32_t));
	file->read(reinterpret_cast<char*>(&channels), sizeof(int32_t));
	if (!*file || width <= 0 || height <= 0 || channels <= 0 || channels > 4) {
		std::cout << "Error loading file: " << path << std::endl;
		std::cout << "Invalid baked texture header" << std::endl;
		return std::shared_ptr<Texture>(0);
	}
	const size_t byteCount =
		static_cast<size_t>(width) * static_cast<size_t>(height) *
		static_cast<size_t>(channels);
	uint8_t* data = new uint8_t[byteCount];
	file->read(reinterpret_cast<char*>(data),
			   static_cast<std::streamsize>(byteCount));
	if (!*file) {
		delete[] data;
		std::cout << "Error loading file: " << path
				  << " (truncated texture data)" << std::endl;
		return std::shared_ptr<Texture>(0);
	}

	TextureFormat form = (TextureFormat)(1 << (channels));
	TextureBuilder builder(data, width, height, form);
	builder.generateMipMaps();
	if (srgb) builder.srgb();
	return builder.build();
}

std::shared_ptr<Texture> ResourceManager::loadTextureWithColorSpace(
	const std::string& path, bool srgb) {
	const std::string cacheKey = srgb ? path + "#srgb" : path;
	auto& textures = m_resources[ASSET_TYPE_TEXTURE];
	if (textures.find(cacheKey) != textures.end()) {
		return std::static_pointer_cast<Texture>(textures.at(cacheKey));
	}

	std::shared_ptr<Texture> tex = buildTextureFromBaked(path, srgb);
	if (!tex) {
		return nullptr;
	}
	addResource(cacheKey, tex, ASSET_TYPE_TEXTURE);
	return tex;
}

std::shared_ptr<Font> ResourceManager::buildFontFromBaked(
	const std::string& path) {
	const auto file = openBaked(RESOURCE_PATH + path + ".bird");
	if (!file) {
		std::cerr << "Error loading font. File does not exist: " << path
				  << ".bird (run BuildTools)" << std::endl;
		return nullptr;
	}

	format::FontHeader header{};
	file->read(reinterpret_cast<char*>(&header), sizeof(header));
	std::vector<format::FontGlyph> glyphs(header.glyphCount);
	file->read(reinterpret_cast<char*>(glyphs.data()),
			   glyphs.size() * sizeof(format::FontGlyph));
	const size_t texelCount =
		static_cast<size_t>(header.atlasWidth) * header.atlasHeight;
	std::vector<uint8_t> distance(texelCount);
	file->read(reinterpret_cast<char*>(distance.data()), distance.size());
	if (!*file) {
		throw std::runtime_error("Truncated font file: " + path + ".bird");
	}

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

	return std::make_shared<Font>(header, std::move(glyphs), atlas);
}

std::shared_ptr<Font> ResourceManager::loadFont(const std::string& path) {
	auto& fonts = m_resources[ASSET_TYPE_FONT];
	if (fonts.find(path) != fonts.end()) {
		return std::static_pointer_cast<Font>(fonts.at(path));
	}

	auto font = buildFontFromBaked(path);
	if (!font) {
		return nullptr;
	}
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
	// Through the VFS, so skeleton bakes inside an APK load like files.
	const auto file = openBaked(RESOURCE_PATH + path + ".skel.bird");
	if (!file) return nullptr;
	auto asset = std::make_shared<const SkeletalAsset>(readSkeletalAsset(*file));
	m_skeletalAssets.emplace(path, asset);
	return asset;
}

std::vector<std::shared_ptr<Mesh>> ResourceManager::loadMesh(
	const std::string& path) {
	if (m_meshes.find(path) != m_meshes.end()) {
		return m_meshes.at(path);
	}
	// load mesh from file
	const auto file = openBaked(RESOURCE_PATH + path + ".bird");

	if (!file) {
		std::cout << "Error loading file: " << path << std::endl;
		return std::vector<std::shared_ptr<Mesh>>(0);
	}

	uint16_t meshCount = 0;
	file->read(reinterpret_cast<char*>(&meshCount), sizeof(uint16_t));
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
		file->read(reinterpret_cast<char*>(&materialIndex), sizeof(uint16_t));
		uint32_t extraBufferPoints = 0;
		file->read(reinterpret_cast<char*>(&extraBufferPoints),
				  sizeof(uint32_t));

		file->read(reinterpret_cast<char*>(&indexCount), sizeof(uint32_t));
		indices = std::make_unique<uint32_t[]>(indexCount);
		file->read(reinterpret_cast<char*>(indices.get()),
				  indexCount * sizeof(uint32_t));

		file->read(reinterpret_cast<char*>(&vertexCount), sizeof(uint32_t));
		uint8_t vertexSize = 0;
		file->read(reinterpret_cast<char*>(&vertexSize), sizeof(uint8_t));
		vertices = std::make_unique<float[]>(vertexCount * vertexSize);
		file->read(reinterpret_cast<char*>(vertices.get()),
				  vertexCount * vertexSize * sizeof(float));

		file->read(reinterpret_cast<char*>(&normalCount), sizeof(uint32_t));
		uint8_t normalSize = 0;
		file->read(reinterpret_cast<char*>(&normalSize), sizeof(uint8_t));
		normals = std::make_unique<float[]>(normalCount * normalSize);
		file->read(reinterpret_cast<char*>(normals.get()),
				  normalCount * normalSize * sizeof(float));

		file->read(reinterpret_cast<char*>(&textureCoordCount),
				  sizeof(uint32_t));
		uint8_t textureCoordSize = 0;
		file->read(reinterpret_cast<char*>(&textureCoordSize), sizeof(uint8_t));
		textureCoords =
			std::make_unique<float[]>(textureCoordCount * textureCoordSize);
		file->read(reinterpret_cast<char*>(textureCoords.get()),
				  textureCoordCount * textureCoordSize * sizeof(float));

		// Extra streams follow the fixed ones; stream 0 holds tangents
		// (xyz + handedness). Unknown streams are skipped.
		std::unique_ptr<Buffer<float, 4>> tangents = nullptr;
		for (uint32_t extra = 0; extra < extraBufferPoints; extra++) {
			uint32_t count = 0;
			uint8_t floatsPer = 0;
			file->read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
			file->read(reinterpret_cast<char*>(&floatsPer), sizeof(uint8_t));
			const size_t floatCount = static_cast<size_t>(count) * floatsPer;
			auto values = std::make_unique<float[]>(floatCount);
			file->read(reinterpret_cast<char*>(values.get()),
					  floatCount * sizeof(float));
			if (extra == 0 && floatsPer == 4 && count == vertexCount) {
				tangents = BufferBuilder<float, 4>(BUFFER_STAGED | BUFFER_ARRAY)
							   .data(std::move(values), floatCount)
							   .build();
			}
		}
		if (!*file) {
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


	addMesh(path, meshes);

	return meshes;
}

std::shared_ptr<Material> ResourceManager::buildMaterialFromBaked(
	const std::string& path) {
	// load material from file
	const auto file = openBaked(RESOURCE_PATH + path + ".bird");

	if (!file) {
		std::cout << "Error loading file: " << path << std::endl;
		return std::shared_ptr<Material>(0);
	}

	const format::MaterialFileData data = format::readMaterialFile(*file);

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

	return mat;
}

std::shared_ptr<Material> ResourceManager::loadMaterial(
	const std::string& path) {
	if (m_resources[ASSET_TYPE_MATERIAL].find(path) !=
		m_resources[ASSET_TYPE_MATERIAL].end()) {
		return std::static_pointer_cast<Material>(
			m_resources[ASSET_TYPE_MATERIAL].at(path));
	}

	auto mat = buildMaterialFromBaked(path);
	if (!mat) {
		return nullptr;
	}

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
	m_shaderBuilders.emplace(cacheKey, builder);
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

bool ResourceManager::reloadTexture(const std::string& path) {
	auto& textures = m_resources[ASSET_TYPE_TEXTURE];
	bool reloadedAny = false;

	const std::pair<std::string, bool> variants[2] = {
		{path, false}, {path + "#srgb", true}};
	for (const auto& [cacheKey, srgb] : variants) {
		auto it = textures.find(cacheKey);
		if (it == textures.end()) continue;

		try {
			std::shared_ptr<Texture> replacement =
				buildTextureFromBaked(path, srgb);
			if (!replacement) {
				platform::log(platform::LogLevel::Error,
							  "[HotReload] Failed to reload texture '" + path +
								  "'; keeping previous handle active.");
				continue;
			}
			auto existing = std::static_pointer_cast<Texture>(it->second);
			existing->swapFrom(*replacement);
			reloadedAny = true;
			platform::log(platform::LogLevel::Info,
						  "[HotReload] Swapped texture in place: " + cacheKey);
		} catch (const std::exception& e) {
			platform::log(platform::LogLevel::Error,
						  "[HotReload] Exception reloading texture '" + path +
							  "': " + e.what());
		}
	}
	return reloadedAny;
}

size_t ResourceManager::reloadShaderStage(const std::string& stagePath) {
	auto& shaders = m_resources[ASSET_TYPE_SHADER];
	size_t reloadedCount = 0;

	for (const auto& [cacheKey, builder] : m_shaderBuilders) {
		bool usesStage = false;
		for (const auto& [file, _] : builder.stages()) {
			if (file == stagePath) {
				usesStage = true;
				break;
			}
		}
		if (!usesStage) continue;

		auto it = shaders.find(cacheKey);
		if (it == shaders.end()) continue;

		try {
			std::shared_ptr<Shader> replacement = builder.build();
			if (!replacement) continue;
			auto existing = std::static_pointer_cast<Shader>(it->second);
			existing->swapFrom(*replacement);
			reloadedCount++;
			platform::log(platform::LogLevel::Info,
						  "[HotReload] Swapped shader program in place: " +
							  cacheKey);
		} catch (const std::exception& e) {
			platform::log(platform::LogLevel::Error,
						  "[HotReload] Failed to relink shader '" + cacheKey +
							  "' (keeping old program active): " + e.what());
		}
	}
	return reloadedCount;
}

size_t ResourceManager::reloadAllShaders() {
	auto& shaders = m_resources[ASSET_TYPE_SHADER];
	size_t reloadedCount = 0;

	for (const auto& [cacheKey, builder] : m_shaderBuilders) {
		auto it = shaders.find(cacheKey);
		if (it == shaders.end()) continue;

		try {
			std::shared_ptr<Shader> replacement = builder.build();
			if (!replacement) continue;
			auto existing = std::static_pointer_cast<Shader>(it->second);
			existing->swapFrom(*replacement);
			reloadedCount++;
		} catch (const std::exception& e) {
			platform::log(platform::LogLevel::Error,
						  "[HotReload] Failed to relink shader '" + cacheKey +
							  "' after include change (keeping old program "
							  "active): " +
							  e.what());
		}
	}
	if (reloadedCount > 0) {
		platform::log(platform::LogLevel::Info,
					  "[HotReload] Relinked " + std::to_string(reloadedCount) +
						  " shader program(s) after include edit");
	}
	return reloadedCount;
}

bool ResourceManager::reloadMaterial(const std::string& path) {
	auto& materials = m_resources[ASSET_TYPE_MATERIAL];
	auto it = materials.find(path);
	if (it == materials.end()) return false;

	try {
		std::shared_ptr<Material> replacement = buildMaterialFromBaked(path);
		if (!replacement) {
			platform::log(platform::LogLevel::Error,
						  "[HotReload] Failed to reload material '" + path +
							  "'; keeping previous material active.");
			return false;
		}
		auto existing = std::static_pointer_cast<Material>(it->second);
		existing->swapFrom(*replacement);
		platform::log(platform::LogLevel::Info,
					  "[HotReload] Swapped material in place: " + path);
		return true;
	} catch (const std::exception& e) {
		platform::log(platform::LogLevel::Error,
					  "[HotReload] Exception reloading material '" + path +
						  "': " + e.what());
		return false;
	}
}

bool ResourceManager::reloadFont(const std::string& path) {
	auto& fonts = m_resources[ASSET_TYPE_FONT];
	auto it = fonts.find(path);
	if (it == fonts.end()) return false;

	try {
		std::shared_ptr<Font> replacement = buildFontFromBaked(path);
		if (!replacement) {
			platform::log(platform::LogLevel::Error,
						  "[HotReload] Failed to reload font '" + path +
							  "'; keeping previous font active.");
			return false;
		}
		auto existing = std::static_pointer_cast<Font>(it->second);
		existing->swapFrom(*replacement);
		platform::log(platform::LogLevel::Info,
					  "[HotReload] Swapped font in place: " + path);
		return true;
	} catch (const std::exception& e) {
		platform::log(platform::LogLevel::Error,
					  "[HotReload] Exception reloading font '" + path +
						  "': " + e.what());
		return false;
	}
}

#ifdef BIRD_ENABLE_HOT_RELOAD
bool ResourceManager::enableHotReload(
	const hotreload::HotReloadConfig& config) {
	disableHotReload();
	m_hotReloadManager = std::make_unique<hotreload::HotReloadManager>(config);
	if (!m_hotReloadManager->start()) {
		m_hotReloadManager.reset();
		return false;
	}
	return true;
}

void ResourceManager::disableHotReload() {
	if (m_hotReloadManager) {
		m_hotReloadManager->stop();
		m_hotReloadManager.reset();
	}
}

size_t ResourceManager::pollHotReloads() {
	if (!m_hotReloadManager) return 0;

	const auto requests = m_hotReloadManager->drainRequests();
	size_t handled = 0;

	for (const hotreload::AssetReloadRequest& req : requests) {
		switch (req.assetId.kind) {
			case hotreload::HotReloadAssetKind::Shader:
				if (reloadShaderStage(req.assetId.sourcePath) > 0) handled++;
				break;
			case hotreload::HotReloadAssetKind::ShaderInclude:
				if (reloadAllShaders() > 0) handled++;
				break;
			case hotreload::HotReloadAssetKind::Texture:
				if (reloadTexture(req.assetId.sourcePath)) handled++;
				break;
			case hotreload::HotReloadAssetKind::Material:
				if (reloadMaterial(req.assetId.sourcePath)) handled++;
				break;
			case hotreload::HotReloadAssetKind::Font:
				if (reloadFont(req.assetId.sourcePath)) handled++;
				break;
			case hotreload::HotReloadAssetKind::DartScript:
				if (m_dartReloadCallback) {
					m_dartReloadCallback(req.assetId.sourcePath);
					handled++;
				}
				break;
			default:
				break;
		}
	}
	return handled;
}

void ResourceManager::setDartReloadCallback(
	hotreload::DartReloadCallback callback) {
	m_dartReloadCallback = std::move(callback);
}

hotreload::HotReloadManager* ResourceManager::getHotReloadManager() const {
	return m_hotReloadManager.get();
}
#endif

}  // namespace bird
