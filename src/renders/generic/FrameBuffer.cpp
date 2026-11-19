#include "FrameBuffer.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "GLFrameBuffer.h"

namespace bird {

FrameBuffer::FrameBuffer(
	uint32_t width, uint32_t height,
	std::vector<std::shared_ptr<Texture>> textureAttachments,
	std::shared_ptr<Texture> depthAttachment, std::shared_ptr<Shader> shader,
	uint8_t opts)
	: m_textureAttachments(std::move(textureAttachments)),
	  m_depthAttachment(std::move(depthAttachment)),
	  m_shader(shader),
	  m_opts(opts),
	  m_width(width),
	  m_height(height) {}

FrameBufferBuilder::FrameBufferBuilder() : m_opts(0) {}

std::vector<std::shared_ptr<Texture>>& FrameBuffer::getColorAttachments() {
	return m_textureAttachments;
}

const std::shared_ptr<Texture>& FrameBuffer::getDepthAttachment() const {
	return m_depthAttachment;
}

std::shared_ptr<Shader> FrameBuffer::getShader() {
	return m_shader;
}

FrameBufferBuilder& FrameBufferBuilder::addTextureAttachment(
	std::shared_ptr<Texture> textureAttachment) {
	m_textureAttachments.emplace_back(textureAttachment);
	return *this;
}

FrameBufferBuilder& FrameBufferBuilder::setDepthAttachment(
	std::shared_ptr<Texture> depthAttachment) {
	if (depthAttachment && !isDepthFormat(depthAttachment->getFormat())) {
		throw std::invalid_argument(
			"FrameBufferBuilder: depth attachment requires a depth format.");
	}
	m_depthAttachment = std::move(depthAttachment);
	return *this;
}

FrameBufferBuilder& FrameBufferBuilder::setShader(
	std::shared_ptr<Shader> shader) {
	m_shader = shader;
	return *this;
}

FrameBufferBuilder& FrameBufferBuilder::setSize(uint32_t width,
												uint32_t height) {
	m_width = width;
	m_height = height;
	return *this;
}

FrameBufferBuilder& FrameBufferBuilder::scaleSize(float factor) {
	m_width = std::max(1u, static_cast<uint32_t>(m_width * factor));
	m_height = std::max(1u, static_cast<uint32_t>(m_height * factor));
	return *this;
}

FrameBufferBuilder& FrameBufferBuilder::addOpts(uint8_t opts) {
	m_opts |= opts;
	return *this;
}

std::unique_ptr<FrameBuffer> FrameBufferBuilder::build() const {
	std::unique_ptr<FrameBuffer> ptr = nullptr;

	switch (CURRENT_GRAPHICS_PIPELINE) {
		case GRAPHICS_PIPELINE_OPENGL:
			ptr = std::make_unique<gl::GLFrameBuffer>(
				m_width, m_height, m_textureAttachments, m_depthAttachment,
				m_shader, m_opts);
			break;
		default:
			throw std::runtime_error(
				"No other graphics pipelines currently support "
				"framebuffers...");
	}

	return std::move(ptr);
}

}  // namespace bird
