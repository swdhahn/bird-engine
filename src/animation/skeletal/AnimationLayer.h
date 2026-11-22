#ifndef BIRD_ANIMATION_ANIMATIONLAYER_H
#define BIRD_ANIMATION_ANIMATIONLAYER_H

#include <cstdint>
#include <memory>
#include <string>

#include "../../util/Constants.h"
#include "BoneMask.h"
#include "PoseNode.h"
#include "Skeleton.h"

namespace bird {

/**
 * How an AnimationLayer combines its evaluated pose with the accumulated pose
 * from lower layers.
 */
enum class LayerBlendMode : uint8_t {
	/**
	 * Replaces the underlying pose per joint according to
	 * `mask[j] * layer.weight` (LERP for T/S, shortest-path SLERP for R).
	 * Ideal for upper-body aiming or reloading over lower-body running.
	 */
	Override = 0,
	/**
	 * Computes a delta pose relative to referencePose (or the skeleton's rest
	 * pose when referencePose is empty) and applies it additively on top of
	 * the underlying pose, scaled by `mask[j] * layer.weight`.
	 * Ideal for breathing, recoil kicks, aim offsets, or hit flinches.
	 */
	Additive = 1
};

/**
 * One overlay layer in a SkeletalAnimator's pose stack.
 *
 * Layers are evaluated in order after the base pose. Each layer owns a
 * PoseNode source (a single clip, blend tree, or independent state machine),
 * an optional BoneMask, a blend mode (Override or Additive), and a global
 * weight in [0.0, 1.0].
 */
struct BIRD_NO_EXPORT AnimationLayer {
	/** Human-readable layer name (e.g. "UpperBody", "RecoilAdditive"). */
	std::string name;
	/** PoseNode producing this layer's pose each frame. */
	std::shared_ptr<PoseNode> source;
	/** Per-joint weights in [0, 1]; empty means all joints have weight 1.0. */
	BoneMask mask;
	/** Override or Additive blending mode. */
	LayerBlendMode blendMode = LayerBlendMode::Override;
	/** Global layer weight in [0, 1]. */
	float weight = 1.0f;
	/**
	 * Optional reference pose used when blendMode == LayerBlendMode::Additive.
	 * When empty, the skeleton's rest pose is used as the reference.
	 */
	Pose referencePose;
	/** Quick toggle to disable evaluation without removing the layer. */
	bool enabled = true;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_ANIMATIONLAYER_H
