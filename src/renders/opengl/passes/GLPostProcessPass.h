#ifndef BIRD_GL_POST_PROCESS_PASS_H
#define BIRD_GL_POST_PROCESS_PASS_H

#include <memory>

#include "../GLRenderState.h"
#include "../GLRenderTargets.h"

namespace bird {
class ResourceManager;
}

namespace bird::gl {

/**
 * Resolves HDR to the screen: exposure and ACES tone mapping (or a G-buffer
 * debug view) into the LDR target, then FXAA into the default framebuffer.
 */
class GLPostProcessPass {
   public:
	/**
	 * Loads the pass's shader programs.
	 * @param resources Shared cache through which the programs load.
	 */
	explicit GLPostProcessPass(ResourceManager& resources);

	/**
	 * @param screenWidth Default framebuffer width in pixels.
	 * @param screenHeight Default framebuffer height in pixels.
	 */
	void execute(GLRenderTargets& targets, uint32_t screenWidth,
				 uint32_t screenHeight, GLRenderState& state);

   private:
	std::shared_ptr<Shader> m_tonemapShader;
};

}  // namespace bird::gl

#endif	// BIRD_GL_POST_PROCESS_PASS_H
