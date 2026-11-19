#ifndef BIRD_GL_GBUFFER_PASS_H
#define BIRD_GL_GBUFFER_PASS_H

#include <memory>
#include <vector>

#include "../../generic/RenderQueue.h"
#include "../GLRenderState.h"
#include "../GLRenderTargets.h"

namespace bird::gl {

/**
 * Writes opaque and masked lit surfaces into the G-buffer, seeding the HDR
 * target with emissive light and the background with the clear color.
 */
class GLGBufferPass {
   public:
	GLGBufferPass();

	/**
	 * @param items Deferred bucket, sorted by material then depth.
	 * @param clearColor Linear background color.
	 */
	void execute(const std::vector<RenderItem>& items, GLRenderTargets& targets,
				 const Vector3& clearColor, GLRenderState& state);

   private:
	std::shared_ptr<Shader> m_shader;
};

}  // namespace bird::gl

#endif	// BIRD_GL_GBUFFER_PASS_H
