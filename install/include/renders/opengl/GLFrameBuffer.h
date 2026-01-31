#include "../generic/FrameBuffer.h"
#include "GLShader.h"
#include "GLTexture.h"

namespace bird::gl {

class GLFrameBuffer : public FrameBuffer {
   public:
	GLFrameBuffer(uint32_t width, uint32_t height,
				  std::vector<std::shared_ptr<Texture>> textureAttachments,
				  std::shared_ptr<Shader> shader, uint8_t opts);
	virtual ~GLFrameBuffer();

	void bind(FrameBuffer* previousFrameBuffer);
	void bindAttachments();

   private:
	uint32_t m_fbo;
	std::vector<uint32_t> m_rbos;
};

}  // namespace bird::gl
