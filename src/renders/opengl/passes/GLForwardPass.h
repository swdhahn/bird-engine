#ifndef BIRD_GL_FORWARD_PASS_H
#define BIRD_GL_FORWARD_PASS_H

#include <memory>
#include <vector>

#include "../../../scene/components/ParticleEmitter.h"
#include "../../generic/RenderQueue.h"
#include "../GLParticleRenderer.h"
#include "../GLRenderState.h"
#include "../GLRenderTargets.h"
#include "../GLSkinning.h"

namespace bird {
class ResourceManager;
}

namespace bird::gl {

/**
 * Shades what deferred shading cannot: unlit and force-forward opaque
 * materials, then alpha-blended and additive materials back to front, followed
 * by batched camera-facing billboard particles. Draws into the HDR target
 * against the copied opaque depth. Expects the light, shadow, and atlas
 * bindings already in place.
 */
class GLForwardPass {
   public:
	/**
	 * Loads the pass's mesh and particle shader programs.
	 * @param resources Shared cache through which the programs load.
	 * @param skinning Shared palette block used by skinned draws.
	 */
	GLForwardPass(ResourceManager& resources, GLSkinningBlock& skinning);

	/**
	 * Executes forward opaque draws, sorted transparent mesh draws, and
	 * batched CPU billboard particle emitters into targets.hdr().
	 *
	 * @param queue Frame render queue holding ForwardOpaque and Transparent buckets.
	 * @param particleEmitters Visible particle emitters that passed frustum culling.
	 * @param cameraView Active camera view matrix (used for billboard orientation).
	 * @param targets Frame render targets.
	 * @param state Cached GL state tracker.
	 */
	void execute(const RenderQueue& queue,
				 const std::vector<ParticleEmitter*>& particleEmitters,
				 const Matrix4& cameraView, GLRenderTargets& targets,
				 GLRenderState& state);

   private:
	GLMeshPrograms m_programs;
	GLParticleRenderer m_particleRenderer;
};

}  // namespace bird::gl

#endif	// BIRD_GL_FORWARD_PASS_H
