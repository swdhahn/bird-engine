#ifndef BIRD_ANDROIDWINDOW_H
#define BIRD_ANDROIDWINDOW_H

#include <EGL/egl.h>

#include <cstdint>
#include <string>

#include "../../renders/Window.h"

struct ANativeWindow;
struct AInputEvent;

namespace bird::platform {

/**
 * Window of a NativeActivity: an OpenGL ES 3.0 context on the activity's
 * ANativeWindow. Android destroys the native window whenever the app goes
 * to the background; the context survives and a new surface is attached
 * when the window returns, so GPU resources stay valid.
 *
 * Coordinates handed to the engine are density-independent (dp): pixels
 * divided by density / 160, so the UI keeps its physical size.
 */
class AndroidWindow final : public Window {
   public:
	/**
	 * @param nativeWindow The activity's current window.
	 * @throws std::runtime_error when EGL setup fails.
	 */
	explicit AndroidWindow(ANativeWindow* nativeWindow);
	~AndroidWindow() override;

	AndroidWindow(const AndroidWindow&) = delete;
	AndroidWindow& operator=(const AndroidWindow&) = delete;

	/** Binds a new native window after the app returns to the foreground. */
	void attachSurface(ANativeWindow* nativeWindow);
	/** Releases the surface before Android destroys the native window. */
	void detachSurface();
	bool hasSurface() const { return m_surface != EGL_NO_SURFACE; }

	/**
	 * Translates touch, mouse and key events for the listener.
	 * @return true when the event was consumed (the Back key is not).
	 */
	bool handleInputEvent(const AInputEvent* event);

	bool shouldWindowClose() const override;
	void present() override;
	void pollEvents() override;
	bool pollResize() override;
	void setCursorMode(CursorMode) override {}
	bool setRawMouseMotion(bool) override { return false; }

	std::string getClipboardText() override { return m_clipboard; }
	void setClipboardText(const std::string& text) override {
		m_clipboard = text;
	}
	void setCursor(UICursor) override {}

   private:
	void createSurface(ANativeWindow* nativeWindow);
	bool handleMotion(const AInputEvent* event);
	bool handleKey(const AInputEvent* event);

	EGLDisplay m_display = EGL_NO_DISPLAY;
	EGLConfig m_config = nullptr;
	EGLContext m_context = EGL_NO_CONTEXT;
	EGLSurface m_surface = EGL_NO_SURFACE;
	float m_scale = 1.0f;		  // pixels per dp
	int32_t m_primaryPointer = -1;  // pointer acting as the mouse
	std::string m_clipboard;
};

}  // namespace bird::platform

#endif	// BIRD_ANDROIDWINDOW_H
