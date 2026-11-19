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

#include "FormatHeaders.h"

namespace bird {

extern std::vector<std::vector<const char*>> supportedFileTypes;

class AssetLoader {
   public:
	AssetLoader();
	~AssetLoader();

	/**
	 * Compiles every supported asset under assetPath into .bird files,
	 * continuing past failures so that one run reports all of them.
	 * @return the number of assets that failed to compile.
	 */
	size_t init(std::string assetPath);

	void loadAsset();

	void loadScene(const std::string& path);
	void loadModel(const std::string& path);
	void loadTexture(const std::string& path);
	void loadSound(const std::string& path);
	/**
	 * Bakes a shader stage via bake::bakeShader when its .bird is stale.
	 * @throws bake::ShaderBakeError on any compile or rule violation.
	 */
	void loadShader(const std::string& path);
	void loadMaterial(const std::string& path);
	/**
	 * Bakes printable ASCII glyphs of a TrueType font into a .bird atlas.
	 * @param path - path to a .ttf or .otf file.
	 * @throws std::runtime_error when the font cannot be read or packed.
	 */
	void loadFont(const std::string& path);
	void loadCustom(const std::string& path);

	void writeImage(uint32_t width, uint32_t height, uint32_t channels,
					uint8_t* imageData, const std::string& path);

	// void writeMaterial(std::string& path, const aiMaterial* material);

	std::string readFile(const std::string& path);

   private:
	Assimp::Importer importer;
};
}  // namespace bird

#endif	// BIRD_ASSETLOADER_H
