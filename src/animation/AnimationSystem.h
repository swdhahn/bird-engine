#ifndef BIRD_ANIMATION_ANIMATIONSYSTEM_H
#define BIRD_ANIMATION_ANIMATIONSYSTEM_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "../util/Constants.h"
#include "../util/Math.h"
#include "../util/Memory.hpp"
#include "AnimationPlayback.h"
#include "Easing.h"

namespace bird {

/**
 * Generational reference to an animation inside an AnimationSystem.
 *
 * Handles never dangle: once the animation finishes or is cancelled its
 * slot generation advances, and every query on a stale handle reports
 * "not active" instead of touching recycled memory.
 */
struct AnimationHandle {
	uint32_t index = UINT32_MAX;
	uint32_t generation = 0;

	/** True for a default-constructed handle that never named anything. */
	bool isNull() const { return index == UINT32_MAX; }
};

/**
 * Owns and advances running animations.
 *
 * The engine keeps one global instance (ANIMATION_SYSTEM) for world
 * animation, ticked after scripts and before transform matrices update;
 * each UIManager keeps its own instance for UI animation. Animations are
 * stored in a slot map: play and cancel are O(1), tick is O(n) in slots.
 *
 * Re-entrancy: callbacks fired during tick() may play or cancel freely.
 * Animations started during a tick first advance on the next tick, and a
 * cancelled playback is destroyed only after the tick completes.
 */
class AnimationSystem {
   public:
	AnimationSystem() = default;
	~AnimationSystem();
	AnimationSystem(const AnimationSystem&) = delete;
	AnimationSystem& operator=(const AnimationSystem&) = delete;

	/**
	 * Takes ownership of a playback and starts it on the next tick.
	 *
	 * @param playback Animation to run; must not be null.
	 * @param owner Optional identity (usually the animated object) used by
	 *        cancelOwner() so objects can stop their animations on destruction.
	 * @return Handle for later control.
	 * @throws std::invalid_argument when playback is null.
	 */
	BIRD_NO_EXPORT AnimationHandle play(_own<AnimationPlayback> playback,
										const void* owner = nullptr);

	/**
	 * Stops an animation where it stands (no final value, no callback).
	 * @return true when the handle named an active animation.
	 */
	bool cancel(AnimationHandle handle);

	/**
	 * Cancels every animation registered with this owner. O(n).
	 * @return Number of animations cancelled.
	 */
	BIRD_NO_EXPORT size_t cancelOwner(const void* owner);

	/**
	 * Cancels every animation whose (non-null) owner matches a predicate,
	 * in one O(n) pass; used to cancel a whole removed subtree at once.
	 * @return Number of animations cancelled.
	 */
	BIRD_NO_EXPORT size_t cancelOwnersIf(
		const std::function<bool(const void*)>& predicate);

	/** Cancels everything. */
	void clear();

	/** @return true while the handle's animation is still running. */
	bool isActive(AnimationHandle handle) const;
	/** @return the playback, or nullptr for a stale or null handle. */
	BIRD_NO_EXPORT AnimationPlayback* get(AnimationHandle handle);

	/** Pauses the animation; @return false for a stale handle. */
	bool pause(AnimationHandle handle);
	/** Resumes the animation; @return false for a stale handle. */
	bool resume(AnimationHandle handle);
	/** Sets the playback rate; @return false for a stale handle. */
	bool setSpeed(AnimationHandle handle, float speed);

	/**
	 * Advances every active animation by delta seconds and retires the
	 * finished ones.
	 * @throws std::invalid_argument when delta is negative or not finite.
	 */
	void tick(float delta);

	/** Number of animations currently running. */
	size_t getActiveCount() const { return m_activeCount; }

	// ------------------------------------------------ script-friendly tweens
	// Concrete overloads so the Dart binding generator (which cannot see
	// templates) exposes tweening for every common value type.

	/**
	 * Animates a float from one value to another through a setter.
	 * @param from Start value.
	 * @param to End value.
	 * @param duration Seconds; must be >= 0.
	 * @param ease Easing applied to normalized time.
	 * @param setter Receives each interpolated value; must not be empty.
	 * @throws std::invalid_argument on a negative duration or empty setter.
	 */
	AnimationHandle tweenFloat(float from, float to, float duration, Ease ease,
							   std::function<void(float)> setter);
	/** Vector2 variant of tweenFloat. */
	AnimationHandle tweenVector2(Vector2 from, Vector2 to, float duration,
								 Ease ease,
								 std::function<void(Vector2)> setter);
	/** Vector3 variant of tweenFloat. */
	AnimationHandle tweenVector3(Vector3 from, Vector3 to, float duration,
								 Ease ease,
								 std::function<void(Vector3)> setter);
	/** Vector4 (colour) variant of tweenFloat. */
	AnimationHandle tweenVector4(Vector4 from, Vector4 to, float duration,
								 Ease ease,
								 std::function<void(Vector4)> setter);
	/** Quaternion variant of tweenFloat (shortest-arc slerp). */
	AnimationHandle tweenQuaternion(Quaternion from, Quaternion to,
									float duration, Ease ease,
									std::function<void(Quaternion)> setter);

   private:
	struct Slot {
		_own<AnimationPlayback> playback;
		const void* owner = nullptr;
		uint32_t generation = 0;
	};

	Slot* resolve(AnimationHandle handle);
	const Slot* resolve(AnimationHandle handle) const;
	/** Retires a slot; destruction is deferred while a tick is running. */
	void release(uint32_t index);

	std::vector<Slot> m_slots;
	std::vector<uint32_t> m_freeSlots;
	/** Slots freed mid-tick; recycled once the tick completes. */
	std::vector<uint32_t> m_pendingFree;
	/** Playbacks retired mid-tick; destroyed once the tick completes. */
	std::vector<_own<AnimationPlayback>> m_graveyard;
	size_t m_activeCount = 0;
	bool m_ticking = false;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_ANIMATIONSYSTEM_H
