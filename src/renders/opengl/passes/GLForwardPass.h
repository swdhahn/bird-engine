#ifndef BIRD_GL_FORWARD_PASS_H
#define BIRD_GL_FORWARD_PASS_H

#include <memory>

#include "../../generic/RenderQueue.h"
#include "../GLRenderState.h"
#include "../GLRenderTargets.h"

namespace bird::gl {

/**
 * Shades what deferred shading cannot: unlit and force-forward opaque
 * materials, then alpha-blended and additive materials back to front. Draws
 * into the HDR target against the copied opaque depth. Expects the light,
 * shadow, and atlas bindings already in place.
 */
class GLForwardPass {
   public:
	GLForwardPass();

	void execute(const RenderQueue& queue, GLRenderTargets& targets,
				 GLRenderState& state);

   private:
	std::shared_ptr<Shader> m_shader;
};

}  // namespace bird::gl

#endif	// BIRD_GL_FORWARD_PASS_H
