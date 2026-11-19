#ifndef BIRD_FIXEDSTEPCLOCK_H
#define BIRD_FIXEDSTEPCLOCK_H

#include <algorithm>
#include <stdexcept>

namespace bird {

/**
 * Converts variable frame times into a whole number of fixed simulation
 * steps (the classic accumulator loop).
 *
 * Long frames are clamped (maxFrameTime) and at most maxSteps steps run per
 * frame; time beyond that is dropped, so a slow machine slows the
 * simulation down instead of spiralling into ever longer frames.
 */
class FixedStepClock {
   public:
	/**
	 * @param step Fixed step length in seconds.
	 * @param maxSteps Maximum steps returned by one advance().
	 * @param maxFrameTime Frame deltas are clamped to this many seconds.
	 * @throws std::invalid_argument for a non-positive step, maxSteps or
	 *         maxFrameTime.
	 */
	explicit FixedStepClock(float step = 1.0f / 60.0f, int maxSteps = 8,
							float maxFrameTime = 0.25f) {
		setStep(step);
		if (maxSteps <= 0 || maxFrameTime <= 0.0f) {
			throw std::invalid_argument("FixedStepClock: limits must be positive");
		}
		m_maxSteps = maxSteps;
		m_maxFrameTime = maxFrameTime;
	}

	/**
	 * Adds one frame's elapsed time.
	 * @param frameDelta Seconds since the previous frame (negative counts as 0).
	 * @return Number of fixed steps to simulate now.
	 */
	int advance(double frameDelta) {
		m_accumulator += std::clamp(frameDelta, 0.0, double(m_maxFrameTime));
		// The epsilon keeps a frame of exactly one step from rounding to 0.
		int steps = static_cast<int>((m_accumulator + 1e-7) / m_step);
		if (steps > m_maxSteps) {
			steps = m_maxSteps;
			m_accumulator = 0.0;  // drop the backlog
		} else {
			m_accumulator = std::max(0.0, m_accumulator - steps * double(m_step));
		}
		return steps;
	}

	/** @throws std::invalid_argument for a non-positive step. */
	void setStep(float step) {
		if (!(step > 0.0f)) {
			throw std::invalid_argument("FixedStepClock: step must be positive");
		}
		m_step = step;
	}

	float getStep() const { return m_step; }
	int getMaxSteps() const { return m_maxSteps; }
	/** Leftover time not yet simulated, in seconds. */
	double getAccumulator() const { return m_accumulator; }
	/** Fraction of a step left over, in [0, 1): for render interpolation. */
	float getAlpha() const { return float(m_accumulator / m_step); }

   private:
	float m_step = 1.0f / 60.0f;
	int m_maxSteps = 8;
	float m_maxFrameTime = 0.25f;
	double m_accumulator = 0.0;
};

}  // namespace bird

#endif	// !BIRD_FIXEDSTEPCLOCK_H
