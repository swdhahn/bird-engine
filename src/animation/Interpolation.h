#ifndef BIRD_ANIMATION_INTERPOLATION_H
#define BIRD_ANIMATION_INTERPOLATION_H

#include "../util/Math.h"

namespace bird {

/**
 * Interpolation rules for one animatable value type.
 *
 * The primary template covers every type with vector-space arithmetic
 * (float, Vector2, Vector3, Vector4, colours stored as Vector4). Quaternion
 * is specialised below. Factors are deliberately NOT clamped: easing curves
 * such as OutBack overshoot past 1 and must reach the interpolator intact.
 *
 * @tparam T Animatable value type.
 */
template <typename T>
struct Interpolator {
	/** Linear blend; t = 0 yields from, t = 1 yields to. */
	static T linear(const T& from, const T& to, float t) {
		return from + (to - from) * t;
	}

	/**
	 * Cubic Hermite spline segment (glTF CUBICSPLINE convention).
	 *
	 * @param p0 Value at the segment start.
	 * @param m0 Outgoing tangent at the start, in units per second.
	 * @param p1 Value at the segment end.
	 * @param m1 Incoming tangent at the end, in units per second.
	 * @param t Normalized segment position in [0, 1].
	 * @param segmentDuration Segment length in seconds (scales tangents).
	 */
	static T hermite(const T& p0, const T& m0, const T& p1, const T& m1,
					 float t, float segmentDuration) {
		const float t2 = t * t;
		const float t3 = t2 * t;
		const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
		const float h10 = t3 - 2.0f * t2 + t;
		const float h01 = -2.0f * t3 + 3.0f * t2;
		const float h11 = t3 - t2;
		return p0 * h00 + m0 * (h10 * segmentDuration) + p1 * h01 +
			   m1 * (h11 * segmentDuration);
	}

	/** Additive identity, used for zero tangents. */
	static T zero() { return T(0.0f); }
};

/**
 * Quaternion rules: shortest-path spherical interpolation, and a
 * normalized component-wise Hermite spline (as the glTF specification
 * prescribes for rotation CUBICSPLINE samplers).
 */
template <>
struct Interpolator<Quaternion> {
	static Quaternion linear(const Quaternion& from, const Quaternion& to,
							 float t) {
		// Flip into the same hemisphere so the blend takes the short arc.
		const Quaternion target = glm::dot(from, to) < 0.0f ? -to : to;
		return glm::normalize(glm::slerp(from, target, t));
	}

	static Quaternion hermite(const Quaternion& p0, const Quaternion& m0,
							  const Quaternion& p1, const Quaternion& m1,
							  float t, float segmentDuration) {
		const Quaternion end = glm::dot(p0, p1) < 0.0f ? -p1 : p1;
		const float t2 = t * t;
		const float t3 = t2 * t;
		const Quaternion blended =
			p0 * (2.0f * t3 - 3.0f * t2 + 1.0f) +
			m0 * ((t3 - 2.0f * t2 + t) * segmentDuration) +
			end * (-2.0f * t3 + 3.0f * t2) +
			m1 * ((t3 - t2) * segmentDuration);
		return glm::normalize(blended);
	}

	static Quaternion zero() { return Quaternion(0.0f, 0.0f, 0.0f, 0.0f); }
};

/**
 * Unclamped linear interpolation through the type's Interpolator.
 * Prefer this over bird::lerp (which clamps) for animation work.
 */
template <typename T>
T interpolate(const T& from, const T& to, float t) {
	return Interpolator<T>::linear(from, to, t);
}

}  // namespace bird

#endif	// BIRD_ANIMATION_INTERPOLATION_H
