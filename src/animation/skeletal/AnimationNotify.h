#ifndef BIRD_ANIMATION_ANIMATIONNOTIFY_H
#define BIRD_ANIMATION_ANIMATIONNOTIFY_H

#include <functional>
#include <string>

namespace bird {

/**
 * A timestamped event attached to a SkeletalClip (e.g. footstep, weapon swing
 * hit window, sound cue, or particle burst).
 *
 * Notifies are stored sorted by clip-local time in seconds. During playback,
 * any notify whose timestamp is crossed between the previous and current sample
 * times (including across loop wrap-arounds) is dispatched as an
 * AnimationNotifyEvent.
 */
struct AnimationNotify {
	/** Clip-local timestamp in seconds, in [0, clipDuration]. */
	float time = 0.0f;
	/** Event identifier (e.g. "FootstepLeft", "AttackHit"). */
	std::string name;
	/** Optional user-defined payload string (e.g. surface tag, sound asset). */
	std::string payload;
};

/**
 * Runtime payload delivered to registered notify listeners when an
 * AnimationNotify is crossed during playback.
 *
 * Callbacks registered via SkeletalAnimator::setOnNotify() or
 * SkeletalAnimationComponent::setOnNotify() can forward this directly into
 * game scripts or an engine-level Event System.
 */
struct AnimationNotifyEvent {
	/** Name of the SkeletalClip that triggered the event. */
	std::string clipName;
	/** Event identifier copied from AnimationNotify::name. */
	std::string eventName;
	/** Optional payload string copied from AnimationNotify::payload. */
	std::string payload;
	/** Clip-local timestamp in seconds at which the notify was authored. */
	float time = 0.0f;
};

/** Callback signature invoked whenever an animation notify fires. */
using AnimationNotifyCallback = std::function<void(const AnimationNotifyEvent&)>;

}  // namespace bird

#endif	// BIRD_ANIMATION_ANIMATIONNOTIFY_H
