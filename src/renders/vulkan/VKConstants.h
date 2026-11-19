//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_VKCONSTANTS_H
#define BIRD_VKCONSTANTS_H

#include "../GraphicsConstants.h"

namespace bird::vk {

#ifdef NDEBUG
inline const bool enableValidationLayers = false;
#else
inline const bool enableValidationLayers = true;
#endif

/** Maximum concurrent frames being rendered on the GPU simultaneously. */
inline const int MAX_FRAMES_IN_FLIGHT = 2;

}  // namespace bird::vk

#endif	// BIRD_VKCONSTANTS_H
