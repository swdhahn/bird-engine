//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_WINDOW_H
#define BIRD_WINDOW_H

#include "GraphicsConstants.h"

namespace bird {

    class Window {
    public:
        Window(uint32_t width, uint32_t height, std::string title);
        ~Window();

        bool shouldWindowClose() const;
        void pollWindow() const;

        GLFWwindow* const getGLFWWindow() const;

    private:
        GLFWwindow* m_pWindow;

    };

} // bird

#endif //BIRD_WINDOW_H
