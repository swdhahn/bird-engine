//
// Created by student on 3/8/24.
//

#ifndef BIRD_RESOURCEMANAGER_H
#define BIRD_RESOURCEMANAGER_H

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

#include "../renders/GraphicsConstants.h"
#include "../renders/generic/Buffer.h"
#include "../renders/generic/Font.h"
#include "../renders/generic/Mesh.h"
#include "../renders/generic/Shader.h"
#include "../renders/generic/Texture.h"
#include "Resource.h"

/*
 * This Resource class includes textures, models, sounds,
 * shaders, and materials.
 *
 * The Resource class is responsible for loading and
 * unloading assets. Also storing assets in an optimized
 * way for both file storage and memory storage.
 */

namespace bird {

enum AssetType {
	ASSET_TYPE_TEXTURE,
	ASSET_TYPE_SCENE,
	ASSET_TYPE_SOUND,
	ASSET_TYPE_SHADER,
	ASSET_TYPE_MATERIAL,
	ASSET_TYPE_FONT,
	ASSET_TYPE_CUSTOM,
	// The above are included in the m_resources variable in the ResourceManager
	// class.
	ASSET_TYPE_MESH
};

class ResourceManager {
   public:
	ResourceManager();
	~ResourceManager();

	void init();

	std::shared_ptr<Texture> loadTexture(std::string path);
	std::vector<std::shared_ptr<Mesh>> loadMesh(std::string path);
	std::shared_ptr<Shader> loadShader(ShaderBuilder& builder);
	std::shared_ptr<Material> loadMaterial(std::string path);
	/**
	 * Loads a font baked by BuildTools from <path>.bird; cached per path.
	 * @param path - source font path, e.g. "assets/fonts/DejaVuSans.ttf".
	 * @return the font, or nullptr when the baked file is missing.
	 * @throws std::runtime_error when the baked file is truncated.
	 */
	std::shared_ptr<Font> loadFont(std::string path);
	void loadSound(std::string path);
	void loadCustom(std::string path);

	/**
	 * Add a resource to the resource manager.
	 *
	 * @param path The path to the resource. Acts as a name.
	 * @param resource The resource to add.
	 * @param type The type of resource.
	 */
	void addResource(std::string path, std::shared_ptr<Resource> resource,
					 AssetType type);
	void addMesh(std::string path, std::vector<std::shared_ptr<Mesh>> meshes);

	std::shared_ptr<Shader> getDefaultShader();
	static inline std::string RESOURCE_PATH = "";

   private:
	std::shared_ptr<Shader> m_defaultShader;
	std::unordered_map<std::string, std::shared_ptr<Resource>>* m_resources;
	std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>>
		m_meshes;
};

}  // namespace bird

#endif	// BIRD_RESOURCEMANAGER_H
