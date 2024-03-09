//
// Created by Seth Hahn on 11/17/22.
//

#include "GLTexture.h"
#include "../GraphicsConstants.h"

namespace bird::gl {

    GLTexture::GLTexture(void* pData, uint32_t width, uint32_t height, TextureFormat format, uint32_t textureID)
    : Texture(pData, width, height, format), m_textureID(textureID) {}

    GLTexture::~GLTexture() {
        glDeleteTextures(1, &m_textureID);
    }


    void GLTexture::bind() {
        glBindTexture(GL_TEXTURE_2D, m_textureID);
    }

    void GLTexture::unbind() {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // bird