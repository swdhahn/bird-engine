#ifndef BIRD_GL_LIGHTING_PASS_H
#define BIRD_GL_LIGHTING_PASS_H

#include <memory>
#include <vector>

#include "../../generic/LightList.h"
#include "../GLRenderState.h"
#include "../GLRenderTargets.h"

namespace bird {
class ResourceManager;
}

namespace bird::gl {

/**
 * Accumulates lighting into the HDR target from the G-buffer:
 *   1. one fullscreen draw for ambient plus every directional light;
 *   2. one rasterized volume per point (sphere) or spot (cone) light,
 *      covering only the pixels the light can reach.
 * Volumes draw both faces with depth test GREATER and depth clamping, so
 * only surfaces inside the volume pass, including when the camera sits
 * inside the volume. Expects the shadow block, light block, and atlases to
 * be bound, and the G-buffer depth already copied into the HDR target.
 */
class GLLightingPass {
   public:
	/**
	 * Loads the pass's shader programs.
	 * @param resources Shared cache through which the programs load.
	 */
	explicit GLLightingPass(ResourceManager& resources);

	/**
	 * @param lights Frame lights, with shadow indices already assigned.
	 * @param frustum Camera frustum for culling volumes.
	 */
	void execute(const std::vector<LightSnapshot>& lights,
				 const Frustum& frustum, GLRenderTargets& targets,
				 GLRenderState& state);

   private:
	void bindGBuffer(const GLRenderTargets& targets);

	std::shared_ptr<Shader> m_directionalShader;
	std::shared_ptr<Shader> m_volumeShader;
	std::shared_ptr<Mesh> m_sphere;
	std::shared_ptr<Mesh> m_cone;
};

/**
 * Model matrix placing the light's volume mesh: a sphere of radius range,
 * or a cone of height range whose base spans the outer angle.
 */
Matrix4 lightVolumeTransform(const LightSnapshot& light);

}  // namespace bird::gl

#endif	// BIRD_GL_LIGHTING_PASS_H
