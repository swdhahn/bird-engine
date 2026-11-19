#ifndef BIRD_GL_SHADOW_PASS_H
#define BIRD_GL_SHADOW_PASS_H

#include <memory>
#include <vector>

#include "../../generic/Buffer.h"
#include "../../generic/RenderQueue.h"
#include "../../generic/ShadowPlanner.h"
#include "../GLFrameBuffer.h"
#include "../GLRenderState.h"
#include "../GLShader.h"

namespace bird::gl {

/**
 * Renders shadow-caster depth into the directional and local atlases, and
 * publishes the shadow uniform block (binding 3) and atlas samplers
 * (units 8 and 9) for the lighting and forward passes.
 */
class GLShadowPass {
   public:
	/** @param settings Atlas sizes; fixed for the pass's lifetime. */
	explicit GLShadowPass(const ShadowSettings& settings);

	/**
	 * Uploads the plan's GPU block and renders every planned view.
	 * Leaves the default framebuffer bound and scissoring disabled.
	 * @param plan Views and matrices for this frame.
	 * @param casters Every shadow-casting draw, culled here per view.
	 */
	void execute(const ShadowPlan& plan, const std::vector<RenderItem>& casters,
				 GLRenderState& state);

	/** Binds the shadow block and both atlases for sampling. */
	void bindForSampling();

	const ShadowSettings& settings() const { return m_settings; }

   private:
	ShadowSettings m_settings;
	std::shared_ptr<Texture> m_directionalDepth;
	std::shared_ptr<Texture> m_localDepth;
	std::unique_ptr<GLFrameBuffer> m_directionalTarget;
	std::unique_ptr<GLFrameBuffer> m_localTarget;
	std::shared_ptr<Shader> m_depthShader;
	std::unique_ptr<Buffer<ShadowUBOData>> m_shadowBlock;
};

}  // namespace bird::gl

#endif	// BIRD_GL_SHADOW_PASS_H
