#ifndef BIRD_GL_FORWARD_PASS_H
#define BIRD_GL_FORWARD_PASS_H

#include <memory>

#include "../../generic/RenderQueue.h"
#include "../GLRenderState.h"
#include "../GLRenderTargets.h"
#include "../GLSkinning.h"

namespace bird {
class ResourceManager;
}

namespace bird::gl {

/**
 * Shades what deferred shading cannot: unlit and force-forward opaque
 * materials, then alpha-blended and additive materials back to front. Draws
 * into the HDR target against the copied opaque depth. Expects the light,
 * shadow, and atlas bindings already in place.
 */
class GLForwardPass {
   public:
	/**
	 * Loads the pass's shader programs.
	 * @param resources Shared cache through which the programs load.
	 */
	/**
	 * @param skinning Shared palette block used by skinned draws.
	 */
	GLForwardPass(ResourceManager& resources, GLSkinningBlock& skinning);

	void execute(const RenderQueue& queue, GLRenderTargets& targets,
				 GLRenderState& state);

   private:
	GLMeshPrograms m_programs;
};

}  // namespace bird::gl

#endif	// BIRD_GL_FORWARD_PASS_H
