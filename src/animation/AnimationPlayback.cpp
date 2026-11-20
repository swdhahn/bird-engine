#include "AnimationPlayback.h"

#include <utility>

namespace bird {

AnimationPlayback::AnimationPlayback(float duration) : m_clock(duration) {}

void AnimationPlayback::setOnFinished(std::function<void()> callback) {
	m_onFinished = std::move(callback);
}

void AnimationPlayback::setOnLoop(std::function<void()> callback) {
	m_onLoop = std::move(callback);
}

bool AnimationPlayback::step(float delta) {
	if (m_clock.isPaused()) return true;
	const PlaybackEvents events = m_clock.advance(delta);
	if (m_clock.isDelayed()) return true;
	apply(m_clock.getTime());
	if (events.looped && m_onLoop) m_onLoop();
	if (events.finished && m_onFinished) m_onFinished();
	return !m_clock.isFinished();
}

}  // namespace bird
