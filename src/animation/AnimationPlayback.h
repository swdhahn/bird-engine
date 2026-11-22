#ifndef BIRD_ANIMATION_ANIMATIONPLAYBACK_H
#define BIRD_ANIMATION_ANIMATIONPLAYBACK_H

#include <functional>

#include "PlaybackClock.h"

namespace bird {

/**
 * One running animation: a PlaybackClock plus a way to apply a time.
 *
 * Subclasses implement apply(time) and nothing else; timing, looping and
 * notification live here so every kind of animation (tweens, curves,
 * skeletal clips) shares identical playback semantics.
 */
class AnimationPlayback {
   public:
	/**
	 * @param duration Cycle length in seconds; must be >= 0.
	 * @throws std::invalid_argument when duration is negative or not finite.
	 */
	explicit AnimationPlayback(float duration);
	virtual ~AnimationPlayback() = default;

	AnimationPlayback(const AnimationPlayback&) = delete;
	AnimationPlayback& operator=(const AnimationPlayback&) = delete;

	PlaybackClock& clock() { return m_clock; }
	const PlaybackClock& clock() const { return m_clock; }

	/**
	 * Called once after the final value is applied (LoopMode::Once only).
	 * The callback may start or cancel other animations.
	 */
	void setOnFinished(std::function<void()> callback);
	/** Called after each completed cycle of a looping animation. */
	void setOnLoop(std::function<void()> callback);

	/**
	 * Advances the clock, applies the resulting time, then notifies.
	 * While a start delay is pending nothing is applied.
	 * @return false once the animation has finished.
	 */
	bool step(float delta);

	/**
	 * Applies the current clock time without advancing (for example to
	 * show the first frame immediately after creation).
	 */
	void applyCurrent() { apply(m_clock.getTime()); }

   protected:
	/** Writes the animated value(s) for a cycle-local time in seconds. */
	virtual void apply(float time) = 0;

   private:
	PlaybackClock m_clock;
	std::function<void()> m_onFinished;
	std::function<void()> m_onLoop;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_ANIMATIONPLAYBACK_H
