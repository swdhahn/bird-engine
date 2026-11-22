#ifndef BIRD_SERIALIZATION_PREFAB_INSTANCE_COMPONENT_H
#define BIRD_SERIALIZATION_PREFAB_INSTANCE_COMPONENT_H

#include <string>
#include <vector>

#include "../scene/components/Component.h"
#include "PropertyOverride.h"
#include "UUID.h"

namespace bird {

/** Canonical string asset identifier (e.g. "assets/prefabs/enemy.prefab.json"). */
using AssetID = std::string;

/**
 * Lightweight metadata component attached to entities instantiated from a Prefab.
 *
 * Tracks the originating `prefab_asset_id`, the entity's stable local UUID
 * within that prefab, whether this entity is the root of the prefab instance,
 * and any recorded PropertyOverride deltas.
 *
 * When SceneSerializer saves a Scene containing a root PrefabInstanceComponent,
 * it emits only the `prefab_asset_id`, instance `uuid`, and property deltas
 * (overrides) rather than duplicating the full prefab subtree.
 */
class PrefabInstanceComponent : public Component {
   public:
	/**
	 * Constructs prefab instance metadata.
	 * @param prefabAssetId AssetID of the source Prefab.
	 * @param sourceLocalUuid Stable local UUID of this entity within the prefab.
	 * @param isInstanceRoot True for the root entity of the spawned instance.
	 */
	PrefabInstanceComponent(AssetID prefabAssetId, UUID sourceLocalUuid,
							bool isInstanceRoot);

	~PrefabInstanceComponent() override = default;

	void init() override {}
	void deinit() override {}

	/** Returns the AssetID of the source Prefab. */
	const AssetID& getPrefabAssetId() const noexcept { return m_prefabAssetId; }

	/** Returns this entity's stable local UUID inside the source Prefab. */
	const UUID& getSourceLocalUuid() const noexcept { return m_sourceLocalUuid; }

	/** Returns true if this entity is the root of the instantiated prefab. */
	bool isInstanceRoot() const noexcept { return m_isInstanceRoot; }

	/** Returns explicit PropertyOverride deltas stored on this instance root. */
	const std::vector<PropertyOverride>& getOverrides() const noexcept {
		return m_overrides;
	}

	std::vector<PropertyOverride>& getOverrides() noexcept {
		return m_overrides;
	}

	/** Replaces the recorded override list. */
	void setOverrides(std::vector<PropertyOverride> overrides) {
		m_overrides = std::move(overrides);
	}

	/**
	 * Adds or updates a PropertyOverride matching the same target entity,
	 * component, and property path.
	 */
	void setOverride(PropertyOverride overrideEntry);

   private:
	AssetID m_prefabAssetId;
	UUID m_sourceLocalUuid;
	bool m_isInstanceRoot = false;
	std::vector<PropertyOverride> m_overrides;
};

/**
 * Finds the PrefabInstanceComponent attached to `entity`, or returns nullptr.
 */
PrefabInstanceComponent* findPrefabInstanceComponent(WorldObject& entity);
const PrefabInstanceComponent* findPrefabInstanceComponent(
	const WorldObject& entity);

}  // namespace bird

#endif	// BIRD_SERIALIZATION_PREFAB_INSTANCE_COMPONENT_H
