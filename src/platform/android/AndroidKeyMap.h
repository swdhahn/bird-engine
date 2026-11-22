#ifndef BIRD_ANDROIDKEYMAP_H
#define BIRD_ANDROIDKEYMAP_H

#include <cstdint>

namespace bird::platform {

/**
 * Translates an Android AKEYCODE_* value to the engine's KEY_* code, so
 * action bindings work with hardware keyboards. O(1).
 * @return KEY_UNKNOWN (-1) for keys without an engine equivalent.
 */
int engineKeyFromAndroid(int32_t androidKeyCode);

/** Translates AMETA_* state to the engine modifier bits (shift 1, ...). */
uint8_t engineModifiersFromAndroid(int32_t metaState);

}  // namespace bird::platform

#endif	// BIRD_ANDROIDKEYMAP_H
