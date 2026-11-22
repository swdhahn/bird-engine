#ifndef BIRD_FRUSTUM_CULLER_H
#define BIRD_FRUSTUM_CULLER_H

#include <cstdint>

#include "../../util/Math.h"
#include "Bounds.h"
#include "RenderQueue.h"

namespace bird {

/**
 * Per-frame frustum culling counters broken down by render pass / bucket.
 *
 * Exposed by GraphicsPipeline::getCullingStats() for debugging, overlays, and
 * performance profiling. All counters are reset at the start of each frame by
 * FrustumCuller::beginFrame().
 */
struct CullingStats {
	/** Total mesh sub-draws evaluated against the frustum. */
	uint32_t totalTested = 0;
	/** Total mesh sub-draws that passed culling and were submitted. */
	uint32_t rendered = 0;
	/** Total mesh sub-draws rejected by the frustum. */
	uint32_t culled = 0;

	/** Deferred G-buffer draws submitted. */
	uint32_t deferredRendered = 0;
	/** Deferred G-buffer draws culled. */
	uint32_t deferredCulled = 0;

	/** Forward opaque draws submitted. */
	uint32_t forwardOpaqueRendered = 0;
	/** Forward opaque draws culled. */
	uint32_t forwardOpaqueCulled = 0;

	/** Forward transparent / additive draws submitted. */
	uint32_t transparentRendered = 0;
	/** Forward transparent / additive draws culled. */
	uint32_t transparentCulled = 0;

	/** Particle emitters evaluated against the frustum. */
	uint32_t emittersTested = 0;
	/** Particle emitters that passed culling and were submitted. */
	uint32_t emittersRendered = 0;
	/** Particle emitters rejected by the frustum. */
	uint32_t emittersCulled = 0;

	/** Resets every counter to zero. */
	void reset();

	/**
	 * Total forward-pass mesh draws submitted (ForwardOpaque + Transparent).
	 * @return Combined forward rendered mesh count.
	 */
	uint32_t forwardRendered() const {
		return forwardOpaqueRendered + transparentRendered;
	}

	/**
	 * Total forward-pass mesh draws culled (ForwardOpaque + Transparent).
	 * @return Combined forward culled mesh count.
	 */
	uint32_t forwardCulled() const {
		return forwardOpaqueCulled + transparentCulled;
	}

	/**
	 * Fraction of tested mesh draws that were culled, in [0.0, 1.0].
	 * @return 0.0f when totalTested is 0.
	 */
	float cullRatio() const;
};

/**
 * Camera frustum culler with per-pass visibility statistics.
 *
 * Extracts the six frustum planes from the camera's view-projection matrix at
 * the start of a frame and tests world-space AABBs or bounding spheres before
 * draw submission for both deferred and forward passes (as well as particle
 * emitters). Can be toggled off via setEnabled(false) for profiling
 * comparisons without changing call sites.
 */
class FrustumCuller {
   public:
	/**
	 * Begins a new frame by extracting the six frustum planes from the
	 * combined projection * view matrix and resetting per-frame statistics.
	 * @param viewProjection Camera projection * view matrix.
	 */
	void beginFrame(const Matrix4& viewProjection);

	/**
	 * Begins a new frame using a pre-extracted frustum and resets per-frame
	 * statistics.
	 * @param frustum Pre-extracted camera frustum.
	 */
	void beginFrame(const Frustum& frustum);

	/**
	 * Tests a world-space mesh AABB against the active frustum and records
	 * rendered vs. culled counts for the item's destination bucket.
	 *
	 * When culling is disabled via setEnabled(false), always returns true and
	 * counts the item as rendered.
	 *
	 * @param worldBounds Mesh AABB transformed into world space.
	 * @param bucket Destination pass bucket (Deferred, ForwardOpaque, or
	 *        Transparent).
	 * @return True if the mesh should be submitted for rendering.
	 */
	bool testMesh(const AABB& worldBounds, RenderBucket bucket);

	/**
	 * Tests a world-space mesh bounding sphere against the active frustum and
	 * records rendered vs. culled counts for the item's destination bucket.
	 *
	 * @param worldSphere Mesh bounding sphere transformed into world space.
	 * @param bucket Destination pass bucket.
	 * @return True if the mesh should be submitted for rendering.
	 */
	bool testMesh(const BoundingSphere& worldSphere, RenderBucket bucket);

	/**
	 * Tests a particle emitter's world-space AABB against the active frustum
	 * and records emitter visibility statistics.
	 *
	 * @param worldBounds Enclosing world-space AABB of the emitter's active
	 *        particles.
	 * @return True if the emitter should be rendered.
	 */
	bool testParticleEmitter(const AABB& worldBounds);

	/**
	 * Enables or disables frustum rejection. When disabled, all test methods
	 * return true while still tracking rendered counts.
	 * @param enabled True to cull off-screen renderables (default).
	 */
	void setEnabled(bool enabled) { m_enabled = enabled; }

	/** @return True when frustum culling is active. */
	bool isEnabled() const { return m_enabled; }

	/** @return The active frustum planes for the current frame. */
	const Frustum& getFrustum() const { return m_frustum; }

	/** @return Per-frame culling statistics accumulated since beginFrame(). */
	const CullingStats& getStats() const { return m_stats; }

   private:
	void recordMeshResult(bool visible, RenderBucket bucket);

	Frustum m_frustum;
	CullingStats m_stats;
	bool m_enabled = true;
};

}  // namespace bird

#endif	// BIRD_FRUSTUM_CULLER_H
