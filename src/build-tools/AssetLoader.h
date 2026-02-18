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
#include <vector>

namespace bird {

extern std::vector<std::vector<const char*>> supportedFileTypes;

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
	// void writeMaterial(std::string& path, const aiMaterial* material);

   private:
	Assimp::Importer importer;
};
}  // namespace bird

#endif	// BIRD_ASSETLOADER_H
