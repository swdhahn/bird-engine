#include "../Platform.h"
#include "HeadlessWindow.h"

namespace bird::platform {

std::unique_ptr<Window> createWindow(uint32_t width, uint32_t height,
									 const std::string& /*title*/) {
	return std::make_unique<HeadlessWindow>(width, height);
}

}  // namespace bird::platform
