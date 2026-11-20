#include "Window.h"

namespace bird {

bool Window::updateSizes(int framebufferWidth, int framebufferHeight,
						 int width, int height) {
	// A minimized window reports zero; keep the last usable size.
	if (framebufferWidth <= 0 || framebufferHeight <= 0 || width <= 0 ||
		height <= 0) {
		return false;
	}
	const bool changed =
		static_cast<uint32_t>(framebufferWidth) != m_framebufferWidth ||
		static_cast<uint32_t>(framebufferHeight) != m_framebufferHeight ||
		static_cast<uint32_t>(width) != m_width ||
		static_cast<uint32_t>(height) != m_height;
	m_framebufferWidth = static_cast<uint32_t>(framebufferWidth);
	m_framebufferHeight = static_cast<uint32_t>(framebufferHeight);
	m_width = static_cast<uint32_t>(width);
	m_height = static_cast<uint32_t>(height);
	return changed;
}

}  // namespace bird
