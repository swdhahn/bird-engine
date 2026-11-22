#ifndef BIRD_SERIALIZATION_PREFAB_MANAGER_H
#define BIRD_SERIALIZATION_PREFAB_MANAGER_H

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../scene/Scene.h"
#include "../scene/WorldObject.h"
#include "../util/Constants.h"
#include "../util/Memory.hpp"
#include "Prefab.h"
#include "PropertyOverride.h"

namespace bird {

/**
 * Central asset cache and instantiation service for Prefab archetypes.
 *
 * Manages loaded Prefab assets keyed by AssetID (e.g. `"prefabs/enemy"` or
 * `"assets/prefabs/turret.prefab.json"`), supports loading through Bird's VFS
 * or direct filesystem paths, and provides `instantiatePrefab()` to spawn
 * runtime entity subtrees with automatic UUID & EntityRef remapping, override
 * application, and component/script `init()` triggering.
 */
class PrefabManager {
   public:
	PrefabManager() = default;
	~PrefabManager() = default;

	/** Returns the process-wide singleton PrefabManager instance. */
	static PrefabManager* get();

	/**
	 * Registers a pre-built Prefab asset under `assetId`.
	 * Overwrites any existing prefab with the same AssetID.
	 */
	BIRD_NO_EXPORT void registerPrefab(const AssetID& assetId,
									   _ref<Prefab> prefab);

	/**
	 * Snapshots a live entity subtree and registers it as a reusable Prefab.
	 * @param assetId AssetID under which to register the prefab.
	 * @param rootEntity Live root entity to capture (must be non-null).
	 * @return True if the prefab was captured and registered.
	 */
	bool registerPrefabFromEntity(const AssetID& assetId,
								  WorldObject* rootEntity);

	/**
	 * Loads a Prefab JSON file (via platform VFS or direct path) and caches it
	 * under `assetId`.
	 * @param assetId AssetID key to associate with the loaded prefab.
	 * @param filePath Relative VFS path or filesystem path to the prefab JSON file.
	 * @return True if loaded and parsed successfully.
	 */
	bool loadPrefabFromFile(const AssetID& assetId,
							const std::string& filePath);

	/**
	 * Parses a Prefab from a JSON string and caches it under `assetId`.
	 * @param assetId AssetID key to associate with the prefab.
	 * @param jsonText JSON text describing the prefab.
	 * @return True if parsed successfully.
	 */
	bool loadPrefabFromJson(const AssetID& assetId,
							const std::string& jsonText);

	/**
	 * Writes a registered Prefab to a JSON file on disk.
	 * @param assetId AssetID of a registered prefab.
	 * @param filePath Destination file path.
	 * @return True if the prefab existed and was written successfully.
	 */
	bool savePrefabToFile(const AssetID& assetId,
						  const std::string& filePath) const;

	/** Returns true if a prefab with `assetId` is currently registered. */
	bool hasPrefab(const AssetID& assetId) const;

	/** Returns the cached Prefab for `assetId`, or nullptr if not found. */
	BIRD_NO_EXPORT _ref<Prefab> getPrefab(const AssetID& assetId) const;

	/** Removes a single prefab from the cache. */
	void unregisterPrefab(const AssetID& assetId);

	/** Removes all cached prefabs. */
	void clear();

	/**
	 * Instantiates a Prefab into `parentScene`.
	 *
	 * Allocates fresh runtime UUIDs for every entity in the prefab subtree,
	 * remaps all internal EntityRef and parent-child references to the newly
	 * spawned IDs, attaches PrefabInstanceComponent metadata, attaches the
	 * root entity to `parentScene`, and triggers `init()` on all components
	 * and Dart scripts.
	 *
	 * If `assetId` is not yet cached, attempts to load it from `assetId` as a
	 * VFS/file path automatically.
	 *
	 * @param assetId AssetID of the prefab to instantiate.
	 * @param parentScene Target Scene receiving ownership of the spawned root.
	 * @return Non-owning pointer to the newly spawned root entity, or nullptr
	 *         if the prefab could not be found or `parentScene` is null.
	 */
	WorldObject* instantiatePrefab(const AssetID& assetId, Scene* parentScene);

	/**
	 * Instantiates a Prefab into `parentScene` and applies `overrides` on top
	 * of the base prefab BEFORE component/script `init()` executes.
	 *
	 * @param assetId AssetID of the prefab to instantiate.
	 * @param parentScene Target Scene receiving ownership of the spawned root.
	 * @param overrides List of PropertyOverride deltas to apply before init().
	 * @param forcedRootUuid Optional fixed runtime UUID for the spawned root
	 *                       (used by SceneSerializer to preserve scene-level
	 *                       instance UUIDs across save/load).
	 * @return Non-owning pointer to the spawned root entity, or nullptr on error.
	 */
	BIRD_NO_EXPORT WorldObject* instantiatePrefabWithOverrides(
		const AssetID& assetId, Scene* parentScene,
		const std::vector<PropertyOverride>& overrides,
		std::optional<UUID> forcedRootUuid = std::nullopt);

	/**
	 * Instantiates a Prefab as a detached owned subtree (not yet added to a
	 * parent Scene). Internal references are remapped and `overrides` are
	 * applied; component `init()` on the root runs when the caller passes the
	 * returned object to `Scene::addChild()`.
	 */
	BIRD_NO_EXPORT _own<WorldObject> instantiatePrefabDetached(
		const AssetID& assetId,
		const std::vector<PropertyOverride>& overrides = {},
		std::optional<UUID> forcedRootUuid = std::nullopt);

   private:
	_ref<Prefab> resolvePrefab(const AssetID& assetId);

	std::unordered_map<AssetID, _ref<Prefab>> m_prefabs;
};

/**
 * Convenience top-level function instantiating a Prefab into `parentScene`
 * via the global PrefabManager singleton. Exposed directly to Dart FFI.
 *
 * @param assetId AssetID of the prefab to instantiate.
 * @param parentScene Target Scene receiving ownership of the spawned entity.
 * @return Non-owning pointer to the spawned root entity, or nullptr on failure.
 */
WorldObject* InstantiatePrefab(const AssetID& assetId, Scene* parentScene);

}  // namespace bird

#endif	// BIRD_SERIALIZATION_PREFAB_MANAGER_H
