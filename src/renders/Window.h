//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_WINDOW_H
#define BIRD_WINDOW_H

#include <array>

#include "../scene/2D/UIPlatform.h"
#include "GraphicsConstants.h"


struct GLFWwindow;
struct GLFWcursor;

namespace bird {

    /** The application window; also the UI's clipboard and cursor service. */
    class Window : public UIPlatform {
    public:
        Window(uint32_t width, uint32_t height, std::string title);
        ~Window();

        bool shouldWindowClose() const;
        void pollWindow() const;
    	double static getGLFWTime();

        GLFWwindow* const getGLFWWindow() const;

        /**
         * Re-reads the window and framebuffer sizes. Polling once per frame
         * coalesces bursts of resize events into a single rebuild.
         * @return true when a size changed and the window is not minimized.
         */
        bool pollResize();
        /** @return framebuffer size in pixels, for render targets. */
        uint32_t getFramebufferWidth() const;
        uint32_t getFramebufferHeight() const;
        /** @return window size in screen coordinates, matching cursor input. */
        uint32_t getWidth() const;
        uint32_t getHeight() const;

        std::string getClipboardText() override;
        void setClipboardText(const std::string& text) override;
        void setCursor(UICursor cursor) override;
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

} // bird

#endif //BIRD_WINDOW_H
