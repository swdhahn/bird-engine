#ifndef BIRD_PHYSICSLAYERS_H
#define BIRD_PHYSICSLAYERS_H

#include <cstdint>

namespace bird {

/**
 * Collision filtering layers and category bitmasks for physical simulation queries.
 */
enum PhysicsLayer : uint16_t {
	LAYER_NON_MOVING = 0,
	LAYER_MOVING = 1,
	LAYER_NUM_LAYERS = 2
};

}  // namespace bird

#endif	// !BIRD_PHYSICSLAYERS_H
