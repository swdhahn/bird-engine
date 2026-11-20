#include "Easing.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace bird {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kBackOvershoot = 1.70158f;
constexpr float kBackOvershootInOut = kBackOvershoot * 1.525f;
constexpr float kElasticPeriod = (2.0f * kPi) / 3.0f;
constexpr float kElasticPeriodInOut = (2.0f * kPi) / 4.5f;

float outBounce(float t) {
	constexpr float n = 7.5625f;
	constexpr float d = 2.75f;
	if (t < 1.0f / d) return n * t * t;
	if (t < 2.0f / d) {
		t -= 1.5f / d;
		return n * t * t + 0.75f;
	}
	if (t < 2.5f / d) {
		t -= 2.25f / d;
		return n * t * t + 0.9375f;
	}
	t -= 2.625f / d;
	return n * t * t + 0.984375f;
}

/** Applies an "in" curve symmetrically as in-out. */
template <typename InCurve>
float inOut(float t, InCurve in) {
	return t < 0.5f ? in(2.0f * t) * 0.5f
					: 1.0f - in(2.0f - 2.0f * t) * 0.5f;
}

}  // namespace

float evaluateEase(Ease ease, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	switch (ease) {
		case Ease::Linear:
			return t;
		case Ease::InQuad:
			return t * t;
		case Ease::OutQuad:
			return 1.0f - (1.0f - t) * (1.0f - t);
		case Ease::InOutQuad:
			return inOut(t, [](float x) { return x * x; });
		case Ease::InCubic:
			return t * t * t;
		case Ease::OutCubic:
			return 1.0f - std::pow(1.0f - t, 3.0f);
		case Ease::InOutCubic:
			return inOut(t, [](float x) { return x * x * x; });
		case Ease::InQuart:
			return t * t * t * t;
		case Ease::OutQuart:
			return 1.0f - std::pow(1.0f - t, 4.0f);
		case Ease::InOutQuart:
			return inOut(t, [](float x) { return x * x * x * x; });
		case Ease::InSine:
			return 1.0f - std::cos(t * kPi * 0.5f);
		case Ease::OutSine:
			return std::sin(t * kPi * 0.5f);
		case Ease::InOutSine:
			return -(std::cos(kPi * t) - 1.0f) * 0.5f;
		case Ease::InExpo:
			return t <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
		case Ease::OutExpo:
			return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
		case Ease::InOutExpo:
			if (t <= 0.0f || t >= 1.0f) return t;
			return inOut(t, [](float x) {
				return std::pow(2.0f, 10.0f * x - 10.0f);
			});
		case Ease::InBack:
			return (kBackOvershoot + 1.0f) * t * t * t -
				   kBackOvershoot * t * t;
		case Ease::OutBack: {
			const float u = t - 1.0f;
			return 1.0f + (kBackOvershoot + 1.0f) * u * u * u +
				   kBackOvershoot * u * u;
		}
		case Ease::InOutBack:
			return inOut(t, [](float x) {
				return (kBackOvershootInOut + 1.0f) * x * x * x -
					   kBackOvershootInOut * x * x;
			});
		case Ease::InElastic:
			if (t <= 0.0f || t >= 1.0f) return t;
			return -std::pow(2.0f, 10.0f * t - 10.0f) *
				   std::sin((10.0f * t - 10.75f) * kElasticPeriod);
		case Ease::OutElastic:
			if (t <= 0.0f || t >= 1.0f) return t;
			return std::pow(2.0f, -10.0f * t) *
					   std::sin((10.0f * t - 0.75f) * kElasticPeriod) +
				   1.0f;
		case Ease::InOutElastic:
			if (t <= 0.0f || t >= 1.0f) return t;
			return inOut(t, [](float x) {
				return -std::pow(2.0f, 10.0f * x - 10.0f) *
					   std::sin((10.0f * x - 11.125f) * kElasticPeriodInOut);
			});
		case Ease::InBounce:
			return 1.0f - outBounce(1.0f - t);
		case Ease::OutBounce:
			return outBounce(t);
		case Ease::InOutBounce:
			return inOut(t, [](float x) { return 1.0f - outBounce(1.0f - x); });
	}
	throw std::invalid_argument("evaluateEase: unknown Ease value");
}

// ------------------------------------------------------------ CubicBezier

CubicBezierEase::CubicBezierEase(float x1, float y1, float x2, float y2)
	: m_x1(x1), m_y1(y1), m_x2(x2), m_y2(y2) {
	if (!(x1 >= 0.0f && x1 <= 1.0f && x2 >= 0.0f && x2 <= 1.0f)) {
		throw std::invalid_argument(
			"CubicBezierEase: control point x values must lie in [0, 1]");
	}
	if (!std::isfinite(y1) || !std::isfinite(y2)) {
		throw std::invalid_argument(
			"CubicBezierEase: control point y values must be finite");
	}
}

namespace {

/** One Bezier coordinate with endpoints 0 and 1 at parameter s. */
float bezierAxis(float c1, float c2, float s) {
	const float u = 1.0f - s;
	return 3.0f * u * u * s * c1 + 3.0f * u * s * s * c2 + s * s * s;
}

float bezierAxisSlope(float c1, float c2, float s) {
	const float u = 1.0f - s;
	return 3.0f * u * u * c1 + 6.0f * u * s * (c2 - c1) +
		   3.0f * s * s * (1.0f - c2);
}

}  // namespace

float CubicBezierEase::evaluate(float t) const {
	t = std::clamp(t, 0.0f, 1.0f);
	constexpr float kEpsilon = 1e-6f;
	float s = t;
	for (int i = 0; i < 8; ++i) {
		const float error = bezierAxis(m_x1, m_x2, s) - t;
		if (std::fabs(error) < kEpsilon) return bezierAxis(m_y1, m_y2, s);
		const float slope = bezierAxisSlope(m_x1, m_x2, s);
		if (std::fabs(slope) < kEpsilon) break;
		s -= error / slope;
	}
	// Newton stalled on a flat region: bisection always converges because
	// x(s) is monotonic for x control points inside [0, 1].
	float low = 0.0f;
	float high = 1.0f;
	s = t;
	for (int i = 0; i < 32; ++i) {
		const float x = bezierAxis(m_x1, m_x2, s);
		if (std::fabs(x - t) < kEpsilon) break;
		(x < t ? low : high) = s;
		s = 0.5f * (low + high);
	}
	return bezierAxis(m_y1, m_y2, s);
}

}  // namespace bird
