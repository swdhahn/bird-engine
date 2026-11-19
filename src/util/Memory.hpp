#ifndef H_BIRD_MEMORY
#define H_BIRD_MEMORY

#include <memory>
#include <utility>

namespace bird {

/**
 * Bird engine memory and smart pointer aliases.
 *
 * _own<T> represents exclusive ownership (std::unique_ptr).
 * _ref<T> represents shared reference-counted ownership (std::shared_ptr).
 * _weak<T> represents non-owning observer references (std::weak_ptr).
 */
template <typename T>
using _own = std::unique_ptr<T>;

template <typename T>
using _ref = std::shared_ptr<T>;

template <typename T>
using _weak = std::weak_ptr<T>;

/**
 * Factory constructing an exclusively owned object (_own<T>).
 * Equivalent to std::make_unique<T>(...).
 */
template <typename T, typename... Args>
constexpr _own<T> _own_mk(Args&&... args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}

/**
 * Factory constructing a reference-counted shared object (_ref<T>).
 * Equivalent to std::make_shared<T>(...).
 */
template <typename T, typename... Args>
constexpr _ref<T> _ref_mk(Args&&... args) {
	return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
 * Explicit move cast helper.
 * Casts an lvalue reference to an rvalue reference (equivalent to std::move).
 */
template <typename T>
constexpr std::remove_reference_t<T>&& _mv(T&& t) noexcept {
	return static_cast<std::remove_reference_t<T>&&>(t);
}

}  // namespace bird
#endif	// !H_BIRD_MEMORY
