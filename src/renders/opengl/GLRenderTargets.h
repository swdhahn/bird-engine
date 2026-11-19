#ifndef BIRD_GL_RENDER_TARGETS_H
#define BIRD_GL_RENDER_TARGETS_H

#include <cstdint>
#include <memory>

#include "../generic/Shader.h"
#include "GLFrameBuffer.h"

namespace bird::gl {

/**
 * Screen-sized render targets for one frame.
 *
 *   gBuffer  RT0 albedo+AO (sRGB8A8), RT1 normal+roughness+metallic
 *            (RGBA16F), RT2 HDR accumulation (RGBA16F), sampleable depth
 *   hdr      RT0 = the same HDR texture, plus its own depth renderbuffer
 *            that receives a copy of the G-buffer depth, so light volumes
 *            can depth-test while sampling the G-buffer depth texture
 *   ldr      tone-mapped RGBA8 input to FXAA
 */
class GLRenderTargets {
   public:
	/**
	 * Allocates every target at the given size.
	 * @param postShader Shader the LDR target presents with (FXAA).
	 * @throws std::runtime_error when a framebuffer is incomplete.
	 */
	GLRenderTargets(uint32_t width, uint32_t height,
					std::shared_ptr<Shader> postShader);

	GLFrameBuffer& gBuffer() { return *m_gBuffer; }
	GLFrameBuffer& hdr() { return *m_hdr; }
	GLFrameBuffer& ldr() { return *m_ldr; }

	const Texture& albedoOcclusion() const { return *m_albedoOcclusion; }
	const Texture& normalRoughnessMetallic() const {
		return *m_normalRoughnessMetallic;
	}
	const Texture& hdrColor() const { return *m_hdrColor; }
	const Texture& depth() const { return *m_depth; }
	const Texture& ldrColor() const { return *m_ldrColor; }

	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }

   private:
	uint32_t m_width;
	uint32_t m_height;
	std::shared_ptr<Texture> m_albedoOcclusion;
	std::shared_ptr<Texture> m_normalRoughnessMetallic;
	std::shared_ptr<Texture> m_hdrColor;
	std::shared_ptr<Texture> m_depth;
	std::shared_ptr<Texture> m_ldrColor;
	std::unique_ptr<GLFrameBuffer> m_gBuffer;
	std::unique_ptr<GLFrameBuffer> m_hdr;
	std::unique_ptr<GLFrameBuffer> m_ldr;
};

}  // namespace bird::gl

#endif	// BIRD_GL_RENDER_TARGETS_H
