#ifndef BIRD_ANIMATION_CURVE_H
#define BIRD_ANIMATION_CURVE_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "Interpolation.h"

namespace bird {

/** How a curve blends between neighbouring keyframes. */
enum class CurveInterpolation : uint8_t {
	/** Holds each key's value until the next key (discrete values). */
	Step = 0,
	/** Straight-line blend (slerp for quaternions). */
	Linear = 1,
	/** Cubic Hermite spline driven by per-key tangents. */
	CubicHermite = 2
};

/**
 * One sample point of a Curve.
 *
 * Tangents are expressed in value units per second and are only consulted
 * by CurveInterpolation::CubicHermite.
 */
template <typename T>
struct Keyframe {
	float time = 0.0f;
	T value{};
	T inTangent = Interpolator<T>::zero();
	T outTangent = Interpolator<T>::zero();
};

/**
 * Time-sorted keyframe sequence sampled at arbitrary times.
 *
 * Sampling outside the key range clamps to the first or last value; looping
 * and ping-pong are the PlaybackClock's responsibility, not the curve's.
 *
 * Complexity: sample(time) is O(log k) by binary search. sample(time,
 * cursor) is amortized O(1) for monotonic playback, falling back to binary
 * search after a seek. Insertion is O(k).
 *
 * @tparam T Value type with an Interpolator specialisation.
 */
template <typename T>
class Curve {
   public:
	explicit Curve(CurveInterpolation mode = CurveInterpolation::Linear)
		: m_mode(mode) {}

	/**
	 * Inserts a key with zero tangents, keeping keys sorted by time. A key
	 * already at exactly this time is replaced.
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKey(float time, const T& value) {
		addKey(Keyframe<T>{time, value, Interpolator<T>::zero(),
						   Interpolator<T>::zero()});
	}

	/**
	 * Inserts a fully specified key (see addKey(float, const T&)).
	 * @throws std::invalid_argument when the key time is not finite.
	 */
	void addKey(const Keyframe<T>& key) {
		if (!std::isfinite(key.time)) {
			throw std::invalid_argument("Curve::addKey: time must be finite");
		}
		const auto position = std::lower_bound(
			m_keys.begin(), m_keys.end(), key.time,
			[](const Keyframe<T>& k, float t) { return k.time < t; });
		if (position != m_keys.end() && position->time == key.time) {
			*position = key;
		} else {
			m_keys.insert(position, key);
		}
	}

	/**
	 * Replaces every key at once (bulk import path). Keys are sorted here,
	 * so callers may pass them in any order.
	 * @throws std::invalid_argument on a non-finite or duplicate time.
	 */
	void setKeys(std::vector<Keyframe<T>> keys) {
		std::sort(keys.begin(), keys.end(),
				  [](const Keyframe<T>& a, const Keyframe<T>& b) {
					  return a.time < b.time;
				  });
		for (size_t i = 0; i < keys.size(); ++i) {
			if (!std::isfinite(keys[i].time)) {
				throw std::invalid_argument(
					"Curve::setKeys: key times must be finite");
			}
			if (i > 0 && keys[i].time == keys[i - 1].time) {
				throw std::invalid_argument(
					"Curve::setKeys: duplicate key time");
			}
		}
		m_keys = std::move(keys);
	}

	void clear() { m_keys.clear(); }

	/**
	 * Derives Catmull-Rom tangents from neighbouring keys, giving a smooth
	 * spline through every key. End keys receive one-sided tangents.
	 */
	void computeAutoTangents() {
		const size_t count = m_keys.size();
		if (count < 2) return;
		for (size_t i = 0; i < count; ++i) {
			const size_t before = i == 0 ? 0 : i - 1;
			const size_t after = i + 1 == count ? i : i + 1;
			const float span = m_keys[after].time - m_keys[before].time;
			const T slope = (m_keys[after].value - m_keys[before].value) *
							(1.0f / span);
			m_keys[i].inTangent = slope;
			m_keys[i].outTangent = slope;
		}
	}

	void setInterpolation(CurveInterpolation mode) { m_mode = mode; }
	CurveInterpolation getInterpolation() const { return m_mode; }

	const std::vector<Keyframe<T>>& getKeys() const { return m_keys; }
	size_t getKeyCount() const { return m_keys.size(); }
	bool isEmpty() const { return m_keys.empty(); }

	/** Time of the first key (0 when empty). */
	float getStartTime() const {
		return m_keys.empty() ? 0.0f : m_keys.front().time;
	}
	/** Time of the last key (0 when empty). */
	float getEndTime() const {
		return m_keys.empty() ? 0.0f : m_keys.back().time;
	}

	/**
	 * Samples the curve by binary search.
	 * @throws std::logic_error when the curve has no keys.
	 */
	T sample(float time) const {
		size_t cursor = 0;
		return sample(time, cursor);
	}

	/**
	 * Samples with a caller-held cursor (index of the last segment used),
	 * which makes forward playback amortized O(1).
	 *
	 * @param time Sample time in seconds.
	 * @param cursor In/out segment hint; any value is safe.
	 * @throws std::logic_error when the curve has no keys.
	 */
	T sample(float time, size_t& cursor) const {
		if (m_keys.empty()) {
			throw std::logic_error("Curve::sample: curve has no keys");
		}
		if (m_keys.size() == 1 || time <= m_keys.front().time) {
			cursor = 0;
			return m_keys.front().value;
		}
		if (time >= m_keys.back().time) {
			cursor = m_keys.size() - 2;
			return m_keys.back().value;
		}
		const size_t index = findSegment(time, cursor);
		cursor = index;
		const Keyframe<T>& a = m_keys[index];
		const Keyframe<T>& b = m_keys[index + 1];
		const float span = b.time - a.time;
		const float t = (time - a.time) / span;
		switch (m_mode) {
			case CurveInterpolation::Step:
				return a.value;
			case CurveInterpolation::Linear:
				return Interpolator<T>::linear(a.value, b.value, t);
			case CurveInterpolation::CubicHermite:
				return Interpolator<T>::hermite(a.value, a.outTangent, b.value,
												b.inTangent, t, span);
		}
		throw std::logic_error("Curve::sample: unknown interpolation mode");
	}

   private:
	/**
	 * Index i such that keys[i].time <= time < keys[i + 1].time. Requires
	 * at least two keys and time strictly inside the key range.
	 */
	size_t findSegment(float time, size_t hint) const {
		const size_t last = m_keys.size() - 2;
		if (hint <= last) {
			// Fast path: same segment or the next one (forward playback).
			if (m_keys[hint].time <= time && time < m_keys[hint + 1].time) {
				return hint;
			}
			if (hint < last && m_keys[hint + 1].time <= time &&
				time < m_keys[hint + 2].time) {
				return hint + 1;
			}
		}
		const auto upper = std::upper_bound(
			m_keys.begin(), m_keys.end(), time,
			[](float t, const Keyframe<T>& k) { return t < k.time; });
		return static_cast<size_t>(upper - m_keys.begin()) - 1;
	}

	std::vector<Keyframe<T>> m_keys;
	CurveInterpolation m_mode;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_CURVE_H
