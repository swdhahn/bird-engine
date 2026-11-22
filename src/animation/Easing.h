#ifndef BIRD_ANIMATION_EASING_H
#define BIRD_ANIMATION_EASING_H

#include <cstdint>

namespace bird {

/**
 * Standard easing functions (Penner set). Each maps normalized time in
 * [0, 1] to progress; Back and Elastic deliberately overshoot [0, 1].
 */
enum class Ease : uint8_t {
	Linear = 0,
	InQuad,
	OutQuad,
	InOutQuad,
	InCubic,
	OutCubic,
	InOutCubic,
	InQuart,
	OutQuart,
	InOutQuart,
	InSine,
	OutSine,
	InOutSine,
	InExpo,
	OutExpo,
	InOutExpo,
	InBack,
	OutBack,
	InOutBack,
	InElastic,
	OutElastic,
	InOutElastic,
	InBounce,
	OutBounce,
	InOutBounce
};

/**
 * Evaluates an easing function.
 *
 * @param ease Easing function to apply.
 * @param t Normalized time; values outside [0, 1] are clamped.
 * @return Eased progress (may exceed [0, 1] for Back and Elastic).
 */
float evaluateEase(Ease ease, float t);

/**
 * CSS-style cubic Bezier timing function with endpoints (0,0) and (1,1).
 *
 * Matches `cubic-bezier(x1, y1, x2, y2)`, so designer-specified UI curves
 * transfer directly.
 */
class CubicBezierEase {
   public:
	/**
	 * @param x1 First control point x; must lie in [0, 1].
	 * @param y1 First control point y (may overshoot).
	 * @param x2 Second control point x; must lie in [0, 1].
	 * @param y2 Second control point y (may overshoot).
	 * @throws std::invalid_argument when an x coordinate leaves [0, 1].
	 */
	CubicBezierEase(float x1, float y1, float x2, float y2);

	/**
	 * Solves the curve for x = t (Newton-Raphson with bisection fallback).
	 * @param t Normalized time; values outside [0, 1] are clamped.
	 * @return Eased progress.
	 */
	float evaluate(float t) const;

   private:
	float m_x1, m_y1, m_x2, m_y2;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_EASING_H
