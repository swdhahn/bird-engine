#ifndef BIRD_ANIMATION_WORLDTWEENS_H
#define BIRD_ANIMATION_WORLDTWEENS_H

#include "../util/Math.h"
#include "AnimationSystem.h"
#include "Easing.h"

namespace bird {

class WorldObject;

/*
 * Transform tweens for WorldObjects. Each starts from the object's current
 * value, runs on the given system, and is registered with the object as
 * owner so destroying the object cancels the tween. The overloads without
 * a system use the engine-wide ANIMATION_SYSTEM.
 */

/**
 * Tweens the local position.
 * @param system System that advances the tween.
 * @param target Object to move; must not be null.
 * @param to Final local position.
 * @param duration Seconds; must be >= 0.
 * @param ease Easing applied to normalized time.
 * @throws std::invalid_argument when target is null or duration negative.
 */
BIRD_NO_EXPORT AnimationHandle tweenLocalPosition(AnimationSystem& system,
												  WorldObject* target,
												  Vector3 to, float duration,
												  Ease ease);
/** Tweens the local rotation along the shortest arc. */
BIRD_NO_EXPORT AnimationHandle tweenLocalRotation(AnimationSystem& system,
												  WorldObject* target,
												  Quaternion to, float duration,
												  Ease ease);
/** Tweens the scale. */
BIRD_NO_EXPORT AnimationHandle tweenScale(AnimationSystem& system,
										  WorldObject* target, Vector3 to,
										  float duration, Ease ease);

/**
 * Tweens the local position on the engine-wide ANIMATION_SYSTEM.
 * @throws std::logic_error when the engine has no animation system.
 */
AnimationHandle tweenLocalPosition(WorldObject* target, Vector3 to,
								   float duration, Ease ease);
/** Tweens the local rotation on ANIMATION_SYSTEM (shortest arc). */
AnimationHandle tweenLocalRotation(WorldObject* target, Quaternion to,
								   float duration, Ease ease);
/** Tweens the scale on ANIMATION_SYSTEM. */
AnimationHandle tweenScale(WorldObject* target, Vector3 to, float duration,
						   Ease ease);

}  // namespace bird

#endif	// BIRD_ANIMATION_WORLDTWEENS_H
