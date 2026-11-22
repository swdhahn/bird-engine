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
#ifdef BIRD_ENABLE_HOT_RELOAD
#include "../hotreload/HotReloadTypes.h"
#endif

namespace bird {

#ifdef BIRD_ENABLE_HOT_RELOAD
namespace hotreload {
class HotReloadManager;
}
#endif

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
	ASSET_TYPE_MESH,
	ASSET_TYPE_PREFAB
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

	/**
	 * Enables or disables automatic fallback assets (magenta checkerboard
	 * texture, unlit fallback material, and unit cube mesh) when an asset
	 * fails to load. Enabled by default.
	 */
	void setUseFallbackAssets(bool enabled) { m_useFallbackAssets = enabled; }

	/** Returns true if missing/corrupt assets return graceful fallbacks. */
	bool usesFallbackAssets() const { return m_useFallbackAssets; }

	/**
	 * Returns the lazily created magenta/black checkerboard fallback texture.
	 * @param srgb When true, returns the sRGB variant for color slots.
	 */
	std::shared_ptr<Texture> getFallbackTexture(bool srgb = false);

	/** Returns the lazily created fallback material using the checkerboard texture. */
	std::shared_ptr<Material> getFallbackMaterial();

	/** Returns the lazily created fallback 1x1x1 unit cube mesh. */
	std::shared_ptr<Mesh> getFallbackMesh();

	/**
	 * Reloads a cached texture (both linear and sRGB variants if cached) from
	 * its freshly compiled `.bird` binary and swaps GPU handles in place.
	 * If loading fails, logs an error and keeps the existing texture active.
	 *
	 * @param path Source asset path key (e.g. "assets/textures/wood.png").
	 * @return True if at least one cached texture variant was reloaded.
	 */
	BIRD_NO_EXPORT bool reloadTexture(const std::string& path);

	/**
	 * Rebuilds and swaps every cached Shader program that uses `stagePath`
	 * (e.g. "assets/shaders/gbuffer.frag"). If compilation or linking fails,
	 * logs the error and preserves the previously linked GPU program.
	 *
	 * @param stagePath Source shader stage path.
	 * @return Number of cached shader programs reloaded.
	 */
	BIRD_NO_EXPORT size_t reloadShaderStage(const std::string& stagePath);

	/**
	 * Rebuilds and swaps all cached Shader programs from their `.bird` binaries.
	 * Used when a shared `.glsl` include file changes.
	 *
	 * @return Number of cached shader programs successfully reloaded.
	 */
	BIRD_NO_EXPORT size_t reloadAllShaders();

	/**
	 * Reloads a cached Material from its `.bird` binary and swaps parameters
	 * and texture bindings in place.
	 *
	 * @param path Material asset path key.
	 * @return True if the cached material was reloaded.
	 */
	BIRD_NO_EXPORT bool reloadMaterial(const std::string& path);

	/**
	 * Reloads a cached Font from its `.bird` binary and swaps glyph metrics
	 * and SDF atlas texture handles in place.
	 *
	 * @param path Source font path key (e.g. "assets/fonts/DejaVuSans.ttf").
	 * @return True if the cached font was reloaded.
	 */
	BIRD_NO_EXPORT bool reloadFont(const std::string& path);

#ifdef BIRD_ENABLE_HOT_RELOAD
	/**
	 * Starts the background hot-reload pipeline (FileWatcher + BuildToolsRunner).
	 * Only compiled in development builds (`BIRD_ENABLE_HOT_RELOAD`).
	 *
	 * @param config Optional hot-reload configuration (watch directory,
	 *        BuildTools executable path, debounce interval).
	 * @return True if watching started successfully.
	 */
	BIRD_NO_EXPORT bool enableHotReload(
		const hotreload::HotReloadConfig& config = {});

	/**
	 * Stops the background hot-reload pipeline and releases watcher resources.
	 */
	BIRD_NO_EXPORT void disableHotReload();

	/**
	 * Polls the thread-safe reload queue on the main thread, loading newly
	 * compiled binary assets and swapping their CPU/GPU handles in place.
	 * Called once per frame by Application::tick().
	 *
	 * @return Number of asset reload requests processed this frame.
	 */
	BIRD_NO_EXPORT size_t pollHotReloads();

	/**
	 * Registers a main-thread callback invoked when a watched `.dart` script
	 * changes on disk.
	 *
	 * @param callback Function receiving the normalized `.dart` file path.
	 */
	BIRD_NO_EXPORT void setDartReloadCallback(
		hotreload::DartReloadCallback callback);

	/** @return Active HotReloadManager, or nullptr if hot-reload is disabled. */
	BIRD_NO_EXPORT hotreload::HotReloadManager* getHotReloadManager() const;
#else
	/** No-op in release/shipping builds where hot-reload is compiled out. */
	BIRD_NO_EXPORT size_t pollHotReloads() { return 0; }
#endif

	/** Root working directory prepended to relative asset paths. */
	static inline std::string RESOURCE_PATH = "";

   private:
	std::shared_ptr<Texture> loadTextureWithColorSpace(const std::string& path,
													   bool srgb);
	std::shared_ptr<Texture> buildTextureFromBaked(const std::string& path,
												   bool srgb);
	std::shared_ptr<Material> buildMaterialFromBaked(const std::string& path);
	std::shared_ptr<Font> buildFontFromBaked(const std::string& path);

	std::shared_ptr<Shader> m_defaultShader;
	std::shared_ptr<Texture> m_fallbackTextureLinear;
	std::shared_ptr<Texture> m_fallbackTextureSrgb;
	std::shared_ptr<Material> m_fallbackMaterial;
	std::shared_ptr<Mesh> m_fallbackMesh;
	bool m_useFallbackAssets = true;
	std::unordered_map<std::string, std::shared_ptr<Resource>>* m_resources;
	std::unordered_map<std::string, ShaderBuilder> m_shaderBuilders;
	std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>>
		m_meshes;
	std::unordered_map<std::string, std::shared_ptr<const SkeletalAsset>>
		m_skeletalAssets;
#ifdef BIRD_ENABLE_HOT_RELOAD
	std::unique_ptr<hotreload::HotReloadManager> m_hotReloadManager;
	hotreload::DartReloadCallback m_dartReloadCallback;
#endif
};

}  // namespace bird

#endif	// BIRD_RESOURCEMANAGER_H
