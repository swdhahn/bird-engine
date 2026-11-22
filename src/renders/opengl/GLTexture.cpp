//
// Created by Seth Hahn on 11/17/22.
//

#include "GLTexture.h"

#include "GLApi.h"

#include "../GraphicsConstants.h"

namespace bird::gl {

GLTexture::GLTexture(uint8_t* pData, uint32_t width, uint32_t height,
					 TextureFormat format, uint32_t textureID)
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

uint32_t GLTexture::getTextureID() {
	return m_textureID;
}

void GLTexture::swapFrom(Texture& other) {
	Texture::swapFrom(other);
	auto* otherGL = dynamic_cast<GLTexture*>(&other);
	if (!otherGL) return;
	std::swap(m_textureID, otherGL->m_textureID);
}

}  // namespace bird::gl
