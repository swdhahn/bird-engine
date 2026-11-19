#ifndef H_BIRD_MEMORY
#define H_BIRD_MEMORY

#include <memory>
#include <utility>

namespace bird {

template <typename T>
using _own = std::unique_ptr<T>;
template <typename T>
using _ref = std::shared_ptr<T>;
template <typename T>
using _weak = std::weak_ptr<T>;

template <typename T, typename... Args>
constexpr _own<T> _own_mk(Args&&... args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
constexpr _ref<T> _ref_mk(Args&&... args) {
	return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T>
constexpr std::remove_reference_t<T>&& _mv(T&& t) noexcept {
	return static_cast<std::remove_reference_t<T>&&>(t);
}

}  // namespace bird
#endif	// !H_BIRD_MEMORY
