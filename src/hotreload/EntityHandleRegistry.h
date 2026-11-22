#ifndef BIRD_HOT_RELOAD_ENTITY_HANDLE_REGISTRY_H
#define BIRD_HOT_RELOAD_ENTITY_HANDLE_REGISTRY_H

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "../util/Constants.h"

#if defined(_WIN32) || defined(_MSC_VER)
// Always export: these are defined only inside Bird_core (compiled through
// the hotreload object library, which never sees BIRD_CORE_EXPORTS) and are
// resolved at runtime by Dart FFI, never linked against by C++ consumers.
#define BIRD_ENTITY_C_API __declspec(dllexport)
#else
#define BIRD_ENTITY_C_API __attribute__((visibility("default")))
#endif

namespace bird {
class WorldObject;
}

namespace bird::hotreload {

/**
 * Stable, monotonically increasing entity identifier matching WorldObject::getID().
 *
 * Unlike raw native pointers (`WorldObject*`), an EntityID can safely persist
 * across Dart hot-reloads and scene mutations: if the underlying native entity
 * has been destroyed, resolving the EntityID returns `nullptr` rather than
 * producing a dangling pointer.
 */
using EntityID = uint64_t;

/** Sentinel value representing an invalid or unassigned entity handle. */
constexpr EntityID kInvalidEntityID = 0;

/**
 * Process-wide registry mapping live EntityID handles to native WorldObject*.
 *
 * WorldObject automatically registers itself upon construction and unregisters
 * itself upon destruction. Native-to-Dart script bindings can store EntityID
 * values instead of raw `ffi.Pointer<ffi.Void>` addresses and resolve them on
 * demand via `resolve()` or the C-ABI functions below, preventing use-after-free
 * and dangling pointers when Dart hot-reload reloads isolate code while C++
 * scene objects remain alive or are recreated.
 *
 * Thread safety: All public methods are protected by an internal mutex.
 */
class BIRD_NO_EXPORT EntityHandleRegistry {
   public:
	/** @return Process-wide EntityHandleRegistry singleton. */
	static EntityHandleRegistry& get();

	/**
	 * Registers a live WorldObject under its unique EntityID.
	 * Called automatically by WorldObject constructors.
	 *
	 * @param id Unique entity identifier (must not be kInvalidEntityID).
	 * @param entity Non-null pointer to the live WorldObject.
	 */
	void registerEntity(EntityID id, WorldObject* entity);

	/**
	 * Removes an entity from the registry upon destruction.
	 * Called automatically by ~WorldObject().
	 *
	 * @param id Unique entity identifier to remove.
	 */
	void unregisterEntity(EntityID id);

	/**
	 * Resolves an EntityID to its live WorldObject*.
	 *
	 * @param id Entity handle to resolve.
	 * @return Pointer to the live WorldObject, or nullptr if the entity does
	 *         not exist or has already been destroyed.
	 */
	WorldObject* resolve(EntityID id) const;

	/**
	 * Looks up the EntityID for a currently registered WorldObject*.
	 *
	 * @param entity Pointer to a registered WorldObject.
	 * @return The entity's EntityID, or kInvalidEntityID if not registered.
	 */
	EntityID getID(const WorldObject* entity) const;

	/**
	 * Checks whether an EntityID currently refers to a live native WorldObject.
	 *
	 * @param id Entity handle to check.
	 * @return True if the entity is currently alive in the registry.
	 */
	bool isValid(EntityID id) const;

	/** @return Number of currently registered live entities. */
	size_t size() const;

	/** Clears all entries (intended for unit tests only). */
	void clear();

   private:
	EntityHandleRegistry() = default;
	~EntityHandleRegistry() = default;

	EntityHandleRegistry(const EntityHandleRegistry&) = delete;
	EntityHandleRegistry& operator=(const EntityHandleRegistry&) = delete;

	mutable std::mutex m_mutex;
	std::unordered_map<EntityID, WorldObject*> m_entities;
	std::unordered_map<const WorldObject*, EntityID> m_reverse;
};

}  // namespace bird::hotreload

extern "C" {

/**
 * C-ABI FFI hook: returns the stable EntityID for a live WorldObject*,
 * or 0 (kInvalidEntityID) if `worldObject` is null.
 */
BIRD_ENTITY_C_API uint64_t bird_EntityRegistry_getID(void* worldObject);

/**
 * C-ABI FFI hook: resolves a stable EntityID to its live WorldObject*,
 * or nullptr if the entity has been destroyed.
 */
BIRD_ENTITY_C_API void* bird_EntityRegistry_resolve(uint64_t entityId);

/**
 * C-ABI FFI hook: returns true if `entityId` currently maps to a live native
 * WorldObject, false otherwise.
 */
BIRD_ENTITY_C_API bool bird_EntityRegistry_isValid(uint64_t entityId);

}  // extern "C"

#endif	// BIRD_HOT_RELOAD_ENTITY_HANDLE_REGISTRY_H
