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
#include "../renders/generic/Material.h"
#include "../build-tools/MaterialFormat.h"
#include "Resource.h"
#include "../animation/skeletal/SkeletalAsset.h"

namespace bird {

/**
 * Discriminator identifying the category of a loaded engine asset.
 */
enum AssetType {
	ASSET_TYPE_TEXTURE,
	ASSET_TYPE_SCENE,
	ASSET_TYPE_SOUND,
	ASSET_TYPE_SHADER,
	ASSET_TYPE_MATERIAL,
	ASSET_TYPE_FONT,
	ASSET_TYPE_CUSTOM,
	ASSET_TYPE_MESH
};

/**
 * Central asset cache and resource management subsystem.
 *
 * Provides thread-safe / centralized loading and lifetime caching for textures,
 * 3D meshes, materials, fonts, and shaders. Deduplicates resource loads by
 * canonical file path. Resolves relative asset paths against RESOURCE_PATH.
 */
class ResourceManager {
   public:
	ResourceManager();
	~ResourceManager();

	/** Initializes internal resource cache maps. */
	void init();

	/**
	 * Loads a 2D image texture from disk, caching the result.
	 * @param path File path to the image or .bird texture asset.
	 * @return Shared pointer to the GPU Texture.
	 */
	std::shared_ptr<Texture> loadTexture(const std::string& path);

	/**
	 * Loads a texture holding color data authored in sRGB (albedo, emissive),
	 * so that shader sampling returns linear values. Cached apart from
	 * loadTexture() because the GPU storage differs.
	 * @param path File path to the baked .bird texture asset.
	 * @return Shared pointer to the GPU Texture, or nullptr when missing.
	 */
	std::shared_ptr<Texture> loadColorTexture(const std::string& path);

	/**
	 * Loads a 3D model, decomposing it into individual meshes and caching them.
	 * @param path File path to the 3D model (.obj, .glb, .fbx).
	 * @return Vector of shared Mesh instances.
	 */
	std::vector<std::shared_ptr<Mesh>> loadMesh(const std::string& path);

	/**
	 * Compiles a shader program, or returns the cached program with the same
	 * ordered stage files.
	 * @param builder Stage configuration; see ShaderBuilder::cacheKey().
	 * @return Shared program.
	 * @throws std::logic_error when the builder holds no stage.
	 */
	std::shared_ptr<Shader> loadShader(const ShaderBuilder& builder);

	/**
	 * Loads a vertex/fragment program; equivalent to the builder overload.
	 * @param vertexPath Vertex stage path.
	 * @param fragmentPath Fragment stage path.
	 * @return Shared program.
	 */
	std::shared_ptr<Shader> loadShader(const std::string& vertexPath,
									   const std::string& fragmentPath);

	/**
	 * Loads material parameters and texture maps from disk.
	 */
	std::shared_ptr<Material> loadMaterial(const std::string& path);

	/**
	 * Loads a font baked by BuildTools from <path>.bird; cached per path.
	 * @param path Source font path, e.g. "assets/fonts/DejaVuSans.ttf".
	 * @return The loaded font, or nullptr when the baked file is missing.
	 * @throws std::runtime_error when the baked file is truncated.
	 */
	std::shared_ptr<Font> loadFont(const std::string& path);

	/**
	 * Loads the skeleton, clips and skins BuildTools baked for a skinned
	 * model (<path>.skel.bird); cached per path and shared by instances.
	 * @param path Source model path, e.g. "assets/models/hero.glb".
	 * @return The asset, or nullptr when the model has no skeleton bake.
	 * @throws std::runtime_error when the baked file is malformed.
	 */
	BIRD_NO_EXPORT std::shared_ptr<const SkeletalAsset> loadSkeletalAsset(
		const std::string& path);

	void loadSound(std::string path);
	void loadCustom(std::string path);

	/**
	 * Adds an instantiated resource manually to the cache under a specific key.
	 * @param path Asset identifier or path key.
	 * @param resource Shared resource instance.
	 * @param type Asset classification.
	 */
	void addResource(std::string path, std::shared_ptr<Resource> resource,
					 AssetType type);

	/**
	 * Registers a pre-loaded mesh collection under the given path key.
	 */
	void addMesh(std::string path, std::vector<std::shared_ptr<Mesh>> meshes);

	/**
	 * Returns the default fallback standard surface shader.
	 */
	std::shared_ptr<Shader> getDefaultShader();

	/** Root working directory prepended to relative asset paths. */
	static inline std::string RESOURCE_PATH = "";

   private:
	std::shared_ptr<Texture> loadTextureWithColorSpace(const std::string& path,
													   bool srgb);

	std::shared_ptr<Shader> m_defaultShader;
	std::unordered_map<std::string, std::shared_ptr<Resource>>* m_resources;
	std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>>
		m_meshes;
	std::unordered_map<std::string, std::shared_ptr<const SkeletalAsset>>
		m_skeletalAssets;
};

}  // namespace bird

#endif	// BIRD_RESOURCEMANAGER_H
