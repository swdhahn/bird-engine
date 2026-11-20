#include "Curves.h"

#include <stdexcept>
#include <utility>

#include "Tween.h"

namespace bird {

namespace {

template <typename T>
AnimationHandle playSharedCurve(AnimationSystem& system,
								const std::shared_ptr<Curve<T>>& curve,
								std::function<void(T)> setter) {
	if (!setter) {
		throw std::invalid_argument("playCurve: setter must not be empty");
	}
	return system.play(_own_mk<CurvePlayback<T>>(
		curve, [setter = std::move(setter)](const T& v) { setter(v); }));
}

}  // namespace

// ------------------------------------------------------------ FloatCurve

FloatCurve::FloatCurve(CurveInterpolation mode)
	: m_curve(std::make_shared<Curve<float>>(mode)) {}

void FloatCurve::addKey(float time, float value) {
	m_curve->addKey(time, value);
}

void FloatCurve::addKeyWithTangents(float time, float value, float inTangent,
                                    float outTangent) {
	m_curve->addKey(Keyframe<float>{time, value, inTangent, outTangent});
}

void FloatCurve::computeAutoTangents() {
	m_curve->computeAutoTangents();
}

void FloatCurve::clear() {
	m_curve->clear();
}

void FloatCurve::setInterpolation(CurveInterpolation mode) {
	m_curve->setInterpolation(mode);
}

CurveInterpolation FloatCurve::getInterpolation() const {
	return m_curve->getInterpolation();
}

int32_t FloatCurve::getKeyCount() const {
	return static_cast<int32_t>(m_curve->getKeyCount());
}

float FloatCurve::getStartTime() const {
	return m_curve->getStartTime();
}

float FloatCurve::getEndTime() const {
	return m_curve->getEndTime();
}

float FloatCurve::sample(float time) const {
	return m_curve->sample(time);
}

AnimationHandle FloatCurve::play(AnimationSystem& system,
                                 std::function<void(float)> setter) const {
	return playSharedCurve<float>(system, m_curve, std::move(setter));
}

// ---------------------------------------------------------- Vector2Curve

Vector2Curve::Vector2Curve(CurveInterpolation mode)
	: m_curve(std::make_shared<Curve<Vector2>>(mode)) {}

void Vector2Curve::addKey(float time, Vector2 value) {
	m_curve->addKey(time, value);
}

void Vector2Curve::addKeyWithTangents(float time, Vector2 value, Vector2 inTangent,
                                      Vector2 outTangent) {
	m_curve->addKey(Keyframe<Vector2>{time, value, inTangent, outTangent});
}

void Vector2Curve::computeAutoTangents() {
	m_curve->computeAutoTangents();
}

void Vector2Curve::clear() {
	m_curve->clear();
}

void Vector2Curve::setInterpolation(CurveInterpolation mode) {
	m_curve->setInterpolation(mode);
}

CurveInterpolation Vector2Curve::getInterpolation() const {
	return m_curve->getInterpolation();
}

int32_t Vector2Curve::getKeyCount() const {
	return static_cast<int32_t>(m_curve->getKeyCount());
}

float Vector2Curve::getStartTime() const {
	return m_curve->getStartTime();
}

float Vector2Curve::getEndTime() const {
	return m_curve->getEndTime();
}

Vector2 Vector2Curve::sample(float time) const {
	return m_curve->sample(time);
}

AnimationHandle Vector2Curve::play(AnimationSystem& system,
                                   std::function<void(Vector2)> setter) const {
	return playSharedCurve<Vector2>(system, m_curve, std::move(setter));
}

// ---------------------------------------------------------- Vector3Curve

Vector3Curve::Vector3Curve(CurveInterpolation mode)
	: m_curve(std::make_shared<Curve<Vector3>>(mode)) {}

void Vector3Curve::addKey(float time, Vector3 value) {
	m_curve->addKey(time, value);
}

void Vector3Curve::addKeyWithTangents(float time, Vector3 value, Vector3 inTangent,
                                      Vector3 outTangent) {
	m_curve->addKey(Keyframe<Vector3>{time, value, inTangent, outTangent});
}

void Vector3Curve::computeAutoTangents() {
	m_curve->computeAutoTangents();
}

void Vector3Curve::clear() {
	m_curve->clear();
}

void Vector3Curve::setInterpolation(CurveInterpolation mode) {
	m_curve->setInterpolation(mode);
}

CurveInterpolation Vector3Curve::getInterpolation() const {
	return m_curve->getInterpolation();
}

int32_t Vector3Curve::getKeyCount() const {
	return static_cast<int32_t>(m_curve->getKeyCount());
}

float Vector3Curve::getStartTime() const {
	return m_curve->getStartTime();
}

float Vector3Curve::getEndTime() const {
	return m_curve->getEndTime();
}

Vector3 Vector3Curve::sample(float time) const {
	return m_curve->sample(time);
}

AnimationHandle Vector3Curve::play(AnimationSystem& system,
                                   std::function<void(Vector3)> setter) const {
	return playSharedCurve<Vector3>(system, m_curve, std::move(setter));
}

// ---------------------------------------------------------- Vector4Curve

Vector4Curve::Vector4Curve(CurveInterpolation mode)
	: m_curve(std::make_shared<Curve<Vector4>>(mode)) {}

void Vector4Curve::addKey(float time, Vector4 value) {
	m_curve->addKey(time, value);
}

void Vector4Curve::addKeyWithTangents(float time, Vector4 value, Vector4 inTangent,
                                      Vector4 outTangent) {
	m_curve->addKey(Keyframe<Vector4>{time, value, inTangent, outTangent});
}

void Vector4Curve::computeAutoTangents() {
	m_curve->computeAutoTangents();
}

void Vector4Curve::clear() {
	m_curve->clear();
}

void Vector4Curve::setInterpolation(CurveInterpolation mode) {
	m_curve->setInterpolation(mode);
}

CurveInterpolation Vector4Curve::getInterpolation() const {
	return m_curve->getInterpolation();
}

int32_t Vector4Curve::getKeyCount() const {
	return static_cast<int32_t>(m_curve->getKeyCount());
}

float Vector4Curve::getStartTime() const {
	return m_curve->getStartTime();
}

float Vector4Curve::getEndTime() const {
	return m_curve->getEndTime();
}

Vector4 Vector4Curve::sample(float time) const {
	return m_curve->sample(time);
}

AnimationHandle Vector4Curve::play(AnimationSystem& system,
                                   std::function<void(Vector4)> setter) const {
	return playSharedCurve<Vector4>(system, m_curve, std::move(setter));
}

// ------------------------------------------------------- QuaternionCurve

QuaternionCurve::QuaternionCurve(CurveInterpolation mode)
	: m_curve(std::make_shared<Curve<Quaternion>>(mode)) {}

void QuaternionCurve::addKey(float time, Quaternion value) {
	m_curve->addKey(time, value);
}

void QuaternionCurve::addKeyWithTangents(float time, Quaternion value, Quaternion inTangent,
                                         Quaternion outTangent) {
	m_curve->addKey(Keyframe<Quaternion>{time, value, inTangent, outTangent});
}

void QuaternionCurve::computeAutoTangents() {
	m_curve->computeAutoTangents();
}

void QuaternionCurve::clear() {
	m_curve->clear();
}

void QuaternionCurve::setInterpolation(CurveInterpolation mode) {
	m_curve->setInterpolation(mode);
}

CurveInterpolation QuaternionCurve::getInterpolation() const {
	return m_curve->getInterpolation();
}

int32_t QuaternionCurve::getKeyCount() const {
	return static_cast<int32_t>(m_curve->getKeyCount());
}

float QuaternionCurve::getStartTime() const {
	return m_curve->getStartTime();
}

float QuaternionCurve::getEndTime() const {
	return m_curve->getEndTime();
}

Quaternion QuaternionCurve::sample(float time) const {
	return m_curve->sample(time);
}

AnimationHandle QuaternionCurve::play(AnimationSystem& system,
                                      std::function<void(Quaternion)> setter) const {
	return playSharedCurve<Quaternion>(system, m_curve, std::move(setter));
}

}  // namespace bird
