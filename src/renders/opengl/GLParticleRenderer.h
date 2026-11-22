#ifndef BIRD_GL_PARTICLE_RENDERER_H
#define BIRD_GL_PARTICLE_RENDERER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../../scene/components/ParticleEmitter.h"
#include "../generic/Shader.h"
#include "GLBuffer.h"
#include "GLRenderState.h"

namespace bird {
class ResourceManager;
}

namespace bird::gl {

/**
 * Batched OpenGL renderer for CPU billboard ParticleEmitter components.
 *
 * Draws inside the forward transparency pass against the scene depth buffer
 * with depth writes disabled and either alpha (`GLBlend::Alpha`) or additive
 * (`GLBlend::AdditiveAlpha`) blending. Maintains a dynamic vertex buffer and
 * precomputed quad index buffer that grow geometrically as needed and are
 * reused across frames without per-frame allocations once capacity stabilizes.
 */
class GLParticleRenderer {
   public:
	/**
	 * Loads the particle shader program and initializes the dynamic VAO/VBO/IBO.
	 * @param resources Shared resource cache.
	 */
	explicit GLParticleRenderer(ResourceManager& resources);
	~GLParticleRenderer();

	GLParticleRenderer(const GLParticleRenderer&) = delete;
	GLParticleRenderer& operator=(const GLParticleRenderer&) = delete;

	/**
	 * Builds camera-facing quads and issues batched draw calls for every
	 * visible particle emitter.
	 *
	 * @param emitters Visible emitters that passed frustum culling this frame.
	 * @param cameraView Active camera view matrix (used to extract camera
	 *        right, up, and forward axes).
	 * @param state Cached GL render state.
	 */
	void draw(const std::vector<ParticleEmitter*>& emitters,
			  const Matrix4& cameraView, GLRenderState& state);

   private:
	void ensureIndexCapacity(size_t quadCount);
	uint32_t ensureFallbackWhiteTexture();

	std::shared_ptr<Shader> m_shader;
	uint32_t m_vao = 0;
	std::unique_ptr<GLBuffer> m_vertices;
	std::unique_ptr<GLBuffer> m_indices;
	size_t m_indexedQuadCapacity = 0;
	uint32_t m_fallbackWhiteTex = 0;
};

}  // namespace bird::gl

#endif	// BIRD_GL_PARTICLE_RENDERER_H
