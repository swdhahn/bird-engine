#ifndef BIRD_PHYSICSLAYERS_H
#define BIRD_PHYSICSLAYERS_H

#include <cstdint>
#include <stdexcept>

namespace bird {

/**
 * Collision layer bit set. A body lives on its `layers` and collides with
 * bodies whose layers intersect its `mask` (both ways must agree, as in
 * Godot): bodies A and B touch when (A.layers & B.mask) && (B.layers & A.mask).
 *
 * Seven layers are available (bits 0-6); they fit Jolt's 16-bit object layer
 * together with the static/moving flag used for broad-phase bucketing.
 */
using PhysicsLayerMask = uint8_t;

/** Number of user collision layers. */
constexpr uint32_t kPhysicsLayerCount = 7;

/** Every layer set: the default collision mask and query mask. */
constexpr PhysicsLayerMask kAllPhysicsLayers = 0x7F;

/**
 * Bit for one layer index.
 * @param index Layer index in [0, kPhysicsLayerCount).
 * @throws std::out_of_range for an index outside that range.
 */
inline PhysicsLayerMask physicsLayer(uint32_t index) {
	if (index >= kPhysicsLayerCount) {
		throw std::out_of_range("physicsLayer: index must be below 7");
	}
	return static_cast<PhysicsLayerMask>(1u << index);
}

}  // namespace bird

#endif	// !BIRD_PHYSICSLAYERS_H
