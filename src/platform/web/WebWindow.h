#ifndef BIRD_WEBWINDOW_H
#define BIRD_WEBWINDOW_H

#include <emscripten/html5.h>

#include <cstdint>
#include <string>

#include "../../renders/Window.h"

namespace bird::platform {

/**
 * Browser window: a WebGL 2 context on the page's <canvas id="canvas">.
 * The canvas follows its CSS size; its drawing buffer uses device pixels
 * (CSS size times devicePixelRatio), while input and UI coordinates stay in
 * CSS pixels. The browser presents each frame when the frame callback
 * returns and delivers input between frames, so present() and pollEvents()
 * have nothing to do.
 *
 * A disabled cursor maps to the Pointer Lock API; browsers grant the lock
 * only during a user gesture, so it takes effect on the next click.
 */
class WebWindow final : public Window {
   public:
	/** @throws std::runtime_error when WebGL 2 is unavailable. */
	WebWindow();
	~WebWindow() override;

	WebWindow(const WebWindow&) = delete;
	WebWindow& operator=(const WebWindow&) = delete;

	bool shouldWindowClose() const override { return false; }
	void present() override {}
	void pollEvents() override {}
	bool pollResize() override;
	void setCursorMode(CursorMode mode) override;
	bool setRawMouseMotion(bool) override { return false; }

	std::string getClipboardText() override { return m_clipboard; }
	void setClipboardText(const std::string& text) override {
		m_clipboard = text;
	}
	void setCursor(UICursor cursor) override;

   private:
	static EM_BOOL onKey(int eventType, const EmscriptenKeyboardEvent* event,
						 void* self);
	static EM_BOOL onMouseButton(int eventType,
								 const EmscriptenMouseEvent* event,
								 void* self);
	static EM_BOOL onMouseMove(int eventType, const EmscriptenMouseEvent* event,
							   void* self);
	static EM_BOOL onWheel(int eventType, const EmscriptenWheelEvent* event,
						   void* self);
	static EM_BOOL onTouch(int eventType, const EmscriptenTouchEvent* event,
						   void* self);

	/** Updates the cursor position from a mouse event, in CSS pixels. */
	void trackPointer(const EmscriptenMouseEvent& event);
	void applyCssCursor(const char* name);

	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE m_context = 0;
	CursorMode m_cursorMode = CursorMode::Normal;
	UICursor m_uiCursor = UICursor::Arrow;
	double m_cursorX = 0.0, m_cursorY = 0.0;
	int32_t m_primaryTouch = -1;  // touch acting as the mouse
	std::string m_clipboard;
};

}  // namespace bird::platform

#endif	// BIRD_WEBWINDOW_H
