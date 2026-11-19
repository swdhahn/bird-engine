#include "GLFrameBuffer.h"

#include <GL/glew.h>

#include <stdexcept>
#include <string>

#include "GLTexture.h"
namespace bird::gl {

GLFrameBuffer::GLFrameBuffer(
	uint32_t width, uint32_t height,
	std::vector<std::shared_ptr<Texture>> textureAttachments,
	std::shared_ptr<Texture> depthAttachment, std::shared_ptr<Shader> shader,
	uint8_t opts)
	: FrameBuffer(width, height, std::move(textureAttachments),
				  std::move(depthAttachment), std::move(shader), opts) {
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	const size_t colorCount = m_textureAttachments.size();
	for (size_t i = 0; i < colorCount; i++) {
		auto* tex = static_cast<GLTexture*>(m_textureAttachments[i].get());
		glFramebufferTexture2D(GL_FRAMEBUFFER,
							   GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i),
							   GL_TEXTURE_2D, tex->getTextureID(), 0);
	}

	if (colorCount == 0) {
		// Depth-only targets (shadow atlases) write no color.
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	} else {
		std::vector<GLenum> buffers(colorCount);
		for (size_t i = 0; i < colorCount; i++) {
			buffers[i] = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
		}
		glDrawBuffers(static_cast<GLsizei>(colorCount), buffers.data());
	}

	if (m_depthAttachment) {
		auto* depth = static_cast<GLTexture*>(m_depthAttachment.get());
		const GLenum attachment =
			(m_depthAttachment->getFormat() & FORMAT_DEPTH24_STENCIL8)
				? GL_DEPTH_STENCIL_ATTACHMENT
				: GL_DEPTH_ATTACHMENT;
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
							   depth->getTextureID(), 0);
	} else {
		createDepthRenderbuffer(width, height, opts);
	}

	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		throw std::runtime_error("GLFrameBuffer incomplete, status 0x" +
								 std::to_string(status));
	}
}

void GLFrameBuffer::createDepthRenderbuffer(uint32_t width, uint32_t height,
											uint8_t opts) {
	const bool hasDepth = (opts & FRAMEBUFFER_OPT_DEPTH);
	const bool hasStencil = (opts & FRAMEBUFFER_OPT_STENCIL);
	if (!hasDepth && !hasStencil) return;

	uint32_t rbo;
	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	m_rbos.emplace_back(rbo);

	GLenum storage = GL_DEPTH24_STENCIL8;
	GLenum attachment = GL_DEPTH_STENCIL_ATTACHMENT;
	if (hasDepth && !hasStencil) {
		storage = GL_DEPTH_COMPONENT24;
		attachment = GL_DEPTH_ATTACHMENT;
	} else if (!hasDepth && hasStencil) {
		storage = GL_STENCIL_INDEX8;
		attachment = GL_STENCIL_ATTACHMENT;
	}
	glRenderbufferStorage(GL_RENDERBUFFER, storage, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

GLFrameBuffer::~GLFrameBuffer() {
	glDeleteRenderbuffers(static_cast<GLsizei>(m_rbos.size()), m_rbos.data());
	glDeleteFramebuffers(1, &m_fbo);
}

void GLFrameBuffer::bind(FrameBuffer* previousFrameBuffer) {
	bindForDrawing();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void GLFrameBuffer::bindForDrawing() {
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);
}

void GLFrameBuffer::blitDepthTo(const GLFrameBuffer& target) const {
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.m_fbo);
	glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, target.m_width,
					  target.m_height,
					  GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFrameBuffer::bindAttachments() {
	static_cast<GLShader*>(m_shader.get())->bind();

	for (size_t i = 0; i < getColorAttachments().size(); i++) {
		glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
		static_cast<GLTexture*>(getColorAttachments()[i].get())->bind();
	}
}

}  // namespace bird::gl
