#include "EntityHandleRegistry.h"

namespace bird::hotreload {

EntityHandleRegistry& EntityHandleRegistry::get() {
	static EntityHandleRegistry instance;
	return instance;
}

void EntityHandleRegistry::registerEntity(EntityID id, WorldObject* entity) {
	if (id == kInvalidEntityID || entity == nullptr) {
		return;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	m_entities[id] = entity;
	m_reverse[entity] = id;
}

void EntityHandleRegistry::unregisterEntity(EntityID id) {
	if (id == kInvalidEntityID) {
		return;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_entities.find(id);
	if (it != m_entities.end()) {
		m_reverse.erase(it->second);
		m_entities.erase(it);
	}
}

WorldObject* EntityHandleRegistry::resolve(EntityID id) const {
	if (id == kInvalidEntityID) {
		return nullptr;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_entities.find(id);
	return it != m_entities.end() ? it->second : nullptr;
}

EntityID EntityHandleRegistry::getID(const WorldObject* entity) const {
	if (entity == nullptr) {
		return kInvalidEntityID;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_reverse.find(entity);
	return it != m_reverse.end() ? it->second : kInvalidEntityID;
}

bool EntityHandleRegistry::isValid(EntityID id) const {
	return resolve(id) != nullptr;
}

size_t EntityHandleRegistry::size() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_entities.size();
}

void EntityHandleRegistry::clear() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_entities.clear();
	m_reverse.clear();
}

}  // namespace bird::hotreload

extern "C" {

BIRD_ENTITY_C_API uint64_t bird_EntityRegistry_getID(void* worldObject) {
	return bird::hotreload::EntityHandleRegistry::get().getID(
		static_cast<const bird::WorldObject*>(worldObject));
}

BIRD_ENTITY_C_API void* bird_EntityRegistry_resolve(uint64_t entityId) {
	return static_cast<void*>(
		bird::hotreload::EntityHandleRegistry::get().resolve(entityId));
}

BIRD_ENTITY_C_API bool bird_EntityRegistry_isValid(uint64_t entityId) {
	return bird::hotreload::EntityHandleRegistry::get().isValid(entityId);
}

}  // extern "C"
