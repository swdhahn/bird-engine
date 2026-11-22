#include "SkeletalClip.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace bird {

SkeletalClip::SkeletalClip(std::string name, float duration)
	: m_name(std::move(name)), m_duration(duration) {
	if (!std::isfinite(duration) || duration < 0.0f) {
		throw std::invalid_argument(
			"SkeletalClip: duration must be finite and >= 0");
	}
}

void SkeletalClip::addTrack(JointTrack track) {
	if (track.joint < 0) {
		throw std::invalid_argument("SkeletalClip::addTrack: bad joint index");
	}
	for (const JointTrack& existing : m_tracks) {
		if (existing.joint == track.joint) {
			throw std::invalid_argument(
				"SkeletalClip::addTrack: joint already has a track");
		}
	}
	m_tracks.push_back(std::move(track));
}

bool SkeletalClip::isCompatibleWith(const Skeleton& skeleton) const {
	for (const JointTrack& track : m_tracks) {
		if (track.joint >= skeleton.getJointCount()) return false;
	}
	return true;
}

void SkeletalClip::sample(float time, Pose& pose, ClipCursor& cursor) const {
	if (cursor.positions.size() != m_tracks.size() * 3) {
		cursor.positions.assign(m_tracks.size() * 3, 0);
	}
	size_t* cursors = cursor.positions.data();
	for (const JointTrack& track : m_tracks) {
		if (static_cast<size_t>(track.joint) >= pose.joints.size()) {
			throw std::invalid_argument(
				"SkeletalClip::sample: pose smaller than the clip's skeleton");
		}
		JointTransform& joint = pose.joints[static_cast<size_t>(track.joint)];
		if (!track.translation.isEmpty()) {
			joint.translation = track.translation.sample(time, cursors[0]);
		}
		if (!track.rotation.isEmpty()) {
			joint.rotation = track.rotation.sample(time, cursors[1]);
		}
		if (!track.scale.isEmpty()) {
			joint.scale = track.scale.sample(time, cursors[2]);
		}
		cursors += 3;
	}
}

void SkeletalClip::addNotify(AnimationNotify notify) {
	if (!std::isfinite(notify.time) || notify.time < 0.0f ||
		notify.time > m_duration) {
		throw std::invalid_argument(
			"SkeletalClip::addNotify: time must be finite and in [0, duration]");
	}
	if (notify.name.empty()) {
		throw std::invalid_argument(
			"SkeletalClip::addNotify: name must not be empty");
	}
	const auto it = std::upper_bound(
		m_notifies.begin(), m_notifies.end(), notify.time,
		[](float t, const AnimationNotify& n) { return t < n.time; });
	m_notifies.insert(it, std::move(notify));
}

void SkeletalClip::addNotify(float time, const std::string& name,
							 const std::string& payload) {
	addNotify(AnimationNotify{time, name, payload});
}

void SkeletalClip::collectNotifies(
	float prevTime, float currTime, bool wrapped, bool reversed,
	bool includeStart, std::vector<const AnimationNotify*>& out) const {
	if (m_notifies.empty()) return;

	if (!reversed) {
		if (!wrapped) {
			if (currTime < prevTime) return;
			for (const AnimationNotify& n : m_notifies) {
				const bool afterPrev =
					includeStart ? (n.time >= prevTime) : (n.time > prevTime);
				if (afterPrev && n.time <= currTime) {
					out.push_back(&n);
				}
			}
		} else {
			// Wrapped forward: (prevTime, duration] then [0, currTime].
			for (const AnimationNotify& n : m_notifies) {
				const bool afterPrev =
					includeStart ? (n.time >= prevTime) : (n.time > prevTime);
				if (afterPrev && n.time <= m_duration) {
					out.push_back(&n);
				}
			}
			for (const AnimationNotify& n : m_notifies) {
				if (n.time >= 0.0f && n.time <= currTime) {
					out.push_back(&n);
				}
			}
		}
	} else {
		if (!wrapped) {
			if (currTime > prevTime) return;
			// Iterate backward so events fire in chronological playback order.
			for (auto it = m_notifies.rbegin(); it != m_notifies.rend(); ++it) {
				const bool beforePrev =
					includeStart ? (it->time <= prevTime) : (it->time < prevTime);
				if (beforePrev && it->time >= currTime) {
					out.push_back(&(*it));
				}
			}
		} else {
			// Wrapped backward: [0, prevTime) then [currTime, duration].
			for (auto it = m_notifies.rbegin(); it != m_notifies.rend(); ++it) {
				const bool beforePrev =
					includeStart ? (it->time <= prevTime) : (it->time < prevTime);
				if (beforePrev && it->time >= 0.0f) {
					out.push_back(&(*it));
				}
			}
			for (auto it = m_notifies.rbegin(); it != m_notifies.rend(); ++it) {
				if (it->time <= m_duration && it->time >= currTime) {
					out.push_back(&(*it));
				}
			}
		}
	}
}

}  // namespace bird
