#include "UIAnimator.h"

#include <cmath>
#include <string>
#include <stdexcept>
#include <utility>

#include "../../animation/Interpolation.h"
#include "UIManager.h"
#include "UIObject.h"

namespace bird {

/**
 * One animated value. Built-in properties are stored as Vector2 (scalars
 * use x) so a single track type serves every visual property.
 */
struct UIAnimator::PendingTrack {
	/** UIAnimatedProperty::Count marks a custom (setter) track. */
	UIAnimatedProperty property = UIAnimatedProperty::Count;
	std::optional<Vector2> from;
	Vector2 to{0.0f};
	std::function<void(float)> customSetter;
	/** Claim stamp, resolved by start(). */
	uint32_t stamp = 0;

	bool isCustom() const { return property == UIAnimatedProperty::Count; }
};

namespace {

Vector2 readProperty(const UIObject& node, UIAnimatedProperty property) {
	switch (property) {
		case UIAnimatedProperty::Offset:
			return node.getVisualOffset();
		case UIAnimatedProperty::Scale:
			return node.getVisualScale();
		case UIAnimatedProperty::Rotation:
			return Vector2(node.getVisualRotation(), 0.0f);
		case UIAnimatedProperty::Opacity:
			return Vector2(node.getOpacity(), 0.0f);
		case UIAnimatedProperty::Count:
			break;
	}
	throw std::logic_error("UIAnimator: custom tracks have no property");
}

void writeProperty(UIObject& node, UIAnimatedProperty property,
				   const Vector2& value) {
	switch (property) {
		case UIAnimatedProperty::Offset:
			node.setVisualOffset(value);
			return;
		case UIAnimatedProperty::Scale:
			node.setVisualScale(value);
			return;
		case UIAnimatedProperty::Rotation:
			node.setVisualRotation(value.x);
			return;
		case UIAnimatedProperty::Opacity:
			node.setOpacity(value.x);
			return;
		case UIAnimatedProperty::Count:
			break;
	}
	throw std::logic_error("UIAnimator: custom tracks have no property");
}

void requireNonNegativeFinite(float value, const char* what) {
	if (!std::isfinite(value) || value < 0.0f) {
		throw std::invalid_argument(std::string("UIAnimator: ") + what +
									" must be finite and >= 0");
	}
}

}  // namespace

/** All tracks of one transition, driven by one eased clock. */
class UIAnimator::Transition final : public AnimationPlayback {
   public:
	Transition(UIObject& node, float duration, Ease ease,
			   std::shared_ptr<CubicBezierEase> bezier,
			   std::vector<std::shared_ptr<PendingTrack>> tracks)
		: AnimationPlayback(duration),
		  m_node(node),
		  m_ease(ease),
		  m_bezier(std::move(bezier)),
		  m_tracks(std::move(tracks)) {}

   protected:
	void apply(float time) override {
		const float duration = clock().getDuration();
		const float t = duration > 0.0f ? time / duration : 1.0f;
		const float eased =
			m_bezier ? m_bezier->evaluate(t) : evaluateEase(m_ease, t);
		for (const auto& track : m_tracks) {
			const Vector2 value = interpolate(*track->from, track->to, eased);
			if (track->isCustom()) {
				track->customSetter(value.x);
			} else if (m_node.ownsAnimated(track->property, track->stamp)) {
				writeProperty(m_node, track->property, value);
			}
		}
	}

   private:
	UIObject& m_node;
	Ease m_ease;
	std::shared_ptr<CubicBezierEase> m_bezier;
	std::vector<std::shared_ptr<PendingTrack>> m_tracks;
};

// ------------------------------------------------------------- builder

UIAnimator::UIAnimator(UIObject& node)
	: m_node(&node), m_duration(node.getTheme().animationSeconds) {}

UIAnimator& UIAnimator::duration(float seconds) {
	requireNonNegativeFinite(seconds, "duration");
	m_duration = seconds;
	return *this;
}

UIAnimator& UIAnimator::delay(float seconds) {
	requireNonNegativeFinite(seconds, "delay");
	m_delay = seconds;
	return *this;
}

UIAnimator& UIAnimator::ease(Ease ease) {
	m_ease = ease;
	m_bezier.reset();
	return *this;
}

UIAnimator& UIAnimator::bezier(float x1, float y1, float x2, float y2) {
	m_bezier = std::make_shared<CubicBezierEase>(x1, y1, x2, y2);
	return *this;
}

UIAnimator& UIAnimator::loop(LoopMode mode) {
	m_loop = mode;
	return *this;
}

UIAnimator& UIAnimator::onFinished(std::function<void()> callback) {
	m_onFinished = std::move(callback);
	return *this;
}

void UIAnimator::addVectorTrack(UIAnimatedProperty property,
								std::optional<Vector2> from, Vector2 to) {
	auto track = std::make_shared<PendingTrack>();
	track->property = property;
	track->from = from;
	track->to = to;
	m_tracks.push_back(std::move(track));
}

void UIAnimator::addScalarTrack(UIAnimatedProperty property,
								std::optional<float> from, float to) {
	addVectorTrack(property,
				   from ? std::optional<Vector2>(Vector2(*from, 0.0f))
						: std::nullopt,
				   Vector2(to, 0.0f));
}

UIAnimator& UIAnimator::opacity(float to) {
	addScalarTrack(UIAnimatedProperty::Opacity, std::nullopt, to);
	return *this;
}

UIAnimator& UIAnimator::opacity(float from, float to) {
	addScalarTrack(UIAnimatedProperty::Opacity, from, to);
	return *this;
}

UIAnimator& UIAnimator::offset(Vector2 to) {
	addVectorTrack(UIAnimatedProperty::Offset, std::nullopt, to);
	return *this;
}

UIAnimator& UIAnimator::offset(Vector2 from, Vector2 to) {
	addVectorTrack(UIAnimatedProperty::Offset, from, to);
	return *this;
}

UIAnimator& UIAnimator::scale(float to) {
	return scaleXY(Vector2(to));
}

UIAnimator& UIAnimator::scale(float from, float to) {
	return scaleXY(Vector2(from), Vector2(to));
}

UIAnimator& UIAnimator::scaleXY(Vector2 to) {
	addVectorTrack(UIAnimatedProperty::Scale, std::nullopt, to);
	return *this;
}

UIAnimator& UIAnimator::scaleXY(Vector2 from, Vector2 to) {
	addVectorTrack(UIAnimatedProperty::Scale, from, to);
	return *this;
}

UIAnimator& UIAnimator::rotation(float to) {
	addScalarTrack(UIAnimatedProperty::Rotation, std::nullopt, to);
	return *this;
}

UIAnimator& UIAnimator::rotation(float from, float to) {
	addScalarTrack(UIAnimatedProperty::Rotation, from, to);
	return *this;
}

UIAnimator& UIAnimator::value(float from, float to,
							  std::function<void(float)> setter) {
	if (!setter) {
		throw std::invalid_argument("UIAnimator::value: setter is empty");
	}
	auto track = std::make_shared<PendingTrack>();
	track->from = Vector2(from, 0.0f);
	track->to = Vector2(to, 0.0f);
	track->customSetter = std::move(setter);
	m_tracks.push_back(std::move(track));
	return *this;
}

AnimationHandle UIAnimator::start() {
	if (m_started) {
		throw std::logic_error("UIAnimator::start: already started");
	}
	if (m_tracks.empty()) {
		throw std::logic_error("UIAnimator::start: no properties to animate");
	}
	m_started = true;

	// Resolve implicit start values and claim properties now, so a later
	// transition on the same property supersedes this one.
	for (const auto& track : m_tracks) {
		if (!track->from) track->from = readProperty(*m_node, track->property);
		if (!track->isCustom()) {
			track->stamp = m_node->claimAnimated(track->property);
		}
	}

	UIManager* manager = m_node->getManager();
	if (!manager) {
		// Detached: no frames will run, so jump straight to the end state.
		for (const auto& track : m_tracks) {
			if (track->isCustom()) {
				track->customSetter(track->to.x);
			} else {
				writeProperty(*m_node, track->property, track->to);
			}
		}
		if (m_onFinished) m_onFinished();
		return AnimationHandle{};
	}

	auto transition = _own_mk<Transition>(*m_node, m_duration, m_ease,
										  m_bezier, m_tracks);
	transition->clock().setDelay(m_delay);
	transition->clock().setLoopMode(m_loop);
	transition->setOnFinished(m_onFinished);
	// Show the first frame now unless a delay keeps the old state visible.
	if (m_delay <= 0.0f) transition->applyCurrent();
	return manager->getAnimationSystem().play(std::move(transition), m_node);
}

}  // namespace bird
