#include "GLRenderTargets.h"

namespace bird::gl {

namespace {

std::shared_ptr<Texture> makeTarget(uint32_t width, uint32_t height,
									TextureFormat format, bool srgb = false) {
	TextureBuilder builder(nullptr, width, height, format);
	builder.clampToEdge();
	if (srgb) builder.srgb();
	return builder.build();
}

std::unique_ptr<GLFrameBuffer> makeFrameBuffer(
	uint32_t width, uint32_t height,
	std::vector<std::shared_ptr<Texture>> colors,
	std::shared_ptr<Texture> depth, std::shared_ptr<Shader> shader,
	uint8_t opts) {
	return std::make_unique<GLFrameBuffer>(width, height, std::move(colors),
										   std::move(depth), std::move(shader),
										   opts);
}

}  // namespace

GLRenderTargets::GLRenderTargets(uint32_t width, uint32_t height,
								 std::shared_ptr<Shader> postShader)
	: m_width(width), m_height(height) {
	m_albedoOcclusion = makeTarget(width, height, FORMAT_8RGBA, true);
	m_normalRoughnessMetallic = makeTarget(width, height, FORMAT_RGBA16F);
	m_hdrColor = makeTarget(width, height, FORMAT_RGBA16F);
	m_depth = makeTarget(width, height, FORMAT_DEPTH24_STENCIL8);
	m_ldrColor = makeTarget(width, height, FORMAT_8RGBA);

	m_gBuffer = makeFrameBuffer(
		width, height,
		{m_albedoOcclusion, m_normalRoughnessMetallic, m_hdrColor}, m_depth,
		nullptr, FRAMEBUFFER_OPT_NONE);
	m_hdr = makeFrameBuffer(width, height, {m_hdrColor}, nullptr, nullptr,
							FRAMEBUFFER_OPT_DEPTH | FRAMEBUFFER_OPT_STENCIL);
	m_ldr = makeFrameBuffer(width, height, {m_ldrColor}, nullptr,
							std::move(postShader), FRAMEBUFFER_OPT_NONE);
}

}  // namespace bird::gl
