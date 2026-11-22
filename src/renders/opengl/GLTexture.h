//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_GLTEXTURE_H
#define BIRD_GLTEXTURE_H

#include "../generic/Texture.h"

namespace bird::gl {

/**
 * OpenGL 2D texture implementation.
 *
 * Wraps a native OpenGL texture object (GL_TEXTURE_2D) with filtering,
 * mipmapping, and active texture unit binding.
 */
class GLTexture : public Texture {
   public:
	GLTexture(uint8_t* pData, uint32_t width, uint32_t height,
			  TextureFormat format, uint32_t textureID);
	~GLTexture() override;

	/** Binds this texture to the active OpenGL texture target. */
	void bind();

	/** Unbinds GL_TEXTURE_2D from the active texture unit. */
	void unbind();

	uint32_t getTextureID();

	/**
	 * Swaps the OpenGL texture object ID alongside base pixel/dimension state
	 * in place. Called on the main GL thread during hot-reload.
	 */
	void swapFrom(Texture& other) override;

   private:
	uint32_t m_textureID;
};

}  // namespace bird::gl

#endif	// BIRD_GLTEXTURE_H
