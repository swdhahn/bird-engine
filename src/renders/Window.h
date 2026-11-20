//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_WINDOW_H
#define BIRD_WINDOW_H

#include <cstdint>
#include <string>

#include "../scene/2D/UIPlatform.h"
#include "GraphicsConstants.h"

namespace bird {

/** Key and button transitions, matching the engine's ACTION_* values. */
enum class InputAction : uint8_t { Release = 0, Press = 1, Repeat = 2 };

/** How the platform presents and constrains the pointer. */
enum class CursorMode : uint8_t {
	Normal,	   // visible, free
	Hidden,	   // invisible over the window, free
	Disabled  // invisible and locked; motion stays unbounded
};

/**
 * Receives input events from a Window. Key and button codes use the
 * engine's KEY_* and MOUSE_BUTTON_* values (see util/Input.h); positions
 * are in window coordinates with +y down; modifiers use the UI bit layout
 * (shift 1, ctrl 2, alt 4, super 8).
 */
class WindowEventListener {
   public:
	virtual ~WindowEventListener() = default;

	virtual void onKey(int key, InputAction action, uint8_t modifiers) = 0;
	virtual void onCharacter(uint32_t codepoint) = 0;
	virtual void onMouseButton(int button, InputAction action,
							   uint8_t modifiers) = 0;
	virtual void onCursorMove(double x, double y) = 0;
	virtual void onScroll(double dx, double dy) = 0;
};

/**
 * Platform application window: owns the native surface and its graphics
 * context, presents frames, pumps operating system events into a
 * WindowEventListener, and serves the UI's platform needs (UIPlatform).
 *
 * Implementations live under src/platform/<platform>/ and are created
 * through platform::createWindow(). This base keeps the size bookkeeping
 * shared by every platform.
 */
class Window : public UIPlatform {
   public:
	~Window() override = default;

	/** @return true once the user or the system asked the app to close. */
	virtual bool shouldWindowClose() const = 0;

	/** Shows the frame rendered since the previous call. */
	virtual void present() = 0;

	/** Dispatches pending operating system events to the listener. */
	virtual void pollEvents() = 0;

	/**
	 * Re-reads the window and framebuffer sizes. Polling once per frame
	 * coalesces bursts of resize events into a single rebuild.
	 * @return true when a size changed and the window is not minimized.
	 */
	virtual bool pollResize() = 0;

	virtual void setCursorMode(CursorMode mode) = 0;

	/**
	 * Requests unaccelerated pointer motion.
	 * @return false when the platform cannot provide it.
	 */
	virtual bool setRawMouseMotion(bool enabled) = 0;

	/** Routes input events to a listener; nullptr stops delivery. */
	void setEventListener(WindowEventListener* listener) {
		m_listener = listener;
	}

	/** @return Framebuffer render target width in physical pixels. */
	uint32_t getFramebufferWidth() const { return m_framebufferWidth; }
	/** @return Framebuffer render target height in physical pixels. */
	uint32_t getFramebufferHeight() const { return m_framebufferHeight; }
	/** @return Window width in screen coordinates, matching cursor input. */
	uint32_t getWidth() const { return m_width; }
	/** @return Window height in screen coordinates, matching cursor input. */
	uint32_t getHeight() const { return m_height; }

   protected:
	/**
	 * Records freshly queried sizes. A zero dimension (a minimized window)
	 * keeps the last usable size.
	 * @return true when a size changed and every dimension is positive.
	 */
	bool updateSizes(int framebufferWidth, int framebufferHeight, int width,
					 int height);

	WindowEventListener* listener() const { return m_listener; }

   private:
	WindowEventListener* m_listener = nullptr;
	// Zero until the first pollResize, which therefore always reports.
	uint32_t m_framebufferWidth = 0;
	uint32_t m_framebufferHeight = 0;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
};

}  // namespace bird

#endif	// BIRD_WINDOW_H
