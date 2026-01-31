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
 * The type of asset that is being loaded.
 */

class AssetsLoader {
   public:
	AssetsLoader() = default;
	~AssetsLoader() = default;

	void init();

	/**
	 * Write loaded assets to the filesystem.
	 */
	void writeAssets();

	void loadAsset();

   private:
	/**
	 * Create a map of assets. Where each file
	 * has a structure but in binary such as:
	 * each asset has a pair of (uint8_t, uint64_t, uint8_t)
	 * where the uint8_t is the type of asset and
	 * the uint64_t is the id of the asset. The last
	 * uint8_t is the hashcode of the asset.
	 *
	 * We begin reading each pair of (uint8_t, uint64_t),
	 * there will be no spaces between the pairs and size
	 * will be strictly enforced.
	 *
	 * Files will be saved as a .bird file where the file
	 * name is the id of the asset.
	 */
	void createAssetMap();

	/**
	 * We load the current map into memory and check against
	 * the loaded assets to see if there are any new assets.
	 * We have to be careful here in case the user has deleted
	 * or changed any assets without the program's knowing. This
	 * could completely break the assets.
	 */
	void updateAssetMap();
	void loadAssetMap();
};

namespace Assets {
std::shared_ptr<Texture> loadTexture(std::string path);
std::vector<std::shared_ptr<Mesh>> loadMesh(std::string path);
void loadSound(std::string path);
void loadShader(std::string path);
std::shared_ptr<Material> loadMaterial(std::string path);
void loadCustom(std::string path);

}  // namespace Assets

}  // namespace bird

#endif	// BIRD_ASSETS_H
