#ifndef BIRD_UIPLATFORM_H
#define BIRD_UIPLATFORM_H

#include <cstdint>
#include <string>

namespace bird {

enum class UICursor : uint8_t {
	Arrow,
	IBeam,
	Hand,
	ResizeHorizontal,
	ResizeVertical,
	ResizeDiagonal
};

/**
 * Operating-system services the UI needs. The window implements this with
 * GLFW; tests substitute a fake, so the UI stays testable without a window.
 */
class UIPlatform {
   public:
	virtual ~UIPlatform() = default;

	virtual std::string getClipboardText() = 0;
	virtual void setClipboardText(const std::string& text) = 0;
	virtual void setCursor(UICursor cursor) = 0;
};

}  // namespace bird

#endif	// !BIRD_UIPLATFORM_H
