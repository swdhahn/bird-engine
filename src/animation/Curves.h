#ifndef BIRD_ANIMATION_CURVES_H
#define BIRD_ANIMATION_CURVES_H

#include <cstdint>
#include <functional>
#include <memory>

#include "../util/Constants.h"
#include "../util/Math.h"
#include "AnimationSystem.h"
#include "Curve.h"

namespace bird {

/*
 * Script-facing curve types.
 *
 * Curve<T> is a template, which the Dart binding generator cannot see, so
 * each common value type gets a concrete wrapper with an identical API.
 * Every wrapper shares its underlying Curve<T> (see getCurve()), so one
 * authored curve may drive many simultaneous playbacks without copying.
 */

/** Keyframed curve over scalar values (opacity, intensity, weights). */
class FloatCurve {
   public:
	explicit FloatCurve(CurveInterpolation mode = CurveInterpolation::Linear);

	/**
	 * Inserts a key with zero tangents (replacing one at the same time).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKey(float time, float value);
	/**
	 * Inserts a key with explicit tangents in units per second
	 * (CurveInterpolation::CubicHermite only).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKeyWithTangents(float time, float value, float inTangent,
							float outTangent);
	/** Derives smooth Catmull-Rom tangents from neighbouring keys. */
	void computeAutoTangents();
	void clear();
	void setInterpolation(CurveInterpolation mode);
	CurveInterpolation getInterpolation() const;
	int32_t getKeyCount() const;
	float getStartTime() const;
	float getEndTime() const;
	/**
	 * Samples the curve, clamping outside the key range.
	 * @throws std::logic_error when the curve has no keys.
	 */
	float sample(float time) const;
	/**
	 * Plays this curve from time zero through a setter.
	 * @param system System that advances the playback.
	 * @param setter Receives each sampled value.
	 * @throws std::invalid_argument when the curve is empty or the setter
	 *         is empty.
	 */
	AnimationHandle play(AnimationSystem& system,
						 std::function<void(float)> setter) const;

	/** Shared underlying curve for C++ callers. */
	BIRD_NO_EXPORT const std::shared_ptr<Curve<float>>& getCurve() const {
		return m_curve;
	}

   private:
	std::shared_ptr<Curve<float>> m_curve;
};

/** Keyframed curve over 2D values (UI offsets, scales, UV scroll). */
class Vector2Curve {
   public:
	explicit Vector2Curve(CurveInterpolation mode = CurveInterpolation::Linear);

	/**
	 * Inserts a key with zero tangents (replacing one at the same time).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKey(float time, Vector2 value);
	/**
	 * Inserts a key with explicit tangents in units per second
	 * (CurveInterpolation::CubicHermite only).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKeyWithTangents(float time, Vector2 value, Vector2 inTangent,
							Vector2 outTangent);
	/** Derives smooth Catmull-Rom tangents from neighbouring keys. */
	void computeAutoTangents();
	void clear();
	void setInterpolation(CurveInterpolation mode);
	CurveInterpolation getInterpolation() const;
	int32_t getKeyCount() const;
	float getStartTime() const;
	float getEndTime() const;
	/**
	 * Samples the curve, clamping outside the key range.
	 * @throws std::logic_error when the curve has no keys.
	 */
	Vector2 sample(float time) const;
	/**
	 * Plays this curve from time zero through a setter.
	 * @param system System that advances the playback.
	 * @param setter Receives each sampled value.
	 * @throws std::invalid_argument when the curve is empty or the setter
	 *         is empty.
	 */
	AnimationHandle play(AnimationSystem& system,
						 std::function<void(Vector2)> setter) const;

	/** Shared underlying curve for C++ callers. */
	BIRD_NO_EXPORT const std::shared_ptr<Curve<Vector2>>& getCurve() const {
		return m_curve;
	}

   private:
	std::shared_ptr<Curve<Vector2>> m_curve;
};

/** Keyframed curve over 3D values (positions, scales, colours without alpha). */
class Vector3Curve {
   public:
	explicit Vector3Curve(CurveInterpolation mode = CurveInterpolation::Linear);

	/**
	 * Inserts a key with zero tangents (replacing one at the same time).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKey(float time, Vector3 value);
	/**
	 * Inserts a key with explicit tangents in units per second
	 * (CurveInterpolation::CubicHermite only).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKeyWithTangents(float time, Vector3 value, Vector3 inTangent,
							Vector3 outTangent);
	/** Derives smooth Catmull-Rom tangents from neighbouring keys. */
	void computeAutoTangents();
	void clear();
	void setInterpolation(CurveInterpolation mode);
	CurveInterpolation getInterpolation() const;
	int32_t getKeyCount() const;
	float getStartTime() const;
	float getEndTime() const;
	/**
	 * Samples the curve, clamping outside the key range.
	 * @throws std::logic_error when the curve has no keys.
	 */
	Vector3 sample(float time) const;
	/**
	 * Plays this curve from time zero through a setter.
	 * @param system System that advances the playback.
	 * @param setter Receives each sampled value.
	 * @throws std::invalid_argument when the curve is empty or the setter
	 *         is empty.
	 */
	AnimationHandle play(AnimationSystem& system,
						 std::function<void(Vector3)> setter) const;

	/** Shared underlying curve for C++ callers. */
	BIRD_NO_EXPORT const std::shared_ptr<Curve<Vector3>>& getCurve() const {
		return m_curve;
	}

   private:
	std::shared_ptr<Curve<Vector3>> m_curve;
};

/** Keyframed curve over 4D values (RGBA colours). */
class Vector4Curve {
   public:
	explicit Vector4Curve(CurveInterpolation mode = CurveInterpolation::Linear);

	/**
	 * Inserts a key with zero tangents (replacing one at the same time).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKey(float time, Vector4 value);
	/**
	 * Inserts a key with explicit tangents in units per second
	 * (CurveInterpolation::CubicHermite only).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKeyWithTangents(float time, Vector4 value, Vector4 inTangent,
							Vector4 outTangent);
	/** Derives smooth Catmull-Rom tangents from neighbouring keys. */
	void computeAutoTangents();
	void clear();
	void setInterpolation(CurveInterpolation mode);
	CurveInterpolation getInterpolation() const;
	int32_t getKeyCount() const;
	float getStartTime() const;
	float getEndTime() const;
	/**
	 * Samples the curve, clamping outside the key range.
	 * @throws std::logic_error when the curve has no keys.
	 */
	Vector4 sample(float time) const;
	/**
	 * Plays this curve from time zero through a setter.
	 * @param system System that advances the playback.
	 * @param setter Receives each sampled value.
	 * @throws std::invalid_argument when the curve is empty or the setter
	 *         is empty.
	 */
	AnimationHandle play(AnimationSystem& system,
						 std::function<void(Vector4)> setter) const;

	/** Shared underlying curve for C++ callers. */
	BIRD_NO_EXPORT const std::shared_ptr<Curve<Vector4>>& getCurve() const {
		return m_curve;
	}

   private:
	std::shared_ptr<Curve<Vector4>> m_curve;
};

/** Keyframed curve over rotations (shortest-arc slerp, normalized splines). */
class QuaternionCurve {
   public:
	explicit QuaternionCurve(CurveInterpolation mode = CurveInterpolation::Linear);

	/**
	 * Inserts a key with zero tangents (replacing one at the same time).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKey(float time, Quaternion value);
	/**
	 * Inserts a key with explicit tangents in units per second
	 * (CurveInterpolation::CubicHermite only).
	 * @throws std::invalid_argument when time is not finite.
	 */
	void addKeyWithTangents(float time, Quaternion value, Quaternion inTangent,
							Quaternion outTangent);
	/** Derives smooth Catmull-Rom tangents from neighbouring keys. */
	void computeAutoTangents();
	void clear();
	void setInterpolation(CurveInterpolation mode);
	CurveInterpolation getInterpolation() const;
	int32_t getKeyCount() const;
	float getStartTime() const;
	float getEndTime() const;
	/**
	 * Samples the curve, clamping outside the key range.
	 * @throws std::logic_error when the curve has no keys.
	 */
	Quaternion sample(float time) const;
	/**
	 * Plays this curve from time zero through a setter.
	 * @param system System that advances the playback.
	 * @param setter Receives each sampled value.
	 * @throws std::invalid_argument when the curve is empty or the setter
	 *         is empty.
	 */
	AnimationHandle play(AnimationSystem& system,
						 std::function<void(Quaternion)> setter) const;

	/** Shared underlying curve for C++ callers. */
	BIRD_NO_EXPORT const std::shared_ptr<Curve<Quaternion>>& getCurve() const {
		return m_curve;
	}

   private:
	std::shared_ptr<Curve<Quaternion>> m_curve;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_CURVES_H
