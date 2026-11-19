#ifndef BIRD_COMPONENT_IMPL_H
#define BIRD_COMPONENT_IMPL_H

#include <cassert>
#include <type_traits>

#include "Component.h"

namespace bird {

/**
 * Template implementation for Component::objectAs<T>().
 *
 * Enforces compile-time type inheritance from Object, checks validity of
 * host pointer, and verifies matching runtime OBJ_TYPES.
 */
template <typename T>
T* Component::objectAs() {
	static_assert(std::is_base_of<Object, T>::value,
				  "T must derive from Object");

	if (!m_object) return nullptr;

	if (m_object->getType() == T::s_type) {
		return static_cast<T*>(m_object);
	}

	assert(false && "Type mismatch in objectAs!");
	return nullptr;
}

}  // namespace bird

#endif // BIRD_COMPONENT_IMPL_H
