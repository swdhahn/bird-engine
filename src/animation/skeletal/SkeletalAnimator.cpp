#include "SkeletalAnimator.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "PoseOps.h"

namespace bird {

SkeletalAnimator::SkeletalAnimator(std::shared_ptr<const Skeleton> skeleton)
	: m_skeleton(std::move(skeleton)) {
	if (!m_skeleton) {
		throw std::invalid_argument("SkeletalAnimator: skeleton is null");
	}
	m_local = m_skeleton->getRestPose();
	computeOutputs();
}

SkeletalAnimator::Layer SkeletalAnimator::makeLayer(
	std::shared_ptr<const SkeletalClip> clip, LoopMode loop) const {
	if (!clip) {
		throw std::invalid_argument("SkeletalAnimator: clip is null");
	}
	if (!clip->isCompatibleWith(*m_skeleton)) {
		throw std::invalid_argument("SkeletalAnimator: clip '" +
									clip->getName() +
									"' targets joints this skeleton lacks");
	}
	Layer layer{clip, PlaybackClock(clip->getDuration()), ClipCursor{}};
	layer.clock.setLoopMode(loop);
	return layer;
}

void SkeletalAnimator::play(std::shared_ptr<const SkeletalClip> clip,
							LoopMode loop) {
	m_current = makeLayer(std::move(clip), loop);
	m_previous.reset();
}

void SkeletalAnimator::crossFade(std::shared_ptr<const SkeletalClip> clip,
								 float fadeSeconds, LoopMode loop) {
	if (!std::isfinite(fadeSeconds) || fadeSeconds < 0.0f) {
		throw std::invalid_argument(
			"SkeletalAnimator::crossFade: fade must be finite and >= 0");
	}
	Layer next = makeLayer(std::move(clip), loop);
	if (!m_current || fadeSeconds == 0.0f) {
		m_current = std::move(next);
		m_previous.reset();
		return;
	}
	// Fading again mid-fade drops the oldest clip; the visible pose may
	// jump slightly, which is the usual trade-off for two-layer fades.
	m_previous = std::move(m_current);
	m_current = std::move(next);
	m_fadeDuration = fadeSeconds;
	m_fadeElapsed = 0.0f;
}

void SkeletalAnimator::stop() {
	m_current.reset();
	m_previous.reset();
}

void SkeletalAnimator::setSpeed(float speed) {
	if (m_current) m_current->clock.setSpeed(speed);
}

void SkeletalAnimator::pause() {
	if (m_current) m_current->clock.pause();
	if (m_previous) m_previous->clock.pause();
}

void SkeletalAnimator::resume() {
	if (m_current) m_current->clock.resume();
	if (m_previous) m_previous->clock.resume();
}

void SkeletalAnimator::setOnFinished(std::function<void()> callback) {
	m_onFinished = std::move(callback);
}

const SkeletalClip* SkeletalAnimator::getCurrentClip() const {
	return m_current ? m_current->clip.get() : nullptr;
}

PlaybackClock* SkeletalAnimator::getClock() {
	return m_current ? &m_current->clock : nullptr;
}

void SkeletalAnimator::sampleLayer(Layer& layer, Pose& out) {
	out.joints = m_skeleton->getRestPose().joints;	// no realloc once warm
	layer.clip->sample(layer.clock.getTime(), out, layer.cursor);
}

void SkeletalAnimator::update(float delta) {
	if (!std::isfinite(delta) || delta < 0.0f) {
		throw std::invalid_argument(
			"SkeletalAnimator::update: delta must be finite and >= 0");
	}
	bool finished = false;
	if (!m_current) {
		m_local.joints = m_skeleton->getRestPose().joints;
	} else {
		finished = m_current->clock.advance(delta).finished;
		sampleLayer(*m_current, m_local);
		if (m_previous && !m_previous->clock.isPaused()) {
			m_previous->clock.advance(delta);
			m_fadeElapsed += delta;
		}
		if (m_previous) {
			const float weight = m_fadeElapsed / m_fadeDuration;
			if (weight >= 1.0f) {
				m_previous.reset();
			} else {
				sampleLayer(*m_previous, m_fadePose);
				blendPoses(m_fadePose, m_local, weight, m_local);
			}
		}
	}
	computeOutputs();
	// Notify last so the callback may start the next clip safely.
	if (finished && m_onFinished) m_onFinished();
}

void SkeletalAnimator::computeOutputs() {
	localToModel(*m_skeleton, m_local, m_model);
	buildSkinningPalette(*m_skeleton, m_model, m_palette);
}

Matrix4 SkeletalAnimator::getJointModelMatrix(int32_t joint) const {
	if (joint < 0 || static_cast<size_t>(joint) >= m_model.size()) {
		throw std::out_of_range(
			"SkeletalAnimator::getJointModelMatrix: bad joint index");
	}
	return m_model[static_cast<size_t>(joint)];
}

}  // namespace bird
