#ifndef BIRD_PHYSICSLAYERS_H
#define BIRD_PHYSICSLAYERS_H

#include <cstdint>

namespace bird {

// Object Layers: "Who am I?"
namespace ObjectLayers {
static constexpr uint16_t NON_MOVING = 0;
static constexpr uint16_t MOVING = 1;
static constexpr uint16_t NUM_LAYERS = 2;
};	// namespace ObjectLayers

// BroadPhase Layers: "Which bucket do I go in?"
// (Used for high-level optimization)
namespace BroadPhaseLayers {
static constexpr uint8_t NON_MOVING = 0;
static constexpr uint8_t MOVING = 1;
static constexpr uint8_t NUM_LAYERS = 2;
};	// namespace BroadPhaseLayers

}  // namespace bird

#endif	// !BIRD_PHYSICSLAYERS_H
