//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_WINDOW_H
#define BIRD_WINDOW_H

#include <array>
#include <string>

#include "../scene/2D/UIPlatform.h"
#include "GraphicsConstants.h"

struct GLFWwindow;
struct GLFWcursor;

namespace bird {

/**
 * Desktop application window, event source, and UIPlatform implementation.
 *
 * Wraps a GLFW window instance, managing display resolution, DPI scaling,
 * double buffering, OS clipboard text access, and standard hardware cursors.
 */
class Window : public UIPlatform {
public:
    /**
     * Creates a desktop window and initializes GLFW context.
     * @param width Initial width in screen points.
     * @param height Initial height in screen points.
     * @param title Window caption text.
     */
    Window(uint32_t width, uint32_t height, std::string title);
    ~Window();

    /** Returns true when the user requested closing the window (e.g. close button or Alt+F4). */
    bool shouldWindowClose() const;

    /** Polls OS window events from GLFW. */
    void pollWindow() const;

    /** Returns current GLFW wall clock time in seconds. */
    static double getGLFWTime();

    /** Underlying GLFW handle, for input callbacks and context calls. */
    GLFWwindow* const getGLFWWindow() const;

    /**
     * Re-reads the window and framebuffer sizes. Polling once per frame
     * coalesces bursts of resize events into a single rebuild.
     * @return true when a size changed and the window is not minimized.
     */
    bool pollResize();

    /** @return Framebuffer render target width in physical pixels. */
    uint32_t getFramebufferWidth() const;

    /** @return Framebuffer render target height in physical pixels. */
    uint32_t getFramebufferHeight() const;

    /** @return Window width in screen coordinates, matching cursor input. */
    uint32_t getWidth() const;

    /** @return Window height in screen coordinates, matching cursor input. */
    uint32_t getHeight() const;

    /** Retrieves textual content from the operating system clipboard. */
    std::string getClipboardText() override;

    /** Replaces the operating system clipboard text. */
    void setClipboardText(const std::string& text) override;

    /** Updates the active platform cursor (arrow, text beam, resize, etc.). */
    void setCursor(UICursor cursor) override;

    /** Human-readable name of a GLFW key code ("W", "Space", "F5"), for key bindings. */
    std::string keyName(int keyCode) override;

private:
    GLFWwindow* m_pWindow;
    // Standard cursors, created on first use and owned by the window.
    std::array<GLFWcursor*, 6> m_cursors{};
    // Zero until the first pollResize, which therefore always reports.
    uint32_t m_framebufferWidth = 0;
    uint32_t m_framebufferHeight = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace bird

#endif // BIRD_WINDOW_H
