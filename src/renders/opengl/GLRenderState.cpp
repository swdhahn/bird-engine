#include "GLRenderState.h"

#include <GL/glew.h>

namespace bird::gl {

namespace {

void toggle(GLenum capability, bool enabled) {
	if (enabled) {
		glEnable(capability);
	} else {
		glDisable(capability);
	}
}

/** Updates a cached flag; returns true when GL must be told. */
template <typename T>
bool changed(int8_t& cached, T value) {
	const auto next = static_cast<int8_t>(value);
	if (cached == next) return false;
	cached = next;
	return true;
}

}  // namespace

void GLRenderState::setDepthTest(bool enabled) {
	if (changed(m_depthTest, enabled)) toggle(GL_DEPTH_TEST, enabled);
}

void GLRenderState::setDepthWrite(bool enabled) {
	if (changed(m_depthWrite, enabled)) glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void GLRenderState::setDepthFunc(GLDepthFunc func) {
	if (!changed(m_depthFunc, func)) return;
	switch (func) {
		case GLDepthFunc::Less:
			glDepthFunc(GL_LESS);
			break;
		case GLDepthFunc::LessEqual:
			glDepthFunc(GL_LEQUAL);
			break;
		case GLDepthFunc::Greater:
			glDepthFunc(GL_GREATER);
			break;
		case GLDepthFunc::Always:
			glDepthFunc(GL_ALWAYS);
			break;
	}
}

void GLRenderState::setDepthClamp(bool enabled) {
	if (changed(m_depthClamp, enabled)) toggle(GL_DEPTH_CLAMP, enabled);
}

void GLRenderState::setBlend(GLBlend blend) {
	if (!changed(m_blend, blend)) return;
	toggle(GL_BLEND, blend != GLBlend::None);
	switch (blend) {
		case GLBlend::Alpha:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case GLBlend::AdditiveAlpha:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;
		case GLBlend::Additive:
			glBlendFunc(GL_ONE, GL_ONE);
			break;
		case GLBlend::None:
			break;
	}
}

void GLRenderState::setCull(GLCull cull) {
	if (!changed(m_cull, cull)) return;
	toggle(GL_CULL_FACE, cull != GLCull::None);
	if (cull == GLCull::Back) glCullFace(GL_BACK);
	if (cull == GLCull::Front) glCullFace(GL_FRONT);
}

void GLRenderState::setPolygonOffset(bool enabled, float factor, float units) {
	const bool toggled = changed(m_polygonOffset, enabled);
	if (toggled) toggle(GL_POLYGON_OFFSET_FILL, enabled);
	if (enabled && (toggled || factor != m_offsetFactor || units != m_offsetUnits)) {
		glPolygonOffset(factor, units);
		m_offsetFactor = factor;
		m_offsetUnits = units;
	}
}

void GLRenderState::setScissor(bool enabled) {
	if (changed(m_scissor, enabled)) toggle(GL_SCISSOR_TEST, enabled);
}

void GLRenderState::invalidate() {
	*this = GLRenderState();
}

}  // namespace bird::gl
