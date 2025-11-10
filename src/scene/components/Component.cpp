#include "Component.h"

namespace bird {

Component::Component() {}

WorldObject* Component::getParent() {
	return m_parent;
}

void Component::setParent(WorldObject* parent) {
	m_parent = parent;
}

}  // namespace bird
