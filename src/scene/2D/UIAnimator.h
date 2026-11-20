#ifndef BIRD_UIANIMATOR_H
#define BIRD_UIANIMATOR_H

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "../../animation/AnimationSystem.h"
#include "../../animation/Easing.h"
#include "../../animation/PlaybackClock.h"
#include "../../util/Math.h"
#include "UIVisualTransform.h"

namespace bird {

class UIObject;

/**
 * Fluent builder for a UI transition: several properties of one node
 * animated together on one clock, eased identically.
 *
 * @code
 * panel->animate().opacity(0.0f, 1.0f).scale(0.9f, 1.0f)
 *     .duration(0.18f).ease(Ease::OutBack).start();
 * @endcode
 *
 * Omitted start values are read from the node when start() runs. A newer
 * transition on the same property takes that property over: the older
 * transition keeps animating only the properties it still owns.
 * Transitions are cancelled automatically when their node is removed.
 * Defaults: duration UITheme::animationSeconds, Ease::OutCubic.
 */
class UIAnimator {
   public:
	explicit UIAnimator(UIObject& node);

	/** @throws std::invalid_argument when negative or not finite. */
	UIAnimator& duration(float seconds);
	/** @throws std::invalid_argument when negative or not finite. */
	UIAnimator& delay(float seconds);
	UIAnimator& ease(Ease ease);
	/**
	 * CSS cubic-bezier timing in place of a named Ease.
	 * @throws std::invalid_argument when an x value leaves [0, 1].
	 */
	UIAnimator& bezier(float x1, float y1, float x2, float y2);
	UIAnimator& loop(LoopMode mode);
	/** Called once the transition completes (not when cancelled). */
	UIAnimator& onFinished(std::function<void()> callback);

	/** Fades from the current opacity. */
	UIAnimator& opacity(float to);
	UIAnimator& opacity(float from, float to);
	/** Slides the visual offset (pixels) from its current value. */
	UIAnimator& offset(Vector2 to);
	UIAnimator& offset(Vector2 from, Vector2 to);
	/** Scales uniformly from the current scale. */
	UIAnimator& scale(float to);
	UIAnimator& scale(float from, float to);
	UIAnimator& scaleXY(Vector2 to);
	UIAnimator& scaleXY(Vector2 from, Vector2 to);
	/** Rotates (radians) from the current rotation. */
	UIAnimator& rotation(float to);
	UIAnimator& rotation(float from, float to);
	/**
	 * Animates any float through a setter (widget-specific state such as a
	 * knob position). Custom tracks never conflict with each other.
	 * @throws std::invalid_argument when the setter is empty.
	 */
	UIAnimator& value(float from, float to, std::function<void(float)> setter);

	/**
	 * Starts the transition on the node's UIManager. A detached node has
	 * no frames to animate in, so its tracks snap to their final values.
	 * @return Handle on the manager's AnimationSystem (null when snapped).
	 * @throws std::logic_error when started twice or with no tracks.
	 */
	AnimationHandle start();

   private:
	struct PendingTrack;
	class Transition;

	void addVectorTrack(UIAnimatedProperty property,
						std::optional<Vector2> from, Vector2 to);
	void addScalarTrack(UIAnimatedProperty property,
						std::optional<float> from, float to);

	UIObject* m_node;
	float m_duration;
	float m_delay = 0.0f;
	Ease m_ease = Ease::OutCubic;
	std::shared_ptr<CubicBezierEase> m_bezier;
	LoopMode m_loop = LoopMode::Once;
	std::function<void()> m_onFinished;
	std::vector<std::shared_ptr<PendingTrack>> m_tracks;
	bool m_started = false;
};

}  // namespace bird

#endif	// BIRD_UIANIMATOR_H
