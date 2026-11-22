#ifndef BIRD_ANIMATION_SKELETALCLIP_H
#define BIRD_ANIMATION_SKELETALCLIP_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../util/Constants.h"
#include "../Curve.h"
#include "Skeleton.h"

namespace bird {

/**
 * Animation channels for one joint. An empty curve leaves that channel at
 * the skeleton's rest value, so clips only store what actually moves.
 */
struct JointTrack {
	int32_t joint = -1;
	Curve<Vector3> translation;
	Curve<Quaternion> rotation;
	Curve<Vector3> scale;
};

/**
 * Per-playback sampling cursors (three per track), making forward
 * playback amortized O(1) per channel. Owned by whoever plays the clip.
 */
struct ClipCursor {
	std::vector<size_t> positions;
};

/**
 * Keyframed joint animation for one Skeleton, built from the same Curve
 * primitive as every other animation in the engine.
 */
class SkeletalClip {
   public:
	/**
	 * @param name Clip name (as authored, e.g. "Walk").
	 * @param duration Length in seconds; must be finite and >= 0.
	 * @throws std::invalid_argument on a bad duration.
	 */
	SkeletalClip(std::string name, float duration);

	/**
	 * Adds a joint track. At most one track per joint.
	 * @throws std::invalid_argument for a negative joint index or a joint
	 *         that already has a track.
	 */
	BIRD_NO_EXPORT void addTrack(JointTrack track);

	const std::string& getName() const { return m_name; }
	float getDuration() const { return m_duration; }
	int32_t getTrackCount() const { return static_cast<int32_t>(m_tracks.size()); }
	BIRD_NO_EXPORT const std::vector<JointTrack>& getTracks() const {
		return m_tracks;
	}

	/**
	 * @return true when every track names a joint of this skeleton.
	 * Validate once when binding a clip, not per sample.
	 */
	bool isCompatibleWith(const Skeleton& skeleton) const;

	/**
	 * Overwrites the animated channels of pose at a clip-local time.
	 * Channels without keys are left untouched, so callers seed the pose
	 * with the rest pose first. O(channels) with a warm cursor.
	 *
	 * @param time Clip-local time in seconds.
	 * @param pose Pose sized for the clip's skeleton.
	 * @param cursor Sampling cursor; resized on first use.
	 * @throws std::invalid_argument when the pose is too small for a track.
	 */
	BIRD_NO_EXPORT void sample(float time, Pose& pose,
							   ClipCursor& cursor) const;

   private:
	std::string m_name;
	float m_duration;
	std::vector<JointTrack> m_tracks;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETALCLIP_H
