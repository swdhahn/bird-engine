#include "PrefabInstanceComponent.h"

#include "../scene/WorldObject.h"

namespace bird {

PrefabInstanceComponent::PrefabInstanceComponent(AssetID prefabAssetId,
												 UUID sourceLocalUuid,
												 bool isInstanceRoot)
	: m_prefabAssetId(std::move(prefabAssetId)),
	  m_sourceLocalUuid(sourceLocalUuid),
	  m_isInstanceRoot(isInstanceRoot) {}

void PrefabInstanceComponent::setOverride(PropertyOverride overrideEntry) {
	for (auto& existing : m_overrides) {
		if (existing.targetLocalUuid == overrideEntry.targetLocalUuid &&
			existing.targetKind == overrideEntry.targetKind &&
			existing.componentType == overrideEntry.componentType &&
			existing.componentIndex == overrideEntry.componentIndex &&
			existing.propertyPath == overrideEntry.propertyPath) {
			existing.value = std::move(overrideEntry.value);
			return;
		}
	}
	m_overrides.push_back(std::move(overrideEntry));
}

PrefabInstanceComponent* findPrefabInstanceComponent(WorldObject& entity) {
	for (auto& comp : entity.getComponents()) {
		if (auto* pic = dynamic_cast<PrefabInstanceComponent*>(comp.get())) {
			return pic;
		}
	}
	return nullptr;
}

const PrefabInstanceComponent* findPrefabInstanceComponent(
	const WorldObject& entity) {
	// getComponents() is non-const on WorldObject; cast away const solely to iterate.
	auto& nonConst = const_cast<WorldObject&>(entity);
	for (const auto& comp : nonConst.getComponents()) {
		if (auto* pic =
				dynamic_cast<const PrefabInstanceComponent*>(comp.get())) {
			return pic;
		}
	}
	return nullptr;
}

}  // namespace bird
