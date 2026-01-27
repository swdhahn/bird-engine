#include "FrameBuffer.h"

#include <memory>

#include "GLFrameBuffer.h"

namespace bird {

FrameBuffer::FrameBuffer(
	uint32_t width, uint32_t height,
	std::vector<std::shared_ptr<Texture>> textureAttachments,
	std::shared_ptr<Shader> shader, uint8_t opts)
	: m_textureAttachments(textureAttachments),
	  m_shader(shader),
	  m_opts(opts),
	  m_width(width),
	  m_height(height) {}

FrameBufferBuilder::FrameBufferBuilder() : m_opts(0) {}

std::vector<std::shared_ptr<Texture>>& FrameBuffer::getColorAttachments() {
	return m_textureAttachments;
}

std::shared_ptr<Shader> FrameBuffer::getShader() {
	return m_shader;
}

void FrameBufferBuilder::addTextureAttachment(
	std::shared_ptr<Texture> textureAttachment) {
	m_textureAttachments.emplace_back(textureAttachment);
}

void FrameBufferBuilder::setShader(std::shared_ptr<Shader> shader) {
	m_shader = shader;
}

void FrameBufferBuilder::addOpts(uint8_t opts) {
	m_opts |= opts;
}

std::unique_ptr<FrameBuffer> FrameBufferBuilder::build() {
	std::unique_ptr<FrameBuffer> ptr = nullptr;

	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			ptr = std::make_unique<gl::GLFrameBuffer>(
				m_width, m_height, m_textureAttachments, m_shader, m_opts);
			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support "
				"framebuffers...");
	}

	return std::move(ptr);
}

}  // namespace bird
