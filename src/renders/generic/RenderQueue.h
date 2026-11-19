#ifndef BIRD_RENDER_QUEUE_H
#define BIRD_RENDER_QUEUE_H

#include <array>
#include <cstddef>
#include <vector>

#include "../../util/Math.h"
#include "Bounds.h"

namespace bird {

class Mesh;
class Material;

/** Destination pass for a draw. */
enum class RenderBucket : uint8_t {
	/** Lit opaque or masked: G-buffer pass. */
	Deferred = 0,
	/** Unlit or force-forward opaque: forward pass, before transparency. */
	ForwardOpaque = 1,
	/** Alpha or additive blending: forward pass, sorted back to front. */
	Transparent = 2,
	Count = 3
};

/**
 * Routes a material to the pass able to shade the material.
 * @param material Material to classify.
 * @return The bucket the draw belongs to.
 */
RenderBucket classifyMaterial(const Material& material);

/** One mesh draw with everything a pass requires. */
struct RenderItem {
	Mesh* mesh = nullptr;
	Material* material = nullptr;
	Matrix4 model{1.0f};
	AABB worldBounds;
	/** Distance from the camera along the view direction. */
	float viewDepth = 0.0f;
};

/**
 * Per-frame draw list split into buckets and sorted for each pass.
 *
 * Opaque buckets sort by material (minimizing uniform and texture rebinds),
 * then front to back (maximizing early depth rejection). The transparent
 * bucket sorts strictly back to front for correct blending. Sorting is
 * O(n log n) per bucket; submission is amortized O(1).
 */
class RenderQueue {
   public:
	/** Empties every bucket while retaining capacity. */
	void clear();

	/**
	 * Adds a draw to the bucket chosen by classifyMaterial().
	 * @throws std::invalid_argument when mesh or material is null.
	 */
	void submit(const RenderItem& item);

	/** Sorts every bucket for its pass. */
	void sort();

	const std::vector<RenderItem>& bucket(RenderBucket bucket) const;

	/** Every submitted item, regardless of bucket (shadow casters). */
	size_t size() const;

   private:
	std::array<std::vector<RenderItem>, static_cast<size_t>(RenderBucket::Count)>
		m_buckets;
};

}  // namespace bird

#endif	// BIRD_RENDER_QUEUE_H
