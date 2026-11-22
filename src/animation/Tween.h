#ifndef BIRD_ANIMATION_TWEEN_H
#define BIRD_ANIMATION_TWEEN_H

#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

#include "AnimationPlayback.h"
#include "Curve.h"
#include "Easing.h"
#include "Interpolation.h"

namespace bird {

/**
 * Eased interpolation between two values, written through a setter.
 * Conceptually a two-key curve; kept separate because it needs no
 * allocation for keys and is the overwhelmingly common UI case.
 *
 * @tparam T Value type with an Interpolator specialisation.
 */
template <typename T>
class Tween final : public AnimationPlayback {
   public:
	using Setter = std::function<void(const T&)>;

	/**
	 * @throws std::invalid_argument on a negative duration or empty setter.
	 */
	Tween(T from, T to, float duration, Ease ease, Setter setter)
		: AnimationPlayback(duration),
		  m_from(std::move(from)),
		  m_to(std::move(to)),
		  m_ease(ease),
		  m_setter(std::move(setter)) {
		if (!m_setter) {
			throw std::invalid_argument("Tween: setter must not be empty");
		}
	}

	/** Uses a CSS-style cubic Bezier in place of a named Ease. */
	void setBezier(const CubicBezierEase& bezier) {
		m_bezier = std::make_unique<CubicBezierEase>(bezier);
	}

   protected:
	void apply(float time) override {
		const float duration = clock().getDuration();
		const float t = duration > 0.0f ? time / duration : 1.0f;
		const float eased =
			m_bezier ? m_bezier->evaluate(t) : evaluateEase(m_ease, t);
		m_setter(Interpolator<T>::linear(m_from, m_to, eased));
	}

   private:
	T m_from;
	T m_to;
	Ease m_ease;
	std::unique_ptr<CubicBezierEase> m_bezier;
	Setter m_setter;
};

/**
 * Plays a shared Curve through a setter. The curve is shared (not copied)
 * so one authored curve can drive many objects at once.
 *
 * @tparam T Value type with an Interpolator specialisation.
 */
template <typename T>
class CurvePlayback final : public AnimationPlayback {
   public:
	using Setter = std::function<void(const T&)>;

	/**
	 * Duration is the curve's end time; playback starts at time zero.
	 * @throws std::invalid_argument when the curve is null or empty, or the
	 *         setter is empty.
	 */
	CurvePlayback(std::shared_ptr<const Curve<T>> curve, Setter setter)
		: AnimationPlayback(validated(curve)->getEndTime()),
		  m_curve(std::move(curve)),
		  m_setter(std::move(setter)) {
		if (!m_setter) {
			throw std::invalid_argument(
				"CurvePlayback: setter must not be empty");
		}
	}

   protected:
	void apply(float time) override {
		m_setter(m_curve->sample(time, m_cursor));
	}

   private:
	static const std::shared_ptr<const Curve<T>>& validated(
		const std::shared_ptr<const Curve<T>>& curve) {
		if (!curve || curve->isEmpty()) {
			throw std::invalid_argument(
				"CurvePlayback: curve must exist and have keys");
		}
		if (curve->getEndTime() < 0.0f) {
			throw std::invalid_argument(
				"CurvePlayback: curve must end at a time >= 0");
		}
		return curve;
	}

	std::shared_ptr<const Curve<T>> m_curve;
	Setter m_setter;
	size_t m_cursor = 0;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_TWEEN_H
