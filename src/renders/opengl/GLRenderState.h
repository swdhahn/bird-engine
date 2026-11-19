#ifndef BIRD_GL_RENDER_STATE_H
#define BIRD_GL_RENDER_STATE_H

#include <cstdint>

namespace bird::gl {

/** Color blending configurations used by the passes. */
enum class GLBlend : uint8_t {
	None,
	/** src * a + dst * (1 - a). */
	Alpha,
	/** src * a + dst; transparent additive materials. */
	AdditiveAlpha,
	/** src + dst; light accumulation. */
	Additive
};

/** Faces discarded by the rasterizer. */
enum class GLCull : uint8_t { None, Back, Front };

/** Depth comparison functions used by the passes. */
enum class GLDepthFunc : uint8_t { Less, LessEqual, Greater, Always };

/**
 * Caches fixed-function state so redundant GL calls are skipped. Every pass
 * sets the full state it relies upon; nothing is assumed from a prior pass.
 * The cache must be invalidated after foreign code (UI) touches GL state.
 */
class GLRenderState {
   public:
	void setDepthTest(bool enabled);
	void setDepthWrite(bool enabled);
	void setDepthFunc(GLDepthFunc func);
	void setDepthClamp(bool enabled);
	void setBlend(GLBlend blend);
	void setCull(GLCull cull);
	void setPolygonOffset(bool enabled, float factor = 0.0f, float units = 0.0f);
	void setScissor(bool enabled);

	/** Forgets cached values, forcing the next calls through. */
	void invalidate();

   private:
	// -1 marks an unknown value.
	int8_t m_depthTest = -1;
	int8_t m_depthWrite = -1;
	int8_t m_depthFunc = -1;
	int8_t m_depthClamp = -1;
	int8_t m_blend = -1;
	int8_t m_cull = -1;
	int8_t m_polygonOffset = -1;
	int8_t m_scissor = -1;
	float m_offsetFactor = 0.0f;
	float m_offsetUnits = 0.0f;
};

}  // namespace bird::gl

#endif	// BIRD_GL_RENDER_STATE_H
