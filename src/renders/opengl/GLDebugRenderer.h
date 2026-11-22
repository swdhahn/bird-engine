#ifndef BIRD_GLDEBUGRENDERER_H
#define BIRD_GLDEBUGRENDERER_H

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "GLBuffer.h"

namespace bird {
class DebugDrawList;
}

namespace bird::gl {

/**
 * Draws a DebugDrawList as GL_LINES: transient lines from one dynamic
 * buffer re-filled every frame, and each cached layer from its own buffer,
 * re-uploaded only when the layer's version changes. The caller binds the
 * debug_line shader and the global UBO.
 */
class GLDebugRenderer {
   public:
	GLDebugRenderer() = default;
	~GLDebugRenderer() = default;

	GLDebugRenderer(const GLDebugRenderer&) = delete;
	GLDebugRenderer& operator=(const GLDebugRenderer&) = delete;

	void draw(const DebugDrawList& list);

   private:
	/** A VAO plus vertex buffer holding one set of lines. */
	struct LineBuffer {
		LineBuffer();
		~LineBuffer();
		void upload(const void* vertices, size_t count);
		void draw() const;

		uint32_t vao = 0;
		std::unique_ptr<GLBuffer> vertices;
		size_t count = 0;
		uint64_t version = 0;
	};

	std::unique_ptr<LineBuffer> m_transient;
	std::unique_ptr<LineBuffer> m_overlay;
	std::unordered_map<const void*, std::unique_ptr<LineBuffer>> m_layers;
};

}  // namespace bird::gl

#endif	// !BIRD_GLDEBUGRENDERER_H
