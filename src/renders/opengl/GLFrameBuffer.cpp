#include "GLFrameBuffer.h"

#include <GL/glew.h>

#include "GLTexture.h"
namespace bird::gl {

GLFrameBuffer::GLFrameBuffer(
	uint32_t width, uint32_t height,
	std::vector<std::shared_ptr<Texture>> textureAttachments,
	std::shared_ptr<Shader> shader, uint8_t opts)
	: FrameBuffer(width, height, textureAttachments, shader, opts) {
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	for (int i = 0; i < textureAttachments.size(); i++) {
		GLTexture* tex = ((GLTexture*)textureAttachments.data()[i].get());

		tex->bind();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
							   GL_TEXTURE_2D, tex->getTextureID(), 0);
		tex->unbind();
	}

	/*GLenum* buffers = new GLenum[textureAttachments.size()];
	for (int i = 0; i < textureAttachments.size(); i++) {
		buffers[i] = GL_COLOR_ATTACHMENT0 + i;
	}
	glDrawBuffers(textureAttachments.size(), buffers);
	delete[] buffers;*/

	bool hasDepth = (opts & FRAMEBUFFER_OPT_DEPTH);
	bool hasStencil = (opts & FRAMEBUFFER_OPT_STENCIL);

	if (hasDepth || hasStencil) {
		uint32_t rbo;

		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		m_rbos.emplace_back(rbo);

		if (hasDepth && hasStencil) {
			// PACKED FORMAT: High compatibility, fixes flickering
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width,
								  height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER,
									  GL_DEPTH_STENCIL_ATTACHMENT,
									  GL_RENDERBUFFER, rbo);
		} else if (hasDepth) {
			// DEPTH ONLY: Use COMPONENT24 for standard, COMPONENT32F only if
			// you need high precision
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width,
								  height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									  GL_RENDERBUFFER, rbo);
		} else if (hasStencil) {
			// STENCIL ONLY
			glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width,
								  height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
									  GL_RENDERBUFFER, rbo);
		}

		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Framebuffer was not created properly." << std::endl;
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLFrameBuffer::~GLFrameBuffer() {
	glDeleteRenderbuffers(m_rbos.size(), m_rbos.data());
	glDeleteFramebuffers(1, &m_fbo);
}

void GLFrameBuffer::bind(FrameBuffer* previousFrameBuffer) {
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLFrameBuffer::bindAttachments() {
	((GLShader*)m_shader.get())->bind();

	for (int i = 0; i < getColorAttachments().size(); i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		((GLTexture*)getColorAttachments().data()[i].get())->bind();
	}
}

}  // namespace bird::gl
