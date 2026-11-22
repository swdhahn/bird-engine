#include "FrustumCuller.h"

namespace bird {

void CullingStats::reset() {
	totalTested = 0;
	rendered = 0;
	culled = 0;
	deferredRendered = 0;
	deferredCulled = 0;
	forwardOpaqueRendered = 0;
	forwardOpaqueCulled = 0;
	transparentRendered = 0;
	transparentCulled = 0;
	emittersTested = 0;
	emittersRendered = 0;
	emittersCulled = 0;
}

float CullingStats::cullRatio() const {
	if (totalTested == 0) return 0.0f;
	return static_cast<float>(culled) / static_cast<float>(totalTested);
}

void FrustumCuller::beginFrame(const Matrix4& viewProjection) {
	beginFrame(Frustum::fromViewProjection(viewProjection));
}

void FrustumCuller::beginFrame(const Frustum& frustum) {
	m_frustum = frustum;
	m_stats.reset();
}

bool FrustumCuller::testMesh(const AABB& worldBounds, RenderBucket bucket) {
	const bool visible = !m_enabled || m_frustum.intersects(worldBounds);
	recordMeshResult(visible, bucket);
	return visible;
}

bool FrustumCuller::testMesh(const BoundingSphere& worldSphere,
							 RenderBucket bucket) {
	const bool visible = !m_enabled || m_frustum.intersects(worldSphere);
	recordMeshResult(visible, bucket);
	return visible;
}

bool FrustumCuller::testParticleEmitter(const AABB& worldBounds) {
	m_stats.emittersTested++;
	const bool visible = !m_enabled || m_frustum.intersects(worldBounds);
	if (visible) {
		m_stats.emittersRendered++;
	} else {
		m_stats.emittersCulled++;
	}
	return visible;
}

void FrustumCuller::recordMeshResult(bool visible, RenderBucket bucket) {
	m_stats.totalTested++;
	if (visible) {
		m_stats.rendered++;
	} else {
		m_stats.culled++;
	}
	switch (bucket) {
		case RenderBucket::Deferred:
			if (visible) {
				m_stats.deferredRendered++;
			} else {
				m_stats.deferredCulled++;
			}
			break;
		case RenderBucket::ForwardOpaque:
			if (visible) {
				m_stats.forwardOpaqueRendered++;
			} else {
				m_stats.forwardOpaqueCulled++;
			}
			break;
		case RenderBucket::Transparent:
			if (visible) {
				m_stats.transparentRendered++;
			} else {
				m_stats.transparentCulled++;
			}
			break;
		default:
			break;
	}
}

}  // namespace bird
