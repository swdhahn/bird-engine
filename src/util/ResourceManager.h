//
// Created by student on 3/8/24.
//

#ifndef BIRD_RESOURCEMANAGER_H
#define BIRD_RESOURCEMANAGER_H

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include "../renders/GraphicsConstants.h"
#include "../renders/generic/Buffer.h"
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
	TEXTURE,
	SCENE,
	SOUND,
	SHADER,
	MATERIAL,
	CUSTOM,
	// The above are included in the m_resources variable in the ResourceManager
	// class.
	MESH
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

   private:
	std::unordered_map<std::string, std::shared_ptr<Resource>>* m_resources;
	std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>>
		m_meshes;
};

}  // namespace bird

#endif	// BIRD_RESOURCEMANAGER_H
