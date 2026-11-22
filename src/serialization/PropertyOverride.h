#ifndef BIRD_SERIALIZATION_PROPERTY_OVERRIDE_H
#define BIRD_SERIALIZATION_PROPERTY_OVERRIDE_H

#include <cstdint>
#include <string>

#include "Archive.h"
#include "SerializedValue.h"
#include "UUID.h"

namespace bird {

class WorldObject;

/**
 * Identifies whether a PropertyOverride targets an entity-level property
 * (name, transform) or a field on an attached component.
 */
enum class OverrideTargetKind : uint8_t {
	/** Targets an entity property: name, worldPosition, localPosition, worldRotation, localRotation, scale. */
	EntityProperty = 0,
	/** Targets a reflected or dynamic field on an attached Component. */
	ComponentField = 1
};

/**
 * Represents a single property delta applied on top of a base Prefab instance.
 *
 * When a prefab is placed in a scene, only the `prefab_asset_id` and a list
 * of PropertyOverride entries are stored in the scene file. On scene load,
 * the base prefab is instantiated first and each PropertyOverride is applied
 * on top before component/script `init()` executes.
 */
struct PropertyOverride {
	/**
	 * Stable local UUID of the entity inside the prefab to modify.
	 * A nil UUID targets the prefab's root entity.
	 */
	UUID targetLocalUuid = UUID::nil();

	/** Whether this override modifies an entity property or a component field. */
	OverrideTargetKind targetKind = OverrideTargetKind::EntityProperty;

	/** Component type name (e.g. "LightComponent", "DartScript") when targetKind == ComponentField. */
	std::string componentType;

	/**
	 * Zero-based index disambiguating multiple components of the same
	 * `componentType` on the same entity.
	 */
	uint32_t componentIndex = 0;

	/**
	 * Property or field name.
	 * For EntityProperty: "name", "worldPosition", "localPosition",
	 * "worldRotation", "localRotation", or "scale".
	 * For ComponentField: registered field name or Dart script property name.
	 */
	std::string propertyPath;

	/** The overridden value to apply. */
	SerializedValue value;

	/** Convenience factory for an entity-level property override. */
	static PropertyOverride entityProperty(const std::string& propertyPath,
										   SerializedValue value,
										   UUID targetLocalUuid = UUID::nil());

	/** Convenience factory for a component-level field override. */
	static PropertyOverride componentField(const std::string& componentType,
										   const std::string& fieldName,
										   SerializedValue value,
										   UUID targetLocalUuid = UUID::nil(),
										   uint32_t componentIndex = 0);

	/** Reads or writes this PropertyOverride using any Archive backend. */
	void serialize(Archive& ar);

	bool operator==(const PropertyOverride& other) const;
	bool operator!=(const PropertyOverride& other) const {
		return !(*this == other);
	}
};

/**
 * Applies an entity-level property override (name or transform component) to `entity`.
 * @return True if `propertyPath` was recognized and the value was applied.
 */
bool applyEntityPropertyOverride(WorldObject& entity,
								 const std::string& propertyPath,
								 const SerializedValue& value);

/**
 * Applies a single PropertyOverride (entity property or component field) to `entity`.
 * @param entity Live target entity (already spawned from the base prefab).
 * @param overrideEntry Delta to apply.
 * @param ctx Optional SerializationContext for remapping EntityRef values.
 * @return True if the override was applied successfully.
 */
bool applyOverrideToEntity(WorldObject& entity,
						   const PropertyOverride& overrideEntry,
						   const SerializationContext* ctx = nullptr);

}  // namespace bird

#endif	// BIRD_SERIALIZATION_PROPERTY_OVERRIDE_H
