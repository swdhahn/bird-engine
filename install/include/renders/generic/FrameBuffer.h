#ifndef BIRD_FRAME_BUFFER_H
#define BIRD_FRAME_BUFFER_H

#include <memory>

#include "Shader.h"
#include "Texture.h"

namespace bird {

enum FrameBufferOptions {
	FRAMEBUFFER_OPT_NONE = 0,
	FRAMEBUFFER_OPT_DEPTH = 1,
	FRAMEBUFFER_OPT_STENCIL = 2
};

class FrameBuffer {
   public:
	FrameBuffer(uint32_t width, uint32_t height,
				std::vector<std::shared_ptr<Texture>> textureAttachments,
				std::shared_ptr<Shader> shader, uint8_t opts);
	virtual ~FrameBuffer() {}

	virtual void bind(FrameBuffer* previousFrameBuffer) = 0;
	std::vector<std::shared_ptr<Texture>>& getColorAttachments();
	std::shared_ptr<Shader> getShader();

   protected:
	std::vector<std::shared_ptr<Texture>> m_textureAttachments;
	std::shared_ptr<Shader> m_shader;
	uint8_t m_opts;
	uint32_t m_width;
	uint32_t m_height;
};

class FrameBufferBuilder {
   public:
	FrameBufferBuilder();
	void setSize(uint32_t width, uint32_t height);
	void scaleSize(float factor);
	void addTextureAttachment(std::shared_ptr<Texture> textureAttachments);
	void setShader(std::shared_ptr<Shader> shader);
	void addOpts(uint8_t opts);

	std::unique_ptr<FrameBuffer> build();

   private:
	std::vector<std::shared_ptr<Texture>> m_textureAttachments;
	std::shared_ptr<Shader> m_shader;
	uint8_t m_opts;
	uint32_t m_width = WINDOW_WIDTH;
	uint32_t m_height = WINDOW_HEIGHT;
};

}  // namespace bird

#endif	// !BIRD_FRAME_BUFFER_H
