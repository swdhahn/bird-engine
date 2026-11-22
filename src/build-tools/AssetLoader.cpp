//
// Created by Seth Hahn on 2/23/24.
//

#include "AssetLoader.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "FormatHeaders.h"
#include "MaterialFormat.h"

#include <assimp/GltfMaterial.h>
#include "ShaderBaker.h"
#include "SkeletalImport.h"

// Static: Assimp 5.3.1 bundles its own stb_image whose *_thread setters
// stay external, so a second external copy collides when linking BuildTools
// against a static libassimp. Every stbi_ call in BuildTools is in this file.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../util/stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../util/stb_truetype.h"

namespace bird {

enum AssetType {
	TEXTURE,
	MODEL,
	SCENE,
	SOUND,
	SHADER,
	MATERIAL,
	FONT,
	CUSTOM
};

std::vector<std::vector<const char*>> supportedFileTypes{
	{".png", ".jpg", ".jpeg", ".tga", ".bmp", ".psd", ".gif", ".hdr", ".pic",
	 ".pnm"},												 // Textures
	{".obj", ".fbx", ".gltf", ".glb"},						 // 3D models
	{".gltf", ".glb"},										 // Scenes
	{".wav", ".mp3", ".ogg"},								 // Sounds
	{".vert", ".frag", ".geom", ".comp", ".tesc", ".tese"},	 // Shaders
	{".mtl", ".pbr"},										 // Materials
	{".ttf", ".otf"},										 // Fonts
	{".bird_custom"}										 // Other
};	//*/

AssetLoader::AssetLoader() {}
AssetLoader::~AssetLoader() {}

size_t AssetLoader::init(std::string assetPath, bool force) {
	size_t failures = 0;
	for (const auto& entry :
		 std::filesystem::recursive_directory_iterator(assetPath)) {
		if (entry.is_directory()) {
			continue;
		}
		if (!compileFile(entry.path().string(), force)) {
			failures++;
		}
	}
	return failures;
}

bool AssetLoader::compileFile(const std::string& filePath, bool force) {
	m_forceRebuild = force;
	const std::filesystem::path fsPath(filePath);
	std::string extLower = fsPath.extension().string();
	std::transform(extLower.begin(), extLower.end(), extLower.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	int matchedType = -1;
	for (size_t i = 0; i < supportedFileTypes.size(); i++) {
		for (const auto& ext : supportedFileTypes[i]) {
			if (extLower == ext) {
				matchedType = static_cast<int>(i);
				break;
			}
		}
		if (matchedType != -1) {
			break;
		}
	}

	try {
		switch (static_cast<AssetType>(matchedType)) {
			case TEXTURE:
				loadTexture(filePath);
				break;
			case MODEL:
				loadModel(filePath);
				break;
			case SCENE:
				loadScene(filePath);
				break;
			case SOUND:
				loadSound(filePath);
				break;
			case SHADER:
				loadShader(filePath);
				break;
			case MATERIAL:
				loadMaterial(filePath);
				break;
			case FONT:
				loadFont(filePath);
				break;
			case CUSTOM:
				loadCustom(filePath);
				break;
			default:
				// Unsupported files (.txt, .bird, etc.) are ignored without error.
				break;
		}
	} catch (const std::exception& error) {
		std::cerr << "FAILED: " << error.what() << std::endl;
		return false;
	}
	return true;
}

void AssetLoader::loadAsset() {
	// load asset
}

void AssetLoader::loadScene(const std::string& path) {
	// load scene
}

void DebugPrintAllTextures(const aiMaterial* mat) {
	return;
	std::cout << "--- Material Debug Info ---" << std::endl;
	// Iterate clearly through all known Assimp texture types
	// Note: Assimp 5.x defines up to around 18 types.
	const int AI_TEXTURE_TYPE_MAX = 20;

	for (int i = 0; i <= AI_TEXTURE_TYPE_MAX; i++) {
		aiTextureType type = static_cast<aiTextureType>(i);
		unsigned int count = mat->GetTextureCount(type);

		if (count > 0) {
			std::cout << "Found " << count
					  << " texture(s) at Type Index: " << i;

			// Map index to name for clarity
			switch (i) {
				case 1:
					std::cout << " (DIFFUSE)";
					break;
				case 12:
					std::cout << " (BASE_COLOR / PBR)";
					break;
				case 5:
					std::cout << " (HEIGHT / NORMAL)";
					break;
				case 6:
					std::cout << " (NORMALS)";
					break;
				case 16:
					std::cout << " (ROUGHNESS)";
					break;
				case 17:
					std::cout << " (METALLIC)";
					break;
			}
			std::cout << std::endl;

			aiString path;
			mat->GetTexture(type, 0, &path);
			std::cout << "   Path: " << path.C_Str() << std::endl;
		}
	}
}

namespace {

/**
 * A baked model is current when newer than its source and its first material
 * already uses the versioned PBR layout; older bakes lack tangents and roles.
 */
bool isModelBakeCurrent(const std::string& path) {
	namespace fs = std::filesystem;
	const std::string baked = path + ".bird";
	if (!fs::exists(baked)) return false;
	if (fs::last_write_time(baked) < fs::last_write_time(path)) return false;
	// A skinned model is current only if its skeleton bake is too; static
	// models never write one, so its absence alone proves nothing.
	const std::string skeleton = path + ".skel.bird";
	if (fs::exists(skeleton) &&
		fs::last_write_time(skeleton) < fs::last_write_time(path)) {
		return false;
	}
	std::ifstream material(path + "0.mat.bird", std::ios::binary);
	if (!material.is_open()) return true;  // model without materials
	char magic[4] = {};
	material.read(magic, sizeof(magic));
	return material &&
		   std::memcmp(magic, format::MATERIAL_MAGIC, sizeof(magic)) == 0;
}

/** Maps glTF alpha mode strings onto baked blend identifiers. */
uint8_t blendFromAlphaMode(const aiMaterial* material, float opacity) {
	aiString alphaMode;
	if (AI_SUCCESS == material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode)) {
		const std::string mode = alphaMode.C_Str();
		if (mode == "MASK") return format::MATERIAL_BLEND_MASKED;
		if (mode == "BLEND") return format::MATERIAL_BLEND_TRANSPARENT;
		return format::MATERIAL_BLEND_OPAQUE;
	}
	return opacity < 0.999f ? format::MATERIAL_BLEND_TRANSPARENT
							: format::MATERIAL_BLEND_OPAQUE;
}

/**
 * Resolves the path written into the material for a texture reference:
 * embedded textures ("*N") become the sibling "<model>_etexN" asset.
 */
std::string resolveTexturePath(const std::string& modelPath,
							   const aiString& reference) {
	std::string texturePath = reference.C_Str();
	if (!texturePath.empty() && texturePath[0] == '*') {
		return std::filesystem::path(modelPath).filename().string() + "_etex" +
			   texturePath.substr(1);
	}
	return texturePath;
}

/** Extracts metallic-roughness PBR parameters and texture roles. */
format::MaterialFileData extractMaterial(const aiMaterial* aiMat,
										 const std::string& modelPath) {
	format::MaterialFileData data;
	aiColor4D color4;
	aiColor3D color3;
	if (AI_SUCCESS == aiMat->Get(AI_MATKEY_BASE_COLOR, color4)) {
		data.baseColor[0] = color4.r;
		data.baseColor[1] = color4.g;
		data.baseColor[2] = color4.b;
		data.baseColor[3] = color4.a;
	} else if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color3)) {
		data.baseColor[0] = color3.r;
		data.baseColor[1] = color3.g;
		data.baseColor[2] = color3.b;
	}
	float opacity = 1.0f;
	if (AI_SUCCESS == aiMat->Get(AI_MATKEY_OPACITY, opacity)) {
		data.baseColor[3] = std::min(data.baseColor[3], opacity);
	}

	float metallic = 0.0f, roughness = 0.0f, shininess = 0.0f;
	if (AI_SUCCESS == aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic)) {
		data.metallic = metallic;
	}
	if (AI_SUCCESS == aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness)) {
		data.roughness = roughness;
	} else if (AI_SUCCESS == aiMat->Get(AI_MATKEY_SHININESS, shininess)) {
		data.roughness = format::shininessToRoughness(shininess);
	}
	if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color3)) {
		data.emissive[0] = color3.r;
		data.emissive[1] = color3.g;
		data.emissive[2] = color3.b;
	}
	float emissiveStrength = 1.0f;
	if (AI_SUCCESS ==
		aiMat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveStrength)) {
		data.emissiveStrength = emissiveStrength;
	}
	float cutoff = 0.5f;
	if (AI_SUCCESS == aiMat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, cutoff)) {
		data.alphaCutoff = cutoff;
	}
	int twoSided = 0;
	if (AI_SUCCESS == aiMat->Get(AI_MATKEY_TWOSIDED, twoSided)) {
		data.doubleSided = twoSided != 0;
	}
	int shadingModel = 0;
	if (AI_SUCCESS == aiMat->Get(AI_MATKEY_SHADING_MODEL, shadingModel)) {
		data.unlit = shadingModel == aiShadingMode_Unlit;
	}
	data.blendMode = blendFromAlphaMode(aiMat, data.baseColor[3]);

	// Candidate assimp types per role, most specific first.
	const std::pair<uint8_t, std::vector<aiTextureType>> roles[] = {
		{format::MATERIAL_SLOT_BASE_COLOR,
		 {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE}},
		{format::MATERIAL_SLOT_METALLIC_ROUGHNESS,
		 {aiTextureType_UNKNOWN, aiTextureType_METALNESS,
		  aiTextureType_DIFFUSE_ROUGHNESS}},
		{format::MATERIAL_SLOT_NORMAL,
		 {aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA,
		  aiTextureType_HEIGHT}},
		{format::MATERIAL_SLOT_OCCLUSION,
		 {aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP}},
		{format::MATERIAL_SLOT_EMISSIVE,
		 {aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE}},
	};
	for (const auto& [slot, candidates] : roles) {
		for (aiTextureType type : candidates) {
			aiString reference;
			if (aiMat->GetTextureCount(type) > 0 &&
				AI_SUCCESS == aiMat->GetTexture(type, 0, &reference)) {
				data.textures.emplace_back(
					slot, resolveTexturePath(modelPath, reference));
				break;
			}
		}
	}
	// Emissive factors default to black in glTF even when a texture exists.
	const bool hasEmissiveTexture =
		std::any_of(data.textures.begin(), data.textures.end(),
					[](const auto& t) {
						return t.first == format::MATERIAL_SLOT_EMISSIVE;
					});
	if (hasEmissiveTexture && data.emissive[0] + data.emissive[1] +
									  data.emissive[2] <= 0.0f) {
		data.emissive[0] = data.emissive[1] = data.emissive[2] = 1.0f;
	}
	return data;
}

}  // namespace

void AssetLoader::loadModel(const std::string& path) {
	if (!m_forceRebuild && isModelBakeCurrent(path)) {
		return;
	}

	std::cout << "Compiling Model: " << path << std::endl;
	// Import without flattening first: PreTransformVertices discards the
	// node hierarchy, bones and animations that skinned models need.
	const aiScene* scene = importer.ReadFile(
		path, aiProcess_CalcTangentSpace | aiProcess_Triangulate |
				  aiProcess_SortByPType | aiProcess_JoinIdenticalVertices |
				  aiProcess_FixInfacingNormals | aiProcess_LimitBoneWeights);
	if (scene == nullptr) {
		std::cout << "Error loading scene" << std::endl;
		std::cout << importer.GetErrorString() << std::endl;
		return;
	}
	const bool skinned = sceneHasSkinning(*scene);
	if (skinned) {
		// Skinned vertices stay in bind space; the skeleton positions them.
		// Limitation: static meshes in the same file keep their local space.
		try {
			saveSkeletalAsset(path + ".skel.bird",
							  extractSkeletalAsset(*scene));
		} catch (const std::exception& e) {
			std::cout << "Error baking skeleton: " << e.what() << std::endl;
			return;
		}
	} else {
		// Static models keep the previous behaviour: one flattened space.
		scene = importer.ApplyPostProcessing(aiProcess_PreTransformVertices);
		if (scene == nullptr) {
			std::cout << importer.GetErrorString() << std::endl;
			return;
		}
		std::filesystem::remove(path + ".skel.bird");  // stale skin bake
	}

	std::cout << "Model Compiled. Saving file" << std::endl;
	std::fstream file(path + ".bird", std::ios::out | std::ios::binary);

	// file will be in the following format:
	/*
	 * Number of meshes
	 * Material index // Mesh 0 start
	 * Number of extra buffer data points // vertices, indices, normals,
	 * texCoords, etc.  This will be 0 if it's just the standard vertices to
	 * texCoords Number of indices Index 0 Index 1
	 * ...
	 * Number of vertices
	 * Number of floats per vertex
	 * Vertex 0
	 * Vertex 1
	 * ...
	 * Number of normals
	 * number of floats per normal
	 * normal 0
	 * normal 1
	 * ...
	 * Number of texCoords
	 * Number of floats per texCoord
	 * texCoord 0
	 * texCoord 1
	 * ...
	 * Number of indices // Mesh 1 start
	 * ...
	 */
	int zero = 0;
	int two = 2;
	int three = 3;
	file.write((char*)&scene->mNumMeshes, sizeof(uint16_t));

	for (int i = 0; i < scene->mNumMeshes; i++) {
		const aiMesh* mesh = scene->mMeshes[i];
		const bool hasTangents =
			mesh->HasTangentsAndBitangents() && mesh->HasNormals();
		const uint32_t extraStreams = hasTangents ? 1 : 0;
		file.write((char*)&mesh->mMaterialIndex,
				   sizeof(uint16_t));  // material index
		file.write((char*)&extraStreams, sizeof(uint32_t));

		int numIndices = 0;
		for (int j = 0; j < scene->mMeshes[i]->mNumFaces; j++) {
			numIndices += scene->mMeshes[i]->mFaces[j].mNumIndices;
		}
		file.write((char*)&numIndices, sizeof(uint32_t));  // number of indices
		for (int j = 0; j < scene->mMeshes[i]->mNumFaces; j++) {
			file.write(
				(char*)&scene->mMeshes[i]->mFaces[j].mIndices[0],
				sizeof(uint32_t) * scene->mMeshes[i]->mFaces[j].mNumIndices);
		}

		file.write((char*)&scene->mMeshes[i]->mNumVertices, sizeof(uint32_t));
		file.write((char*)&three, sizeof(uint8_t));	 // dimension per vertex
		file.write((char*)&scene->mMeshes[i]->mVertices[0],
				   scene->mMeshes[i]->mNumVertices * three * sizeof(float));

		if (scene->mMeshes[i]->HasNormals()) {
			file.write((char*)&scene->mMeshes[i]->mNumVertices,
					   sizeof(uint32_t));
			file.write((char*)&three, sizeof(uint8_t));	 // dimension per normal
			file.write((char*)&scene->mMeshes[i]->mNormals[0],
					   scene->mMeshes[i]->mNumVertices * three * sizeof(float));
		} else {
			file.write((char*)&zero, sizeof(uint32_t));
			file.write((char*)&three, sizeof(uint8_t));
		}
		if (!scene->mMeshes[i]->HasTextureCoords(0)) {
			file.write((char*)&zero, sizeof(uint32_t));
			file.write((char*)&two, sizeof(uint8_t));
		} else {
			file.write((char*)&scene->mMeshes[i]->mNumVertices,
					   sizeof(uint32_t));
			file.write((char*)&two, sizeof(uint8_t));
			std::vector<aiVector2D> buffer2D(scene->mMeshes[i]->mNumVertices);

			// Use std::transform to strip the Z component
			// We may come back to this and support 3D texture coords
			std::transform(scene->mMeshes[i]->mTextureCoords[0],
						   scene->mMeshes[i]->mTextureCoords[0] +
							   scene->mMeshes[i]->mNumVertices,
						   buffer2D.begin(), [](const aiVector3D& v) {
							   return aiVector2D(v.x, v.y);
						   });

			file.write((char*)buffer2D.data(),
					   scene->mMeshes[i]->mNumVertices * sizeof(aiVector2D));
		}

		// Extra stream 0: tangent xyz plus bitangent handedness in w, so
		// shaders rebuild the bitangent as cross(N, T) * w.
		if (hasTangents) {
			std::vector<float> tangents(mesh->mNumVertices * 4);
			for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
				const aiVector3D& n = mesh->mNormals[v];
				const aiVector3D& t = mesh->mTangents[v];
				const aiVector3D& b = mesh->mBitangents[v];
				const float handedness = ((n ^ t) * b) < 0.0f ? -1.0f : 1.0f;
				tangents[v * 4 + 0] = t.x;
				tangents[v * 4 + 1] = t.y;
				tangents[v * 4 + 2] = t.z;
				tangents[v * 4 + 3] = handedness;
			}
			const uint8_t four = 4;
			file.write((char*)&mesh->mNumVertices, sizeof(uint32_t));
			file.write((char*)&four, sizeof(uint8_t));
			file.write((char*)tangents.data(), tangents.size() * sizeof(float));
		}
	}

	file.close();
	std::cout << "File saved" << std::endl;
	for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
		std::fstream mf((path + std::to_string(i)) + ".mat.bird",
						std::ios::out | std::ios::binary | std::ios::trunc);
		format::writeMaterialFile(mf,
								  extractMaterial(scene->mMaterials[i], path));
		mf.close();
	}

	for (int i = 0; i < scene->mNumTextures; i++) {
		aiTexture* image = scene->mTextures[i];

		uint8_t* imgData = nullptr;
		int width, height, channels;
		std::string fpath = path + "_etex" + std::to_string(i);

		if (scene->mTextures[i]->mHeight == 0) {
			imgData = stbi_load_from_memory((unsigned char*)(image->pcData),
											image->mWidth, &width, &height,
											&channels, 0);
			writeImage(width, height, channels, imgData, fpath);
			stbi_image_free(imgData);
		} else {
			width = image->mWidth;
			height = image->mHeight;
			channels = 4;
			imgData = new uint8_t[width * height * channels];
			std::memcpy(imgData, image->pcData, width * height * channels);
			// Assimp is in BRGA but we want RGBA
			for (int j = 0; j < width * height; j++) {
				imgData[j * channels] =
					((uint8_t*)image->pcData)[j * channels + 2];
				imgData[j * channels + 2] =
					((uint8_t*)image->pcData)[j * channels];
			}
			writeImage(width, height, channels, imgData, fpath);
			delete[] imgData;
		}
	}

	std::cout << "Model Materials Saved!" << std::endl;
}

void AssetLoader::loadTexture(const std::string& path) {
	namespace fs = std::filesystem;
	const std::string baked = path + ".bird";
	if (!m_forceRebuild && fs::exists(baked)) {
		std::error_code ec;
		if (fs::last_write_time(baked, ec) >= fs::last_write_time(path, ec) &&
			!ec) {
			return;
		}
	}
	// load texture
	int32_t width = 0, height = 0, channels = 0;
	std::cout << "Compiling Image: " << path << std::endl;

	// Load the image
	uint8_t* img = stbi_load(path.c_str(), &width, &height, &channels, 0);

	if (img == NULL) {
		throw std::runtime_error("Error in loading the image: " + path);
	}

	writeImage(width, height, channels, img, path);

	std::cout << "Image Compiled. Saving file" << std::endl;

	stbi_image_free(img);
}

void AssetLoader::writeImage(uint32_t width, uint32_t height, uint32_t channels,
							 uint8_t* imageData, const std::string& path) {
	const std::string finalPath = path + ".bird";
	const std::string tempPath = finalPath + ".tmp";
	if (!m_forceRebuild && std::filesystem::exists(finalPath)) {
		std::error_code ec;
		if (std::filesystem::exists(path, ec) && !ec &&
			std::filesystem::last_write_time(finalPath, ec) >=
				std::filesystem::last_write_time(path, ec) &&
			!ec) {
			return;
		}
	}
	std::fstream file(tempPath,
					  std::ios::out | std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open output texture file: " +
								 tempPath);
	}

	file.write((char*)&width, sizeof(int32_t));
	file.write((char*)&height, sizeof(int32_t));
	file.write((char*)&channels, sizeof(int32_t));
	file.write((char*)imageData,
			   static_cast<std::streamsize>(width) * height * channels *
				   sizeof(uint8_t));
	file.close();
	if (!file) {
		std::error_code ec;
		std::filesystem::remove(tempPath, ec);
		throw std::runtime_error("Failed writing texture file: " + finalPath);
	}
	std::filesystem::rename(tempPath, finalPath);

	std::cout << "Image saved." << std::endl;
}

namespace {
// SDF bake parameters. Distances are encoded so that ONEDGE marks the glyph
// outline and each texel step of PIXEL_DIST_SCALE is one pixel of distance
// at the baked size; PADDING texels of falloff surround every glyph.
constexpr float kFontBakedPixelHeight = 48.0f;
constexpr int kFontSdfPadding = 6;
constexpr unsigned char kFontSdfOnEdge = 128;
constexpr float kFontSdfPixelDistScale =
	static_cast<float>(kFontSdfOnEdge) / kFontSdfPadding;
constexpr uint32_t kFontAtlasWidth = 1024;
constexpr uint32_t kFontFirstCodepoint = 32;  // space
constexpr uint32_t kFontGlyphCount = 95;	  // through '~'
constexpr uint32_t kFontWhiteBlockSize = 4;	  // fully inside, for solid quads
constexpr uint32_t kFontGlyphGap = 1;

struct SdfBitmap {
	unsigned char* pixels = nullptr;
	int width = 0, height = 0, offsetX = 0, offsetY = 0;
};

uint32_t nextPowerOfTwo(uint32_t value) {
	uint32_t result = 1;
	while (result < value) result <<= 1;
	return result;
}
}  // namespace

void AssetLoader::loadFont(const std::string& path) {
	namespace fs = std::filesystem;
	const std::string baked = path + ".bird";
	if (!m_forceRebuild && fs::exists(baked)) {
		std::error_code ec;
		if (fs::last_write_time(baked, ec) >= fs::last_write_time(path, ec) &&
			!ec) {
			return;
		}
	}
	std::cout << "Compiling Font: " << path << std::endl;

	const std::string ttf = readFile(path);
	const auto* ttfData = reinterpret_cast<const unsigned char*>(ttf.data());
	stbtt_fontinfo info;
	if (ttf.empty() ||
		!stbtt_InitFont(&info, ttfData, stbtt_GetFontOffsetForIndex(ttfData, 0))) {
		throw std::runtime_error("Unable to parse font: " + path);
	}
	const float scale = stbtt_ScaleForPixelHeight(&info, kFontBakedPixelHeight);

	// Generate every glyph's distance field, then shelf-pack them in order.
	std::vector<SdfBitmap> bitmaps(kFontGlyphCount);
	for (uint32_t i = 0; i < kFontGlyphCount; i++) {
		SdfBitmap& bmp = bitmaps[i];
		bmp.pixels = stbtt_GetCodepointSDF(
			&info, scale, kFontFirstCodepoint + i, kFontSdfPadding,
			kFontSdfOnEdge, kFontSdfPixelDistScale, &bmp.width, &bmp.height,
			&bmp.offsetX, &bmp.offsetY);
	}

	std::vector<format::FontGlyph> glyphs(kFontGlyphCount);
	uint32_t penX = 0, penY = 0, shelfHeight = 0;
	auto place = [&](uint32_t width, uint32_t height) {
		if (width + 2 * kFontGlyphGap > kFontAtlasWidth) {
			throw std::runtime_error("Glyph wider than font atlas: " + path);
		}
		if (penX + width + kFontGlyphGap > kFontAtlasWidth) {
			penX = 0;
			penY += shelfHeight + kFontGlyphGap;
			shelfHeight = 0;
		}
		const uint32_t x = penX + kFontGlyphGap;
		const uint32_t y = penY + kFontGlyphGap;
		penX += width + kFontGlyphGap;
		shelfHeight = std::max(shelfHeight, height);
		return std::make_pair(x, y);
	};
	std::vector<std::pair<uint32_t, uint32_t>> origins(kFontGlyphCount);
	for (uint32_t i = 0; i < kFontGlyphCount; i++) {
		if (bitmaps[i].pixels) {
			origins[i] = place(bitmaps[i].width, bitmaps[i].height);
		}
	}
	const auto whiteOrigin = place(kFontWhiteBlockSize, kFontWhiteBlockSize);
	const uint32_t atlasHeight =
		nextPowerOfTwo(penY + shelfHeight + 2 * kFontGlyphGap);

	std::vector<uint8_t> atlas(kFontAtlasWidth * atlasHeight, 0);
	for (uint32_t i = 0; i < kFontGlyphCount; i++) {
		int advance = 0, leftBearing = 0;
		stbtt_GetCodepointHMetrics(&info, kFontFirstCodepoint + i, &advance,
								   &leftBearing);
		const SdfBitmap& bmp = bitmaps[i];
		format::FontGlyph& glyph = glyphs[i];
		glyph = {0, 0, 0, 0, 0.0f, 0.0f, advance * scale};
		if (!bmp.pixels) continue;  // whitespace: advance only

		const auto [x, y] = origins[i];
		for (int row = 0; row < bmp.height; row++) {
			std::memcpy(&atlas[(y + row) * kFontAtlasWidth + x],
						&bmp.pixels[row * bmp.width], bmp.width);
		}
		glyph.atlasX0 = static_cast<uint16_t>(x);
		glyph.atlasY0 = static_cast<uint16_t>(y);
		glyph.atlasX1 = static_cast<uint16_t>(x + bmp.width);
		glyph.atlasY1 = static_cast<uint16_t>(y + bmp.height);
		glyph.offsetX = static_cast<float>(bmp.offsetX);
		glyph.offsetY = static_cast<float>(bmp.offsetY);
		stbtt_FreeSDF(bmp.pixels, nullptr);
	}
	for (uint32_t row = 0; row < kFontWhiteBlockSize; row++) {
		std::memset(
			&atlas[(whiteOrigin.second + row) * kFontAtlasWidth + whiteOrigin.first],
			0xFF, kFontWhiteBlockSize);
	}

	int ascent = 0, descent = 0, lineGap = 0;
	stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);

	format::FontHeader header{};
	header.version = format::FONT_FORMAT_VERSION;
	header.bakedPixelHeight = kFontBakedPixelHeight;
	header.ascent = ascent * scale;
	header.descent = descent * scale;
	header.lineGap = lineGap * scale;
	header.atlasWidth = kFontAtlasWidth;
	header.atlasHeight = atlasHeight;
	header.firstCodepoint = kFontFirstCodepoint;
	header.glyphCount = kFontGlyphCount;
	header.whiteTexelX = static_cast<uint16_t>(whiteOrigin.first + 1);
	header.whiteTexelY = static_cast<uint16_t>(whiteOrigin.second + 1);

	const std::string finalFontPath = path + ".bird";
	const std::string tempFontPath = finalFontPath + ".tmp";
	std::fstream file(tempFontPath,
					  std::ios::out | std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<const char*>(&header), sizeof(header));
	file.write(reinterpret_cast<const char*>(glyphs.data()),
			   glyphs.size() * sizeof(format::FontGlyph));
	file.write(reinterpret_cast<const char*>(atlas.data()), atlas.size());
	file.close();
	std::filesystem::rename(tempFontPath, finalFontPath);

	std::cout << "Font saved (" << kFontAtlasWidth << "x" << atlasHeight
			  << " SDF atlas)." << std::endl;
}

void AssetLoader::loadSound(const std::string& path) {
	// load sound
}

void AssetLoader::loadShader(const std::string& path) {
	const std::string source = readFile(path);
	if (!m_forceRebuild && !bake::needsShaderBake(path, source)) {
		return;
	}
	std::cout << "Compiling Shader: " << path << std::endl;
	const std::string finalPath = path + ".bird";
	const std::string tempPath = finalPath + ".tmp";
	writeShaderBinaryFile(tempPath, bake::bakeShader(path, source));
	std::filesystem::rename(tempPath, finalPath);
}

void AssetLoader::loadMaterial(const std::string& path) {
	return;
	// load material
	// load model
	if (std::filesystem::exists(path + ".bird")) {
		return;
	}

	std::cout << "Compiling Material: " << path << std::endl;
	const aiScene* scene = nullptr;	 // importer.

	if (scene == nullptr) {
		std::cout << "Error loading scene" << std::endl;
		std::cout << importer.GetErrorString() << std::endl;
		return;
	}

	std::cout << "Material Compiled. Saving file" << std::endl;
	std::fstream file(path + ".bird", std::ios::out | std::ios::binary);
	file.close();
}

void AssetLoader::loadCustom(const std::string& path) {
	// load custom
}

std::string AssetLoader::readFile(const std::string& path) {
	std::ifstream file(path, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + path);
	}

	size_t fileSize = (size_t)file.tellg();
	std::string buffer;
	buffer.resize(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

}  // namespace bird
