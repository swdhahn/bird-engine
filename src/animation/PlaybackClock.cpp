#include "PlaybackClock.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace bird {

namespace {

void requireNonNegativeFinite(float value, const char* what) {
	if (!std::isfinite(value) || value < 0.0f) {
		throw std::invalid_argument(std::string("PlaybackClock: ") + what +
									" must be finite and >= 0");
	}
}

}  // namespace

PlaybackClock::PlaybackClock(float duration) {
	setDuration(duration);
}

void PlaybackClock::setDuration(float duration) {
	requireNonNegativeFinite(duration, "duration");
	m_duration = duration;
	m_position = std::min(m_position, m_duration);
}

void PlaybackClock::setSpeed(float speed) {
	if (!std::isfinite(speed)) {
		throw std::invalid_argument("PlaybackClock: speed must be finite");
	}
	m_speed = speed;
}

void PlaybackClock::setDelay(float delay) {
	requireNonNegativeFinite(delay, "delay");
	m_delay = delay;
	m_delayRemaining = delay;
}

PlaybackEvents PlaybackClock::advance(float delta) {
	requireNonNegativeFinite(delta, "delta");
	PlaybackEvents events;
	if (m_paused || m_finished) return events;

	if (m_delayRemaining > 0.0f) {
		const float consumed = std::min(delta, m_delayRemaining);
		m_delayRemaining -= consumed;
		delta -= consumed;
		if (m_delayRemaining > 0.0f) return events;
	}

	// A zero-length cycle cannot loop meaningfully: it completes at once.
	if (m_duration <= 0.0f) {
		m_position = 0.0f;
		m_finished = true;
		events.finished = true;
		return events;
	}

	const float direction = m_reversed ? -1.0f : 1.0f;
	float position = m_position + delta * m_speed * direction;

	switch (m_loopMode) {
		case LoopMode::Once: {
			const bool pastEnd = position >= m_duration;
			const bool pastStart = position <= 0.0f && m_speed < 0.0f;
			m_position = std::clamp(position, 0.0f, m_duration);
			if (pastEnd || pastStart) {
				m_finished = true;
				events.finished = true;
			}
			return events;
		}
		case LoopMode::Loop: {
			const float wraps = std::floor(position / m_duration);
			m_position = position - wraps * m_duration;
			if (wraps != 0.0f) {
				m_cycles += static_cast<uint32_t>(std::fabs(wraps));
				events.looped = true;
			}
			return events;
		}
		case LoopMode::PingPong: {
			// Whole there-and-back periods change nothing but the count.
			const float period = 2.0f * m_duration;
			uint32_t legs = 0;
			if (position > period || position < -m_duration) {
				const float periods = std::floor(std::fabs(position) / period);
				position = std::fmod(position, period);
				legs += static_cast<uint32_t>(periods) * 2u;
			}
			while (position > m_duration || position < 0.0f) {
				position = position > m_duration ? period - position
												 : -position;
				m_reversed = !m_reversed;
				++legs;
			}
			m_position = position;
			if (legs > 0) {
				m_cycles += legs;
				events.looped = true;
			}
			return events;
		}
	}
	throw std::logic_error("PlaybackClock::advance: unknown LoopMode");
}

void PlaybackClock::seek(float time) {
	if (!std::isfinite(time)) {
		throw std::invalid_argument("PlaybackClock::seek: time must be finite");
	}
	m_position = std::clamp(time, 0.0f, m_duration);
	m_finished = false;
}

void PlaybackClock::restart() {
	m_position = m_speed < 0.0f ? m_duration : 0.0f;
	m_reversed = false;
	m_finished = false;
	m_cycles = 0;
	m_delayRemaining = m_delay;
}

float PlaybackClock::getTime() const {
	return m_position;
}

float PlaybackClock::getNormalizedTime() const {
	return m_duration > 0.0f ? m_position / m_duration : 1.0f;
}

}  // namespace bird
