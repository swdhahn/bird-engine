#ifndef BIRD_GL_GBUFFER_PASS_H
#define BIRD_GL_GBUFFER_PASS_H

#include <memory>
#include <vector>

#include "../../generic/RenderQueue.h"
#include "../GLRenderState.h"
#include "../GLRenderTargets.h"
#include "../GLSkinning.h"

namespace bird {
class ResourceManager;
}

namespace bird::gl {

/**
 * Writes opaque and masked lit surfaces into the G-buffer, seeding the HDR
 * target with emissive light and the background with the clear color.
 */
class GLGBufferPass {
   public:
	/**
	 * Loads the pass's shader programs.
	 * @param resources Shared cache through which the programs load.
	 */
	/**
	 * @param skinning Shared palette block used by skinned draws.
	 */
	GLGBufferPass(ResourceManager& resources, GLSkinningBlock& skinning);

	/**
	 * @param items Deferred bucket, sorted by material then depth.
	 * @param clearColor Linear background color.
	 */
	void execute(const std::vector<RenderItem>& items, GLRenderTargets& targets,
				 const Vector3& clearColor, GLRenderState& state);

   private:
	GLMeshPrograms m_programs;
};

}  // namespace bird::gl

#endif	// BIRD_GL_GBUFFER_PASS_H
