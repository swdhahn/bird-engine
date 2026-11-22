#include "WorldTweens.h"

#include <stdexcept>
#include <utility>

#include "../scene/WorldObject.h"
#include "Tween.h"

namespace bird {

namespace {

WorldObject* requireTarget(WorldObject* target) {
	if (!target) {
		throw std::invalid_argument("World tween: target must not be null");
	}
	return target;
}

AnimationSystem& requireGlobalSystem() {
	if (!ANIMATION_SYSTEM) {
		throw std::logic_error(
			"World tween: ANIMATION_SYSTEM is null (engine not running)");
	}
	return *ANIMATION_SYSTEM;
}

template <typename T, typename Setter>
AnimationHandle playOwnedTween(AnimationSystem& system, WorldObject* target,
							   const T& from, const T& to, float duration,
							   Ease ease, Setter setter) {
	return system.play(
		_own_mk<Tween<T>>(from, to, duration, ease, std::move(setter)), target);
}

}  // namespace

AnimationHandle tweenLocalPosition(AnimationSystem& system,
								   WorldObject* target, Vector3 to,
								   float duration, Ease ease) {
	requireTarget(target);
	return playOwnedTween<Vector3>(
		system, target, target->getLocalPosition(), to, duration, ease,
		[target](const Vector3& v) { target->setLocalPosition(v); });
}

AnimationHandle tweenLocalRotation(AnimationSystem& system,
								   WorldObject* target, Quaternion to,
								   float duration, Ease ease) {
	requireTarget(target);
	return playOwnedTween<Quaternion>(
		system, target, target->getLocalRotation(), to, duration, ease,
		[target](const Quaternion& q) { target->setLocalRotation(q); });
}

AnimationHandle tweenScale(AnimationSystem& system, WorldObject* target,
						   Vector3 to, float duration, Ease ease) {
	requireTarget(target);
	return playOwnedTween<Vector3>(
		system, target, target->getScale(), to, duration, ease,
		[target](const Vector3& v) { target->setScale(v); });
}

AnimationHandle tweenLocalPosition(WorldObject* target, Vector3 to,
								   float duration, Ease ease) {
	return tweenLocalPosition(requireGlobalSystem(), target, to, duration,
							  ease);
}

AnimationHandle tweenLocalRotation(WorldObject* target, Quaternion to,
								   float duration, Ease ease) {
	return tweenLocalRotation(requireGlobalSystem(), target, to, duration,
							  ease);
}

AnimationHandle tweenScale(WorldObject* target, Vector3 to, float duration,
						   Ease ease) {
	return tweenScale(requireGlobalSystem(), target, to, duration, ease);
}

}  // namespace bird
