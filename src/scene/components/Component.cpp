#include "Component.h"

namespace bird {

Component::Component() {}

void Component::_init() {}

void Component::_deinit() {}

WorldObject* Component::world_object() {
	return m_object;
}

void Component::setWorldObject(WorldObject* parent) {
	m_object = parent;
}

}  // namespace bird
