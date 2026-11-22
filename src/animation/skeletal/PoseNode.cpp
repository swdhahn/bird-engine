#include "PoseNode.h"

#include <stdexcept>
#include <utility>

namespace bird {

ClipPoseNode::ClipPoseNode(std::shared_ptr<const SkeletalClip> clip,
						   LoopMode loop)
	: m_clip(std::move(clip)),
	  m_clock(m_clip ? m_clip->getDuration() : 0.0f) {
	if (!m_clip) {
		throw std::invalid_argument("ClipPoseNode: clip must not be null");
	}
	m_clock.setLoopMode(loop);
}

void ClipPoseNode::evaluate(const PoseEvalContext& ctx, Pose& outPose) {
	if (!ctx.skeleton) {
		throw std::invalid_argument("ClipPoseNode::evaluate: skeleton is null");
	}
	const float prevTime = m_clock.getTime();
	const bool includeStart = !m_hasTicked;
	const PlaybackEvents ev = m_clock.advance(ctx.deltaTime);
	const float currTime = m_clock.getTime();
	m_hasTicked = true;
	m_lastLooped = ev.looped;

	outPose.joints = ctx.skeleton->getRestPose().joints;
	m_clip->sample(currTime, outPose, m_cursor);

	if (ctx.outNotifies && !m_clip->getNotifies().empty() &&
		(ctx.deltaTime > 0.0f || includeStart)) {
		const bool reversed = m_clock.getSpeed() < 0.0f;
		m_notifyScratch.clear();
		m_clip->collectNotifies(prevTime, currTime, ev.looped, reversed,
								includeStart, m_notifyScratch);
		for (const AnimationNotify* notify : m_notifyScratch) {
			ctx.outNotifies->push_back(AnimationNotifyEvent{
				m_clip->getName(), notify->name, notify->payload,
				notify->time});
		}
	}
}

void ClipPoseNode::reset() {
	m_clock.restart();
	m_cursor.positions.clear();
	m_hasTicked = false;
	m_lastLooped = false;
}

float ClipPoseNode::getNormalizedTime() const {
	return m_clock.getNormalizedTime();
}

float ClipPoseNode::getDuration() const {
	return m_clip->getDuration();
}

bool ClipPoseNode::isFinished() const {
	return m_clock.isFinished();
}

void ClipPoseNode::setSpeed(float speed) {
	m_clock.setSpeed(speed);
}

void ClipPoseNode::setLoopMode(LoopMode loop) {
	m_clock.setLoopMode(loop);
}

}  // namespace bird
