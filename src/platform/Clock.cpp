#include "Clock.h"

#include <chrono>

namespace bird::platform {

double now() {
	using Seconds = std::chrono::duration<double>;
	return std::chrono::duration_cast<Seconds>(
			   std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

}  // namespace bird::platform
