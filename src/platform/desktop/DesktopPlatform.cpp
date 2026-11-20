#include "../Platform.h"
#include "GlfwWindow.h"

namespace bird::platform {

std::unique_ptr<Window> createWindow(uint32_t width, uint32_t height,
									 const std::string& title) {
	return std::make_unique<GlfwWindow>(width, height, title);
}

}  // namespace bird::platform
