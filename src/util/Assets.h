//
// Created by student on 2/13/24.
//

#ifndef BIRD_ASSETS_H
#define BIRD_ASSETS_H

#include <renders/GraphicsPipeline.h>
#include <renders/generic/Buffer.h>
#include <renders/generic/Texture.h>
#include <renders/opengl/GLBuffer.h>

#include <fstream>

namespace bird {

#define LETTER_OFFSET 26 + 26 + 2 + 1 + 10

/**
 * Manages serialized asset indexing and incremental asset map metadata.
 */
class AssetsLoader {
   public:
	AssetsLoader() = default;
	~AssetsLoader() = default;

	void init();

	/**
	 * Flushes tracked asset metadata and binary records to the filesystem.
	 */
	void writeAssets();

	void loadAsset();

   private:
	/**
	 * Constructs the asset map mapping asset IDs to file offsets and hashcodes.
	 */
	void createAssetMap();

	/**
	 * Compares the cached asset map against filesystem timestamps and hashes
	 * to detect stale or missing assets.
	 */
	void updateAssetMap();
	void loadAssetMap();
};

/**
 * Global convenience wrappers for asset loading via the active ResourceManager.
 */
namespace Assets {
/** Loads a texture via bird::RESOURCE_MANAGER. */
std::shared_ptr<Texture> loadTexture(std::string path);
/** Loads a model mesh collection via bird::RESOURCE_MANAGER. */
std::vector<std::shared_ptr<Mesh>> loadMesh(std::string path);
void loadSound(std::string path);
void loadShader(std::string path);
/** Loads a material definition via bird::RESOURCE_MANAGER. */
std::shared_ptr<Material> loadMaterial(std::string path);
void loadCustom(std::string path);

}  // namespace Assets

}  // namespace bird

#endif	// BIRD_ASSETS_H
