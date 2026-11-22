#ifndef BIRD_ANIMATION_PLAYBACKCLOCK_H
#define BIRD_ANIMATION_PLAYBACKCLOCK_H

#include <cstdint>

namespace bird {

/** What a PlaybackClock does on reaching the end of its duration. */
enum class LoopMode : uint8_t {
	/** Stops at the end and reports finished. */
	Once = 0,
	/** Wraps back to the start indefinitely. */
	Loop = 1,
	/** Alternates forward and backward indefinitely. */
	PingPong = 2
};

/** What happened during one PlaybackClock::advance call. */
struct PlaybackEvents {
	/** A LoopMode::Once clock reached its end during this advance. */
	bool finished = false;
	/** A looping clock completed at least one cycle during this advance. */
	bool looped = false;
};

/**
 * Converts accumulated frame deltas into a local animation time.
 *
 * Owns speed, start delay, looping and pausing, so curves and clips stay
 * pure functions of time. The clock reports events instead of invoking
 * callbacks, letting its owner apply the final value before notifying. A zero duration is valid
 * and finishes on the first advance (an instantaneous "set").
 */
class PlaybackClock {
   public:
	/**
	 * @param duration Length of one cycle in seconds; must be >= 0.
	 * @throws std::invalid_argument when duration is negative or not finite.
	 */
	explicit PlaybackClock(float duration = 0.0f);

	/** @throws std::invalid_argument when negative or not finite. */
	void setDuration(float duration);
	float getDuration() const { return m_duration; }

	/**
	 * Playback rate multiplier; negative values play backwards.
	 * @throws std::invalid_argument when not finite.
	 */
	void setSpeed(float speed);
	float getSpeed() const { return m_speed; }

	/**
	 * Seconds to wait before time starts moving.
	 * @throws std::invalid_argument when negative or not finite.
	 */
	void setDelay(float delay);
	float getDelay() const { return m_delay; }

	void setLoopMode(LoopMode mode) { m_loopMode = mode; }
	LoopMode getLoopMode() const { return m_loopMode; }

	/**
	 * Moves time forward by delta * speed (after any remaining delay).
	 * Does nothing while paused or finished.
	 * @return The events this step produced.
	 * @throws std::invalid_argument when delta is negative or not finite.
	 */
	PlaybackEvents advance(float delta);

	/** Jumps to a cycle-local time, clearing the finished state. */
	void seek(float time);
	/** Rewinds to the start, restoring the full delay. */
	void restart();
	void pause() { m_paused = true; }
	void resume() { m_paused = false; }
	bool isPaused() const { return m_paused; }
	bool isFinished() const { return m_finished; }
	/** True while the start delay has not yet elapsed. */
	bool isDelayed() const { return m_delayRemaining > 0.0f; }

	/** Current time inside one cycle, in [0, duration]. */
	float getTime() const;
	/** getTime() / duration, or 1 for a zero-duration clock. */
	float getNormalizedTime() const;
	/** Completed cycles (loops) since the last restart. */
	uint32_t getCompletedCycles() const { return m_cycles; }

   private:
	float m_duration = 0.0f;
	float m_speed = 1.0f;
	float m_delay = 0.0f;
	float m_delayRemaining = 0.0f;
	/** Position inside the current cycle, always in [0, duration]. */
	float m_position = 0.0f;
	/** True while a ping-pong clock runs backward. */
	bool m_reversed = false;
	bool m_paused = false;
	bool m_finished = false;
	uint32_t m_cycles = 0;
	LoopMode m_loopMode = LoopMode::Once;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_PLAYBACKCLOCK_H
