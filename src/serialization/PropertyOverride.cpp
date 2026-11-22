#include "PropertyOverride.h"

#include "../scene/WorldObject.h"
#include "ComponentRegistry.h"

namespace bird {

PropertyOverride PropertyOverride::entityProperty(
	const std::string& propertyPath, SerializedValue value,
	UUID targetLocalUuid) {
	PropertyOverride ov;
	ov.targetLocalUuid = targetLocalUuid;
	ov.targetKind = OverrideTargetKind::EntityProperty;
	ov.propertyPath = propertyPath;
	ov.value = std::move(value);
	return ov;
}

PropertyOverride PropertyOverride::componentField(
	const std::string& componentType, const std::string& fieldName,
	SerializedValue value, UUID targetLocalUuid, uint32_t componentIndex) {
	PropertyOverride ov;
	ov.targetLocalUuid = targetLocalUuid;
	ov.targetKind = OverrideTargetKind::ComponentField;
	ov.componentType = componentType;
	ov.componentIndex = componentIndex;
	ov.propertyPath = fieldName;
	ov.value = std::move(value);
	return ov;
}

void PropertyOverride::serialize(Archive& ar) {
	ar.field("target_local_uuid", targetLocalUuid);
	std::string kindStr =
		(targetKind == OverrideTargetKind::ComponentField) ? "component"
														   : "entity";
	ar.field("target_kind", kindStr);
	if (ar.isReading()) {
		targetKind = (kindStr == "component")
						 ? OverrideTargetKind::ComponentField
						 : OverrideTargetKind::EntityProperty;
	}
	if (targetKind == OverrideTargetKind::ComponentField) {
		ar.field("component_type", componentType);
		ar.field("component_index", componentIndex);
	}
	ar.field("property", propertyPath);
	ar.field("value", value);
}

bool PropertyOverride::operator==(const PropertyOverride& other) const {
	return targetLocalUuid == other.targetLocalUuid &&
		   targetKind == other.targetKind &&
		   componentType == other.componentType &&
		   componentIndex == other.componentIndex &&
		   propertyPath == other.propertyPath &&
		   value.approxEquals(other.value);
}

bool applyEntityPropertyOverride(WorldObject& entity,
								 const std::string& propertyPath,
								 const SerializedValue& value) {
	if (propertyPath == "name" && value.isString()) {
		entity.setName(value.asString());
		return true;
	}
	if (propertyPath == "worldPosition" &&
		(value.isVec3() || (value.isArray() && value.asArray().size() == 3))) {
		entity.setWorldPosition(value.asVec3());
		return true;
	}
	if (propertyPath == "localPosition" &&
		(value.isVec3() || (value.isArray() && value.asArray().size() == 3))) {
		entity.setLocalPosition(value.asVec3());
		return true;
	}
	if (propertyPath == "worldRotation" &&
		(value.isQuat() || (value.isArray() && value.asArray().size() == 4))) {
		entity.setWorldRotation(value.asQuat());
		return true;
	}
	if (propertyPath == "localRotation" &&
		(value.isQuat() || (value.isArray() && value.asArray().size() == 4))) {
		entity.setLocalRotation(value.asQuat());
		return true;
	}
	if (propertyPath == "scale" &&
		(value.isVec3() || (value.isArray() && value.asArray().size() == 3))) {
		entity.setScale(value.asVec3());
		return true;
	}
	return false;
}

bool applyOverrideToEntity(WorldObject& entity,
						   const PropertyOverride& overrideEntry,
						   const SerializationContext* ctx) {
	if (overrideEntry.targetKind == OverrideTargetKind::EntityProperty) {
		return applyEntityPropertyOverride(entity, overrideEntry.propertyPath,
										   overrideEntry.value);
	}

	const auto& reg = ComponentRegistry::instance();
	uint32_t matchCount = 0;
	for (auto& compPtr : entity.getComponents()) {
		if (!compPtr) continue;
		const ComponentDescriptor* desc = reg.findForComponent(*compPtr);
		if (!desc || desc->typeName != overrideEntry.componentType) continue;

		if (matchCount == overrideEntry.componentIndex) {
			return reg.applyFieldOverride(*compPtr, overrideEntry.propertyPath,
										  overrideEntry.value, ctx);
		}
		++matchCount;
	}
	return false;
}

}  // namespace bird
