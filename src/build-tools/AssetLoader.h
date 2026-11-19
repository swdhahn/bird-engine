//
// Created by Seth Hahn on 2/23/24.
//

#ifndef BIRD_ASSETLOADER_H
#define BIRD_ASSETLOADER_H

#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_common.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <vector>

#include "FormatHeaders.h"

namespace bird {

enum ShaderGraphicsPipeline { SHADER_OPENGL, SHADER_VULKAN, SHADER_METAL };

extern std::vector<std::vector<const char*>> supportedFileTypes;

extern std::unordered_map<std::string, shaderc_shader_kind> shader_map;

class AssetLoader {
   public:
	AssetLoader();
	~AssetLoader();

	void init(std::string assetPath);

	void loadAsset();

	void loadScene(const std::string& path);
	void loadModel(const std::string& path);
	void loadTexture(const std::string& path);
	void loadSound(const std::string& path);
	void loadShader(const std::string& path);
	void loadMaterial(const std::string& path);
	void loadCustom(const std::string& path);

	void writeImage(uint32_t width, uint32_t height, uint32_t channels,
					uint8_t* imageData, const std::string& path);
	void writeShaderInformation(
		const std::string& path, std::string ext,
		std::vector<format::ShaderGPHeader>& gpHeader,
		std::vector<format::ShaderStage>& stages,
		std::vector<std::pair<std::string, format::ShaderUniformInfo>>&
			uniformInfo,
		std::unordered_map<ShaderGraphicsPipeline, std::vector<uint32_t>>&
			shaderData,
		std::vector<std::string>& names);

	// void writeMaterial(std::string& path, const aiMaterial* material);

	std::string readFile(const std::string& path);

   private:
	Assimp::Importer importer;
};
}  // namespace bird

#endif	// BIRD_ASSETLOADER_H
