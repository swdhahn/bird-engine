#include <cassert>

#include "Component.h"

namespace bird {

template <typename T>
T* Component::objectAs() {
	// 1. Compile-time check: Is T actually an Object?
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
