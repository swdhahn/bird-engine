#ifndef BIRD_FRAME_BUFFER_H
#define BIRD_FRAME_BUFFER_H

#include <memory>
#include <vector>

#include "Shader.h"
#include "Texture.h"

namespace bird {

/**
 * Options configuring offscreen render target attachments.
 */
enum FrameBufferOptions {
	FRAMEBUFFER_OPT_NONE = 0,
	FRAMEBUFFER_OPT_DEPTH = 1,
	FRAMEBUFFER_OPT_STENCIL = 2
};

/**
 * Offscreen render target containing color texture attachments and depth/stencil buffers.
 */
class FrameBuffer {
   public:
	FrameBuffer(uint32_t width, uint32_t height,
				std::vector<std::shared_ptr<Texture>> textureAttachments,
				std::shared_ptr<Texture> depthAttachment,
				std::shared_ptr<Shader> shader, uint8_t opts);
	virtual ~FrameBuffer() {}

	/**
	 * Activates this framebuffer for rendering, setting up viewports and attachment targets,
	 * then clears color and depth.
	 * @param previousFrameBuffer Currently bound buffer to detach (or nullptr).
	 */
	virtual void bind(FrameBuffer* previousFrameBuffer) = 0;

	/** Activates this framebuffer and its viewport while preserving contents. */
	virtual void bindForDrawing() = 0;

	std::vector<std::shared_ptr<Texture>>& getColorAttachments();

	/** @return Sampleable depth texture, or nullptr when depth is a renderbuffer. */
	const std::shared_ptr<Texture>& getDepthAttachment() const;

	std::shared_ptr<Shader> getShader();
	uint32_t getWidth() const { return m_width; }
	uint32_t getHeight() const { return m_height; }

   protected:
	std::vector<std::shared_ptr<Texture>> m_textureAttachments;
	std::shared_ptr<Texture> m_depthAttachment;
	std::shared_ptr<Shader> m_shader;
	uint8_t m_opts;
	uint32_t m_width;
	uint32_t m_height;
};

/**
 * Fluent builder for creating offscreen FrameBuffer instances.
 */
class FrameBufferBuilder {
   public:
	FrameBufferBuilder();

	/** Sets explicit framebuffer dimensions in pixels. */
	void setSize(uint32_t width, uint32_t height);

	/** Scales current dimensions by a multiplier (e.g. 0.5f for half-res bloom). */
	void scaleSize(float factor);

	/** Adds a color texture attachment target. */
	void addTextureAttachment(std::shared_ptr<Texture> textureAttachments);

	/**
	 * Attaches a sampleable depth texture (FORMAT_DEPTH24_STENCIL8 or
	 * FORMAT_DEPTH32F) in place of a depth renderbuffer.
	 */
	void setDepthAttachment(std::shared_ptr<Texture> depthAttachment);

	/** Binds a post-processing shader associated with this framebuffer. */
	void setShader(std::shared_ptr<Shader> shader);

	/** Appends FrameBufferOptions bitmask flags (e.g. FRAMEBUFFER_OPT_DEPTH). */
	void addOpts(uint8_t opts);

	/** Instantiates the backend FrameBuffer. */
	std::unique_ptr<FrameBuffer> build();

   private:
	std::vector<std::shared_ptr<Texture>> m_textureAttachments;
	std::shared_ptr<Texture> m_depthAttachment;
	std::shared_ptr<Shader> m_shader;
	uint8_t m_opts;
	uint32_t m_width = WINDOW_WIDTH;
	uint32_t m_height = WINDOW_HEIGHT;
};

}  // namespace bird

#endif	// !BIRD_FRAME_BUFFER_H
