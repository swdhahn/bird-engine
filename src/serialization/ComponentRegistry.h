#ifndef BIRD_SERIALIZATION_COMPONENT_REGISTRY_H
#define BIRD_SERIALIZATION_COMPONENT_REGISTRY_H

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "../scene/components/Component.h"
#include "../util/Memory.hpp"
#include "Archive.h"
#include "SerializedValue.h"

namespace bird {

namespace detail {

template <typename T>
struct FieldTraits;

template <>
struct FieldTraits<bool> {
	static constexpr SerializedValueType kType = SerializedValueType::Bool;
	static SerializedValue toValue(const bool& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, bool& out,
						  const SerializationContext*) {
		if (!v.isBool()) return false;
		out = v.asBool();
		return true;
	}
};

template <>
struct FieldTraits<int32_t> {
	static constexpr SerializedValueType kType = SerializedValueType::Int;
	static SerializedValue toValue(const int32_t& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, int32_t& out,
						  const SerializationContext*) {
		if (!v.isNumber()) return false;
		out = static_cast<int32_t>(v.asInt());
		return true;
	}
};

template <>
struct FieldTraits<uint32_t> {
	static constexpr SerializedValueType kType = SerializedValueType::Int;
	static SerializedValue toValue(const uint32_t& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, uint32_t& out,
						  const SerializationContext*) {
		if (!v.isNumber()) return false;
		out = static_cast<uint32_t>(v.asInt());
		return true;
	}
};

template <>
struct FieldTraits<int64_t> {
	static constexpr SerializedValueType kType = SerializedValueType::Int;
	static SerializedValue toValue(const int64_t& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, int64_t& out,
						  const SerializationContext*) {
		if (!v.isNumber()) return false;
		out = v.asInt();
		return true;
	}
};

template <>
struct FieldTraits<uint64_t> {
	static constexpr SerializedValueType kType = SerializedValueType::Int;
	static SerializedValue toValue(const uint64_t& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, uint64_t& out,
						  const SerializationContext*) {
		if (!v.isNumber()) return false;
		out = static_cast<uint64_t>(v.asInt());
		return true;
	}
};

template <>
struct FieldTraits<float> {
	static constexpr SerializedValueType kType = SerializedValueType::Double;
	static SerializedValue toValue(const float& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, float& out,
						  const SerializationContext*) {
		if (!v.isNumber()) return false;
		out = v.asFloat();
		return true;
	}
};

template <>
struct FieldTraits<double> {
	static constexpr SerializedValueType kType = SerializedValueType::Double;
	static SerializedValue toValue(const double& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, double& out,
						  const SerializationContext*) {
		if (!v.isNumber()) return false;
		out = v.asDouble();
		return true;
	}
};

template <>
struct FieldTraits<std::string> {
	static constexpr SerializedValueType kType = SerializedValueType::String;
	static SerializedValue toValue(const std::string& v) {
		return SerializedValue(v);
	}
	static bool fromValue(const SerializedValue& v, std::string& out,
						  const SerializationContext*) {
		if (!v.isString()) return false;
		out = v.asString();
		return true;
	}
};

template <>
struct FieldTraits<UUID> {
	static constexpr SerializedValueType kType = SerializedValueType::Uuid;
	static SerializedValue toValue(const UUID& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, UUID& out,
						  const SerializationContext*) {
		if (v.isUuid() || v.isString()) {
			out = v.asUuid();
			return true;
		}
		return false;
	}
};

template <>
struct FieldTraits<Vector2> {
	static constexpr SerializedValueType kType = SerializedValueType::Vec2;
	static SerializedValue toValue(const Vector2& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, Vector2& out,
						  const SerializationContext*) {
		if (!v.isVec2() && !(v.isArray() && v.asArray().size() == 2)) return false;
		out = v.asVec2();
		return true;
	}
};

template <>
struct FieldTraits<Vector3> {
	static constexpr SerializedValueType kType = SerializedValueType::Vec3;
	static SerializedValue toValue(const Vector3& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, Vector3& out,
						  const SerializationContext*) {
		if (!v.isVec3() && !(v.isArray() && v.asArray().size() == 3)) return false;
		out = v.asVec3();
		return true;
	}
};

template <>
struct FieldTraits<Vector4> {
	static constexpr SerializedValueType kType = SerializedValueType::Vec4;
	static SerializedValue toValue(const Vector4& v) { return SerializedValue(v); }
	static bool fromValue(const SerializedValue& v, Vector4& out,
						  const SerializationContext*) {
		if (!v.isVec4() && !(v.isArray() && v.asArray().size() == 4)) return false;
		out = v.asVec4();
		return true;
	}
};

template <>
struct FieldTraits<Quaternion> {
	static constexpr SerializedValueType kType = SerializedValueType::Quat;
	static SerializedValue toValue(const Quaternion& v) {
		return SerializedValue(v);
	}
	static bool fromValue(const SerializedValue& v, Quaternion& out,
						  const SerializationContext*) {
		if (!v.isQuat() && !(v.isArray() && v.asArray().size() == 4)) return false;
		out = v.asQuat();
		return true;
	}
};

template <>
struct FieldTraits<EntityRef> {
	static constexpr SerializedValueType kType = SerializedValueType::EntityRef;
	static SerializedValue toValue(const EntityRef& v) {
		return SerializedValue(v);
	}
	static bool fromValue(const SerializedValue& v, EntityRef& out,
						  const SerializationContext* ctx) {
		if (!v.isEntityRef() && !v.isUuid() && !v.isString()) return false;
		EntityRef ref = v.asEntityRef();
		if (ctx) ref = ctx->remapEntityRef(ref);
		out = ref;
		return true;
	}
};

template <>
struct FieldTraits<SerializedValue> {
	static constexpr SerializedValueType kType = SerializedValueType::Null;
	static SerializedValue toValue(const SerializedValue& v) { return v; }
	static bool fromValue(const SerializedValue& v, SerializedValue& out,
						  const SerializationContext* ctx) {
		if (v.isEntityRef() && ctx) {
			out = SerializedValue(ctx->remapEntityRef(v.asEntityRef()));
		} else {
			out = v;
		}
		return true;
	}
};

}  // namespace detail

/**
 * Reflection descriptor for a single serializable field on a Component.
 *
 * Powers archive read/write, prefab override application, and override diffing
 * uniformly from a single field registration.
 */
struct FieldDescriptor {
	/** Field name as written in serialized JSON/binary output and override paths. */
	std::string name;

	/** Logical value type of this field. */
	SerializedValueType valueType = SerializedValueType::Null;

	/** Writes this field's value from `comp` into `ar`. */
	std::function<void(const Component& comp, Archive& ar)> writeToArchive;

	/** Reads this field's value from `ar` into `comp`. */
	std::function<void(Component& comp, Archive& ar)> readFromArchive;

	/** Reads this field's current value as a SerializedValue (for override diffing). */
	std::function<SerializedValue(const Component& comp)> getValue;

	/**
	 * Applies a SerializedValue to this field (for prefab overrides).
	 * @return True if the value was compatible and successfully applied.
	 */
	std::function<bool(Component& comp, const SerializedValue& value,
					   const SerializationContext* ctx)>
		setValue;
};

/**
 * Complete reflection and serialization metadata for a registered Component type.
 */
struct ComponentDescriptor {
	/** Stable string identifier serialized under `"type"` (e.g. "LightComponent"). */
	std::string typeName;

	/** C++ runtime type_index for reverse lookup during serialization. */
	std::type_index typeIndex = std::type_index(typeid(void));

	/** Default factory creating a new component instance. */
	std::function<_own<Component>()> factory;

	/**
	 * Optional archive-aware factory for components whose constructor requires
	 * immutable parameters from the archive (e.g. LightComponent(LightType)).
	 */
	std::function<_own<Component>(Archive& ar)> factoryFromArchive;

	/** Declaratively reflected fields. */
	std::vector<FieldDescriptor> fields;

	/** Optional custom serialize hook (runs in addition to or instead of fields). */
	std::function<void(const Component& comp, Archive& ar)> customSerialize;

	/** Optional custom deserialize hook (runs in addition to or instead of fields). */
	std::function<void(Component& comp, Archive& ar)> customDeserialize;

	/**
	 * Optional dynamic field lookup (e.g. DartScriptRecord exposed properties).
	 * Called when a field name is not in `fields`.
	 */
	std::function<std::optional<SerializedValue>(const Component& comp,
												 const std::string& fieldName)>
		dynamicGetField;

	/**
	 * Optional dynamic field mutator (e.g. DartScriptRecord exposed properties).
	 * Called when an override targets a field name not in `fields`.
	 */
	std::function<bool(Component& comp, const std::string& fieldName,
					   const SerializedValue& value,
					   const SerializationContext* ctx)>
		dynamicSetField;

	/**
	 * Optional dynamic field enumerator returning all current dynamic
	 * (name -> value) pairs for automatic prefab override diffing.
	 */
	std::function<std::vector<std::pair<std::string, SerializedValue>>(
		const Component& comp)>
		dynamicEnumerateFields;

	/** Finds a statically registered FieldDescriptor by name, or nullptr. */
	const FieldDescriptor* findField(const std::string& name) const {
		for (const auto& f : fields) {
			if (f.name == name) return &f;
		}
		return nullptr;
	}
};

/**
 * Fluent builder used inside ComponentRegistry::registerComponent<T>() to
 * declare serializable fields and optional custom hooks for component type T.
 *
 * @tparam T Concrete Component subclass being registered.
 */
template <typename T>
class ComponentMetaBuilder {
   public:
	static_assert(std::is_base_of_v<Component, T>,
				  "T must inherit from bird::Component");

	explicit ComponentMetaBuilder(ComponentDescriptor& desc) : m_desc(desc) {}

	/**
	 * Registers a direct public data member for serialization and overrides.
	 *
	 * @tparam FieldT Supported primitive, math, string, UUID, or EntityRef type.
	 * @param name Serialized field name.
	 * @param memberPtr Pointer to member `FieldT T::*`.
	 */
	template <typename FieldT>
	ComponentMetaBuilder& field(const std::string& name, FieldT T::*memberPtr) {
		return field<FieldT>(
			name,
			[memberPtr](const T& c) -> FieldT { return c.*memberPtr; },
			[memberPtr](T& c, const FieldT& v) { c.*memberPtr = v; });
	}

	/**
	 * Registers a field backed by member getter and setter methods.
	 * Accepts getters returning `FieldT` or `const FieldT&`, and setters
	 * accepting `FieldT` or `const FieldT&`.
	 *
	 * @param name Serialized field name.
	 * @param getter Const member function returning the field value.
	 * @param setter Member function updating the field value.
	 */
	template <typename GetterRet, typename SetterArg>
	ComponentMetaBuilder& field(const std::string& name,
								GetterRet (T::*getter)() const,
								void (T::*setter)(SetterArg)) {
		using FieldT = std::decay_t<GetterRet>;
		return field<FieldT>(
			name,
			[getter](const T& c) -> FieldT { return (c.*getter)(); },
			[setter](T& c, const FieldT& v) { (c.*setter)(v); });
	}

	/**
	 * Registers a field backed by arbitrary getter and setter callables.
	 *
	 * @tparam FieldT Logical field type.
	 * @param name Serialized field name.
	 * @param getter Callable `FieldT(const T&)`.
	 * @param setter Callable `void(T&, const FieldT&)`.
	 */
	template <typename FieldT, typename GetterFn, typename SetterFn>
	ComponentMetaBuilder& field(const std::string& name, GetterFn getter,
								SetterFn setter) {
		using DecayedT = std::decay_t<FieldT>;
		FieldDescriptor fd;
		fd.name = name;
		fd.valueType = detail::FieldTraits<DecayedT>::kType;
		fd.writeToArchive = [name, getter](const Component& comp, Archive& ar) {
			DecayedT v = getter(static_cast<const T&>(comp));
			ar.field(name.c_str(), v);
		};
		fd.readFromArchive = [name, getter, setter](Component& comp,
													Archive& ar) {
			DecayedT v = getter(static_cast<const T&>(comp));
			if (ar.field(name.c_str(), v)) {
				setter(static_cast<T&>(comp), v);
			}
		};
		fd.getValue = [getter](const Component& comp) -> SerializedValue {
			return detail::FieldTraits<DecayedT>::toValue(
				getter(static_cast<const T&>(comp)));
		};
		fd.setValue = [setter](Component& comp, const SerializedValue& val,
							   const SerializationContext* ctx) -> bool {
			DecayedT parsed{};
			if (!detail::FieldTraits<DecayedT>::fromValue(val, parsed, ctx)) {
				return false;
			}
			setter(static_cast<T&>(comp), parsed);
			return true;
		};
		m_desc.fields.push_back(std::move(fd));
		return *this;
	}

	/**
	 * Registers an enum or enum class field serialized as its integer value.
	 *
	 * @tparam EnumT Enum type.
	 * @param name Serialized field name.
	 * @param getter Callable `EnumT(const T&)`.
	 * @param setter Callable `void(T&, EnumT)`.
	 */
	template <typename EnumT, typename GetterFn, typename SetterFn>
	ComponentMetaBuilder& enumField(const std::string& name, GetterFn getter,
									SetterFn setter) {
		static_assert(std::is_enum_v<EnumT>, "EnumT must be an enum type");
		FieldDescriptor fd;
		fd.name = name;
		fd.valueType = SerializedValueType::Int;
		fd.writeToArchive = [name, getter](const Component& comp, Archive& ar) {
			int32_t raw = static_cast<int32_t>(getter(static_cast<const T&>(comp)));
			ar.field(name.c_str(), raw);
		};
		fd.readFromArchive = [name, getter, setter](Component& comp,
													Archive& ar) {
			int32_t raw = static_cast<int32_t>(getter(static_cast<const T&>(comp)));
			if (ar.field(name.c_str(), raw)) {
				setter(static_cast<T&>(comp), static_cast<EnumT>(raw));
			}
		};
		fd.getValue = [getter](const Component& comp) -> SerializedValue {
			return SerializedValue(
				static_cast<int64_t>(getter(static_cast<const T&>(comp))));
		};
		fd.setValue = [setter](Component& comp, const SerializedValue& val,
							   const SerializationContext*) -> bool {
			if (!val.isNumber()) return false;
			setter(static_cast<T&>(comp), static_cast<EnumT>(val.asInt()));
			return true;
		};
		m_desc.fields.push_back(std::move(fd));
		return *this;
	}

	/**
	 * Registers an archive-aware factory used when constructing T during
	 * deserialization (for components whose constructor takes immutable args).
	 */
	ComponentMetaBuilder& factoryFromArchive(
		std::function<_own<Component>(Archive&)> fn) {
		m_desc.factoryFromArchive = std::move(fn);
		return *this;
	}

	/**
	 * Registers custom archive write/read hooks (for dynamic-schema components
	 * such as DartScriptRecord).
	 */
	ComponentMetaBuilder& customSerializer(
		std::function<void(const T&, Archive&)> writeFn,
		std::function<void(T&, Archive&)> readFn) {
		m_desc.customSerialize = [writeFn](const Component& c, Archive& ar) {
			writeFn(static_cast<const T&>(c), ar);
		};
		m_desc.customDeserialize = [readFn](Component& c, Archive& ar) {
			readFn(static_cast<T&>(c), ar);
		};
		return *this;
	}

	/**
	 * Registers dynamic field getters, setters, and enumerators for components
	 * with runtime-defined property maps.
	 */
	ComponentMetaBuilder& dynamicFields(
		std::function<std::optional<SerializedValue>(const T&, const std::string&)>
			getFn,
		std::function<bool(T&, const std::string&, const SerializedValue&,
						   const SerializationContext*)>
			setFn,
		std::function<std::vector<std::pair<std::string, SerializedValue>>(const T&)>
			enumFn) {
		m_desc.dynamicGetField = [getFn](const Component& c,
										 const std::string& name) {
			return getFn(static_cast<const T&>(c), name);
		};
		m_desc.dynamicSetField = [setFn](Component& c, const std::string& name,
										 const SerializedValue& v,
										 const SerializationContext* ctx) {
			return setFn(static_cast<T&>(c), name, v, ctx);
		};
		m_desc.dynamicEnumerateFields = [enumFn](const Component& c) {
			return enumFn(static_cast<const T&>(c));
		};
		return *this;
	}

   private:
	ComponentDescriptor& m_desc;
};

/**
 * Central reflection and serialization registry for Bird Engine components.
 *
 * Components register themselves (either at startup or via static init) using
 * `ComponentRegistry::instance().registerComponent<MyComp>(...)`. The scene
 * serializer, prefab instantiator, and override applier all query this
 * registry dynamically without hardcoding concrete component types.
 */
class ComponentRegistry {
   public:
	/** Returns the process-wide singleton ComponentRegistry. */
	static ComponentRegistry& instance();

	/**
	 * Registers a concrete Component subclass T under a unique `typeName`.
	 *
	 * @tparam T Concrete Component subclass.
	 * @param typeName Stable string type identifier (e.g. "HealthComponent").
	 * @param factory Default constructor factory returning `_own<Component>`.
	 * @param configure Callback receiving `ComponentMetaBuilder<T>&` to declare fields.
	 */
	template <typename T, typename ConfigureFn>
	void registerComponent(const std::string& typeName,
						   std::function<_own<Component>()> factory,
						   ConfigureFn configure) {
		static_assert(std::is_base_of_v<Component, T>,
					  "T must inherit from bird::Component");
		ComponentDescriptor desc;
		desc.typeName = typeName;
		desc.typeIndex = std::type_index(typeid(T));
		desc.factory = std::move(factory);
		ComponentMetaBuilder<T> builder(desc);
		configure(builder);
		registerDescriptor(std::move(desc));
	}

	/**
	 * Convenience overload for default-constructible Component subclasses T.
	 */
	template <typename T, typename ConfigureFn>
	void registerComponent(const std::string& typeName, ConfigureFn configure) {
		registerComponent<T>(
			typeName, []() -> _own<Component> { return _own_mk<T>(); },
			std::move(configure));
	}

	/**
	 * Registers an alias mapping a second C++ type_index to an existing
	 * ComponentDescriptor (e.g. mapping live DartScriptComponent to the
	 * "DartScript" descriptor).
	 */
	void registerTypeAlias(std::type_index aliasType,
						   const std::string& canonicalTypeName);

	/** Registers a fully constructed ComponentDescriptor directly. */
	void registerDescriptor(ComponentDescriptor descriptor);

	/** Removes a registration by type name (primarily useful in unit tests). */
	void unregisterComponent(const std::string& typeName);

	/** Returns true if a component with the given typeName is registered. */
	bool isRegistered(const std::string& typeName) const;

	/** Looks up a descriptor by its serialized typeName, or nullptr. */
	const ComponentDescriptor* findByName(const std::string& typeName) const;

	/** Looks up a descriptor by C++ type_index, or nullptr. */
	const ComponentDescriptor* findByType(std::type_index type) const;

	/** Looks up the descriptor for a live Component instance via typeid(*comp). */
	const ComponentDescriptor* findForComponent(const Component& comp) const;

	/**
	 * Serializes a component's fields into the current archive object scope.
	 * Does NOT write the `"type"` discriminator (caller manages the envelope).
	 *
	 * @return True if the component type was registered and serialized.
	 */
	bool serializeComponent(const Component& comp, Archive& ar) const;

	/**
	 * Constructs and deserializes a component of the given `typeName` from
	 * the current archive object scope.
	 *
	 * @return Newly constructed component, or nullptr if `typeName` is unregistered.
	 */
	_own<Component> deserializeComponent(const std::string& typeName,
										 Archive& ar) const;

	/**
	 * Reads a named field (static or dynamic) from a live component.
	 */
	std::optional<SerializedValue> getFieldValue(
		const Component& comp, const std::string& fieldName) const;

	/**
	 * Writes a named field (static or dynamic) on a live component.
	 * Used by the prefab system when applying PropertyOverride deltas.
	 *
	 * @return True if the field existed and the value was applied.
	 */
	bool applyFieldOverride(Component& comp, const std::string& fieldName,
							const SerializedValue& value,
							const SerializationContext* ctx = nullptr) const;

	/**
	 * Enumerates all static and dynamic (name -> value) field pairs of `comp`.
	 */
	std::vector<std::pair<std::string, SerializedValue>> enumerateFields(
		const Component& comp) const;

	/** Returns all registered component type names. */
	std::vector<std::string> getRegisteredTypeNames() const;

   private:
	ComponentRegistry() = default;

	std::unordered_map<std::string, ComponentDescriptor> m_byName;
	std::unordered_map<std::type_index, std::string> m_typeToName;
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_COMPONENT_REGISTRY_H
