#ifndef BIRD_HEADLESSWINDOW_H
#define BIRD_HEADLESSWINDOW_H

#include <cstdint>
#include <string>

#include "../../renders/Window.h"

namespace bird::platform {

/**
 * Off-screen window for automated runs (CI rendering checks, captures on
 * machines without a display). Renders into an EGL pbuffer of fixed size,
 * delivers no input, and asks to close after a set number of frames:
 * BIRD_HEADLESS_FRAMES (default 120). Combine with BIRD_CAPTURE to save a
 * frame. The context is OpenGL ES 3.0 under BIRD_GL_ES, else OpenGL 4.1
 * core.
 */
class HeadlessWindow final : public Window {
   public:
	/**
	 * @throws std::runtime_error when EGL or the context cannot be created.
	 */
	HeadlessWindow(uint32_t width, uint32_t height);
	~HeadlessWindow() override;

	HeadlessWindow(const HeadlessWindow&) = delete;
	HeadlessWindow& operator=(const HeadlessWindow&) = delete;

	bool shouldWindowClose() const override;
	void present() override;
	void pollEvents() override {}
	bool pollResize() override;
	void setCursorMode(CursorMode) override {}
	bool setRawMouseMotion(bool) override { return false; }

	std::string getClipboardText() override { return m_clipboard; }
	void setClipboardText(const std::string& text) override {
		m_clipboard = text;
	}
	void setCursor(UICursor) override {}

   private:
	void* m_display = nullptr;	// EGLDisplay
	void* m_context = nullptr;	// EGLContext
	void* m_surface = nullptr;	// EGLSurface
	uint32_t m_surfaceWidth;
	uint32_t m_surfaceHeight;
	uint64_t m_framesPresented = 0;
	uint64_t m_frameLimit;
	std::string m_clipboard;
};

}  // namespace bird::platform

#endif	// BIRD_HEADLESSWINDOW_H
