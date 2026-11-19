//
// Created by Seth Hahn on 2/23/24.
//

#include "AssetLoader.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "FormatHeaders.h"

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

std::unordered_map<std::string, shaderc_shader_kind> shader_map = {
	{".vert", shaderc_vertex_shader},
	{".frag", shaderc_fragment_shader},
	{".comp", shaderc_compute_shader},
	{".geom", shaderc_geometry_shader},
	{".tesc", shaderc_tess_control_shader},
	{".tese", shaderc_tess_evaluation_shader}};

AssetLoader::AssetLoader() {}
AssetLoader::~AssetLoader() {}

void AssetLoader::init(std::string assetPath) {
	// load assets from file

	for (const auto& entry :
		 std::filesystem::recursive_directory_iterator(assetPath)) {
		if (entry.is_directory()) {
			continue;
		}
		int i = 0;
		bool fileSupported = false;
		std::string filePath = entry.path().string();
		for (; i < supportedFileTypes.size(); i++) {
			for (const auto& ext : supportedFileTypes[i]) {
				std::string mp = entry.path().extension().string();
				std::transform(mp.begin(), mp.end(), mp.begin(),
							   [](unsigned char c) { return std::tolower(c); });
				if (mp == ext) {
					fileSupported = true;
					break;
				}
			}
			if (fileSupported) {
				break;
			}
		}
		if (!fileSupported) {
			i = -1;
		}

		// Check if generated asset exists / needs updating
		switch ((AssetType)i) {
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
				// Do nothing if file is not supported. This is not an error.
				// .txt, etc.
				break;
		}
	}
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

void AssetLoader::loadModel(const std::string& path) {
	// load model
	if (std::filesystem::exists(path + ".bird")) {
		return;
	}

	std::cout << "Compiling Model: " << path << std::endl;
	const aiScene* scene = importer.ReadFile(
		path, aiProcess_CalcTangentSpace | aiProcess_Triangulate |
				  aiProcess_SortByPType | aiProcess_JoinIdenticalVertices |
				  aiProcess_FixInfacingNormals |
				  aiProcess_PreTransformVertices);
	if (scene == nullptr) {
		std::cout << "Error loading scene" << std::endl;
		std::cout << importer.GetErrorString() << std::endl;
		return;
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
		file.write((char*)&scene->mMeshes[i]->mMaterialIndex,
				   sizeof(uint16_t));				 // material index
		file.write((char*)&zero, sizeof(uint32_t));	 // extra buffer data points

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

		// handle extra buffer data, it is currently 0 but we'll allow custom
		// data to be added later
	}

	file.close();
	std::cout << "File saved" << std::endl;
	aiColor3D eColor(0.f, 0.f, 0.f);

	// material order: diffuse, specular, ambient, shininess, opacity, emission,
	// reflectivity

	for (int i = 0; i < scene->mNumMaterials; i++) {
		std::fstream mf((path + std::to_string(i)) + ".mat.bird",
						std::ios::out | std::ios::binary);

		aiMaterial* aiMat = scene->mMaterials[i];
		DebugPrintAllTextures(aiMat);
		aiColor3D color(0.f, 0.f, 0.f);
		float shininess = 32, opacity = 1.0, reflectivity = 0.0, zero = 0;

		// Diffuse color
		if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
			mf.write((char*)&color.r, 3 * sizeof(float));
		} else {
			mf.write((char*)&eColor.r, 3 * sizeof(float));
		}

		// Specular color
		if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color)) {
			mf.write((char*)&color.r, 3 * sizeof(float));
		} else {
			eColor = aiColor3D(1.0);
			mf.write((char*)&eColor.r, 3 * sizeof(float));
			eColor = aiColor3D(0.0);
		}

		// Ambient color
		if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color)) {
			mf.write((char*)&color.r, 3 * sizeof(float));
		} else {
			eColor = aiColor3D(1.0);
			mf.write((char*)&eColor.r, 3 * sizeof(float));
			eColor = aiColor3D(0.0);
		}

		// Shininess
		if (AI_SUCCESS == aiMat->Get(AI_MATKEY_SHININESS, shininess)) {
			mf.write((char*)&shininess, sizeof(float));
		} else {
			mf.write((char*)&shininess, sizeof(float));
		}

		// Opacity
		if (AI_SUCCESS == aiMat->Get(AI_MATKEY_OPACITY, opacity)) {
			mf.write((char*)&opacity, sizeof(float));
		} else {
			mf.write((char*)&opacity, sizeof(float));
		}

		// Emission
		if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color)) {
			mf.write((char*)&color.r, 3 * sizeof(float));
		} else {
			mf.write((char*)&eColor.r, 3 * sizeof(float));
		}

		// Reflectivity
		if (AI_SUCCESS == aiMat->Get(AI_MATKEY_REFLECTIVITY, reflectivity)) {
			mf.write((char*)&reflectivity, sizeof(float));
		} else {
			mf.write((char*)&reflectivity, sizeof(float));
		}

		uint8_t totalTexCount = 0;	// max 256 textures per material
		for (unsigned int t = aiTextureType_DIFFUSE; t <= aiTextureType_UNKNOWN;
			 ++t) {
			aiTextureType texType = static_cast<aiTextureType>(t);
			totalTexCount += aiMat->GetTextureCount(texType);
		}

		mf.write((char*)&totalTexCount, sizeof(uint8_t));

		aiString apath;
		for (unsigned int t = aiTextureType_DIFFUSE; t <= aiTextureType_UNKNOWN;
			 ++t) {
			aiTextureType texType = static_cast<aiTextureType>(t);
			unsigned int texCount = aiMat->GetTextureCount(texType);
			for (unsigned int i = 0; i < texCount; ++i) {
				if (AI_SUCCESS == aiMat->GetTexture(texType, i, &apath)) {
					std::string np = apath.C_Str();
					auto fp = std::filesystem::path(np);
					if (!fp.has_extension()) {
						np = std::filesystem::path(path).filename().string() +
							 "_etex" + fp.filename().string().substr(1);
					}
					uint16_t npl = np.size();
					mf.write((char*)&npl, sizeof(uint16_t));
					mf.write((char*)np.c_str(), npl);
				}
			}
		}

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
	if (std::filesystem::exists(path + ".bird")) {
		return;
	}
	// load texture
	int32_t width, height, channels;
	std::cout << "Compiling Image: " << path << std::endl;

	// Load the image
	uint8_t* img = stbi_load(path.c_str(), &width, &height, &channels, 0);

	if (img == NULL) {
		std::cout << "Error in loading the image: " << path << std::endl;
	}

	writeImage(width, height, channels, img, path);

	std::cout << "Image Compiled. Saving file" << std::endl;

	stbi_image_free(img);
}

void AssetLoader::writeImage(uint32_t width, uint32_t height, uint32_t channels,
							 uint8_t* imageData, const std::string& path) {
	if (std::filesystem::exists(path + ".bird")) {
		return;
	}
	std::fstream file(path + ".bird", std::ios::out | std::ios::binary);

	file.write((char*)&width, sizeof(int32_t));
	file.write((char*)&height, sizeof(int32_t));
	file.write((char*)&channels, sizeof(int32_t));
	file.write((char*)imageData, width * height * channels * sizeof(uint8_t));

	file.close();

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
	if (std::filesystem::exists(path + ".bird")) {
		return;
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

	std::fstream file(path + ".bird", std::ios::out | std::ios::binary);
	file.write(reinterpret_cast<const char*>(&header), sizeof(header));
	file.write(reinterpret_cast<const char*>(glyphs.data()),
			   glyphs.size() * sizeof(format::FontGlyph));
	file.write(reinterpret_cast<const char*>(atlas.data()), atlas.size());
	file.close();

	std::cout << "Font saved (" << kFontAtlasWidth << "x" << atlasHeight
			  << " SDF atlas)." << std::endl;
}

void AssetLoader::loadSound(const std::string& path) {
	// load sound
}

void AssetLoader::loadShader(const std::string& path) {
	// load shader

	std::filesystem::path _newPath = std::filesystem::path(path);
	std::string npath = _newPath.parent_path();
	npath += _newPath.stem();
	if (std::filesystem::exists(npath + ".bird")) {
		return;
	}
	std::vector<format::ShaderStage> stages;
	std::vector<format::ShaderGPHeader> gpHeader;
	std::vector<format::ShaderUniformInfo> uniformInfo;
	std::vector<std::string> names;
	std::unordered_map<ShaderGraphicsPipeline, std::vector<uint32_t>>
		shaderData;

	std::fstream os(npath, std::ios::out | std::ios::binary);
	// writeShaderInformation(npath, _newPath.extension(), gpHeader, stages,
	//					   uniformInfo, shaderData, names);

	/*
	 * ShaderHeader
	 * ShaderGPHeader[]
	 * ShaderStage[]
	 * ShaderUniformInfo[]
	 * uint8_t* uniformNames;
	 * uint32_t* shaderData;
	 */
}

void AssetLoader::writeShaderInformation(
	const std::string& path, std::string ext,
	std::vector<format::ShaderGPHeader>& gpHeader,
	std::vector<format::ShaderStage>& stages,
	std::vector<std::pair<std::string, format::ShaderUniformInfo>>& uniformInfo,
	std::unordered_map<ShaderGraphicsPipeline, std::vector<uint32_t>>&
		shaderData,
	std::vector<std::string>& names) {
	/*shaderc_shader_kind kind =
		shader_map[std::filesystem::path(path).extension()];

	std::string source = readFile(path);

	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	// We need to keep it at debug or we lose variable names
	options.SetOptimizationLevel(shaderc_optimization_level_zero);

	auto result =
		compiler.CompileGlslToSpv(source, kind, path.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << result.GetErrorMessage();
		return;
	}
	std::vector<uint32_t> spirv_binary(result.cbegin(), result.cend());

	spirv_cross::CompilerGLSL glsl_compiler(spirv_binary);

	spirv_cross::CompilerGLSL::Options gl_options;
	gl_options.version = 450;  // maybe 330 depending on the target
	gl_options.es = false;
	glsl_compiler.set_common_options(gl_options);

	std::string opengl_source = glsl_compiler.compile();

	spirv_cross::ShaderResources resources =
		glsl_compiler.get_shader_resources();

	// Handle Push Constants (Flattened to Default Uniforms in GL)
	for (auto& res : resources.push_constant_buffers) {
		const auto& type = glsl_compiler.get_type(res.base_type_id);
		for (uint32_t i = 0; i < type.member_types.size(); i++) {
			format::ShaderUniformInfo info{};
			std::string name =
				glsl_compiler.get_member_name(res.base_type_id, i);
			info.nameSize = (uint8_t)(name.size() + 1);
			info.type = format::UNIFORM;
			info.bindingLocation = (uint8_t)glsl_compiler.get_decoration(
				res.id, spv::DecorationLocation);
			uniformInfo.push_back({name, info});
		}
	}

	// UBOs
	for (auto& res : resources.uniform_buffers) {
		format::ShaderUniformInfo info{};
		info.nameSize = (uint8_t)(res.name.size() + 1);
		info.type = format::UBO;
		info.bindingLocation = (uint8_t)glsl_compiler.get_decoration(
			res.id, spv::DecorationBinding);
		uniformInfo.push_back({res.name, info});
	}

	// 3. Handle Textures
	for (auto& res : resources.sampled_images) {
		format::ShaderUniformInfo info{};
		info.nameSize = (uint8_t)(res.name.size() + 1);
		info.type = format::TEXTURE;
		info.bindingLocation = (uint8_t)glsl_compiler.get_decoration(
			res.id, spv::DecorationBinding);
		uniformInfo.push_back({res.name, info});
	}

	format::ShaderStage stageHeader;
	stageHeader.stage = (uint8_t)kind;
	stageHeader.uniformCount = (uint8_t)finalUniforms.size();
	stageHeader.dataSize = (uint32_t)source.size();*/
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
