#ifndef BIRD_GL_SKYBOX_PASS_H
#define BIRD_GL_SKYBOX_PASS_H

#include <memory>

#include "../../generic/EnvironmentMap.h"
#include "../GLRenderState.h"
#include "../GLRenderTargets.h"

namespace bird {
class ResourceManager;
class Shader;
}  // namespace bird

namespace bird::gl {

/**
 * Renders the active EnvironmentMap's HDR cubemap as an infinite-distance
 * skybox into the HDR target.
 *
 * Uses the xyww depth trick with GL_LEQUAL depth testing and depth writes
 * disabled so only background pixels (depth == 1.0) are shaded. Runs after
 * deferred lighting and before the forward transparency pass so transparent
 * surfaces and particles blend cleanly over the sky.
 */
class GLSkyboxPass {
   public:
	/**
	 * Loads the skybox shader program through the shared ResourceManager.
	 * @param resources Shared resource cache.
	 */
	explicit GLSkyboxPass(ResourceManager& resources);

	/**
	 * Draws the skybox into targets.hdr() when an environment map is active.
	 * Does nothing if environment is nullptr.
	 *
	 * @param environment Active environment map, or nullptr to skip.
	 * @param targets Frame render targets (draws into targets.hdr()).
	 * @param state Cached GL state tracker.
	 */
	void execute(const EnvironmentMap* environment, GLRenderTargets& targets,
				 GLRenderState& state);

   private:
	std::shared_ptr<Shader> m_shader;
};

}  // namespace bird::gl

#endif	// BIRD_GL_SKYBOX_PASS_H
