#include "SkeletalClip.h"

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

}  // namespace bird
