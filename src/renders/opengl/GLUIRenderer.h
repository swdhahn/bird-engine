#ifndef BIRD_GLUIRENDERER_H
#define BIRD_GLUIRENDERER_H

#include <cstdint>
#include <memory>

#include "GLBuffer.h"

namespace bird {
class UIDrawList;
class Texture;
}

namespace bird::gl {

/**
 * Uploads a UIDrawList into one interleaved vertex buffer and draws it in
 * a single call. Buffers grow geometrically and are reused across frames.
 * The caller binds the UI shader, global UBO and atlas texture.
 */
class GLUIRenderer {
   public:
	GLUIRenderer();
	~GLUIRenderer();
	GLUIRenderer(const GLUIRenderer&) = delete;
	GLUIRenderer& operator=(const GLUIRenderer&) = delete;

	/**
	 * Draws every command of the list; commands without a texture use the
	 * font atlas. Binds textures on unit 0.
	 */
	void draw(const UIDrawList& list, const Texture* atlas);

   private:
	uint32_t m_vao = 0;
	std::unique_ptr<GLBuffer> m_vertices;
	std::unique_ptr<GLBuffer> m_indices;
};

}  // namespace bird::gl

#endif	// !BIRD_GLUIRENDERER_H
