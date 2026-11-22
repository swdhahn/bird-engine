#ifndef BIRD_UIVISUALTRANSFORM_H
#define BIRD_UIVISUALTRANSFORM_H

#include <cstdint>

#include "../../util/Math.h"

namespace bird {

/**
 * Post-layout presentation of a UI node (CSS `transform` and `opacity`).
 *
 * Layout never reads these values, so animating them only recomposes
 * transforms and rebuilds the draw list; it never re-measures the tree.
 * Hit testing does follow the visual transform.
 */
struct UIVisualTransform {
	/** Pixel translation added after layout. */
	Vector2 offset{0.0f};
	/** Scale applied around the origin. */
	Vector2 scale{1.0f};
	/** Rotation and scale origin as a fraction of the node size. */
	Vector2 origin{0.5f};
	/** Alpha multiplier for this node and its subtree, in [0, 1]. */
	float opacity = 1.0f;
};

/** Visual properties a UIAnimator can own; see UIObject::claimAnimated. */
enum class UIAnimatedProperty : uint8_t {
	Offset = 0,
	Scale,
	Rotation,
	Opacity,
	Count
};

}  // namespace bird

#endif	// BIRD_UIVISUALTRANSFORM_H
