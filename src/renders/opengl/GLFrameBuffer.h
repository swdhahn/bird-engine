#ifndef BIRD_GLFRAMEBUFFER_H
#define BIRD_GLFRAMEBUFFER_H

#include <vector>
#include <memory>
#include <cstdint>

#include "../generic/FrameBuffer.h"
#include "GLShader.h"
#include "GLTexture.h"

namespace bird::gl {

/**
 * OpenGL Framebuffer Object (FBO) implementation.
 *
 * Attaches GLTexture color targets and creates Renderbuffer Objects (RBOs)
 * for hardware depth and stencil buffers.
 */
class GLFrameBuffer : public FrameBuffer {
   public:
	GLFrameBuffer(uint32_t width, uint32_t height,
				  std::vector<std::shared_ptr<Texture>> textureAttachments,
				  std::shared_ptr<Shader> shader, uint8_t opts);
	virtual ~GLFrameBuffer();

	/** Binds this FBO for offscreen rendering. */
	void bind(FrameBuffer* previousFrameBuffer) override;

	/** Binds attached color textures to consecutive texture units. */
	void bindAttachments();

   private:
	uint32_t m_fbo;
	std::vector<uint32_t> m_rbos;
};

}  // namespace bird::gl

#endif // BIRD_GLFRAMEBUFFER_H
