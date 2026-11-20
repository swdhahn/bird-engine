#include "RenderQueue.h"

#include <algorithm>
#include <stdexcept>

#include "Material.h"

namespace bird {

RenderBucket classifyMaterial(const Material& material) {
	const BlendMode blend = material.getBlendMode();
	if (blend == BlendMode::Transparent || blend == BlendMode::Additive) {
		return RenderBucket::Transparent;
	}
	if (material.getDomain() == MaterialDomain::Unlit ||
		material.isForceForward()) {
		return RenderBucket::ForwardOpaque;
	}
	return RenderBucket::Deferred;
}

void RenderQueue::clear() {
	for (auto& items : m_buckets) items.clear();
}

void RenderQueue::submit(const RenderItem& item) {
	if (item.mesh == nullptr || item.material == nullptr) {
		throw std::invalid_argument(
			"RenderQueue::submit: mesh and material are required.");
	}
	m_buckets[static_cast<size_t>(classifyMaterial(*item.material))]
		.push_back(item);
}

void RenderQueue::sort() {
	auto opaqueOrder = [](const RenderItem& a, const RenderItem& b) {
		if (a.isSkinned() != b.isSkinned()) return b.isSkinned();
		if (a.material != b.material) return a.material < b.material;
		return a.viewDepth < b.viewDepth;
	};
	auto backToFront = [](const RenderItem& a, const RenderItem& b) {
		return a.viewDepth > b.viewDepth;
	};
	auto& deferred = m_buckets[static_cast<size_t>(RenderBucket::Deferred)];
	auto& forward = m_buckets[static_cast<size_t>(RenderBucket::ForwardOpaque)];
	auto& transparent =
		m_buckets[static_cast<size_t>(RenderBucket::Transparent)];
	std::sort(deferred.begin(), deferred.end(), opaqueOrder);
	std::sort(forward.begin(), forward.end(), opaqueOrder);
	// Stable: coplanar transparent draws keep submission order, avoiding
	// frame-to-frame flicker.
	std::stable_sort(transparent.begin(), transparent.end(), backToFront);
}

const std::vector<RenderItem>& RenderQueue::bucket(RenderBucket bucket) const {
	const auto index = static_cast<size_t>(bucket);
	if (index >= m_buckets.size()) {
		throw std::out_of_range("RenderQueue::bucket: invalid bucket.");
	}
	return m_buckets[index];
}

size_t RenderQueue::size() const {
	size_t total = 0;
	for (const auto& items : m_buckets) total += items.size();
	return total;
}

}  // namespace bird
