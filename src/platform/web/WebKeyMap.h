#ifndef BIRD_WEBKEYMAP_H
#define BIRD_WEBKEYMAP_H

#include <string>

namespace bird::platform {

/**
 * Translates a DOM KeyboardEvent.code ("KeyA", "ArrowLeft", ...) to the
 * engine's KEY_* code. The code names the physical key, so bindings such
 * as WASD keep their positions on every keyboard layout. O(1) average.
 * @return KEY_UNKNOWN (-1) for keys without an engine equivalent.
 */
int engineKeyFromDomCode(const std::string& code);

}  // namespace bird::platform

#endif	// BIRD_WEBKEYMAP_H
