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
#include <string>

#include "FormatHeaders.h"

namespace bird {

extern std::vector<std::vector<const char*>> supportedFileTypes;

/**
 * Offline asset processor and binary compiler.
 *
 * Scans directories for raw 3D models, textures, TrueType fonts, and GLSL shaders,
 * compiling them into optimized, engine-native `.bird` binary assets.
 */
class AssetLoader {
   public:
	AssetLoader();
	~AssetLoader();

	/**
	 * Compiles every supported asset under assetPath into .bird files,
	 * continuing past failures so that one run reports all of them.
	 * @return The number of assets that failed to compile.
	 */
	size_t init(std::string assetPath);

	void loadAsset();

	void loadScene(const std::string& path);

	/**
	 * Parses 3D model formats (.obj, .fbx, .glb) via Assimp and writes .bird models and materials.
	 */
	void loadModel(const std::string& path);

	/**
	 * Reads an image file and writes a serialized .bird texture file.
	 */
	void loadTexture(const std::string& path);

	void loadSound(const std::string& path);

	/**
	 * Bakes a shader stage via bake::bakeShader when its .bird is stale.
	 * @throws bake::ShaderBakeError on any compile or rule violation.
	 */
	void loadShader(const std::string& path);

	/**
	 * Serializes an Assimp material definition into a .bird material.
	 */
	void loadMaterial(const std::string& path);

	/**
	 * Bakes printable ASCII glyphs of a TrueType font into a .bird atlas.
	 * @param path Path to a .ttf or .otf file.
	 * @throws std::runtime_error when the font cannot be read or packed.
	 */
	void loadFont(const std::string& path);

	void loadCustom(const std::string& path);

	/**
	 * Encodes raw pixel data and writes an image file to disk.
	 */
	void writeImage(uint32_t width, uint32_t height, uint32_t channels,
					uint8_t* imageData, const std::string& path);

	std::string readFile(const std::string& path);

   private:
	Assimp::Importer importer;
};
}  // namespace bird

#endif	// BIRD_ASSETLOADER_H
