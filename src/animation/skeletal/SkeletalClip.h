#ifndef BIRD_ANIMATION_SKELETALCLIP_H
#define BIRD_ANIMATION_SKELETALCLIP_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../util/Constants.h"
#include "../Curve.h"
#include "AnimationNotify.h"
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

	/**
	 * Attaches a timestamped AnimationNotify to this clip. Notifies are kept
	 * sorted by time.
	 *
	 * @param notify Event definition; notify.time must be finite and within
	 *               [0, duration], and notify.name must be non-empty.
	 * @throws std::invalid_argument on an invalid timestamp or empty name.
	 */
	BIRD_NO_EXPORT void addNotify(AnimationNotify notify);

	/**
	 * Convenience overload for attaching a notify by timestamp, name, and
	 * optional payload.
	 *
	 * @param time Clip-local timestamp in seconds, in [0, duration].
	 * @param name Event identifier (non-empty).
	 * @param payload Optional string payload.
	 * @throws std::invalid_argument on an invalid timestamp or empty name.
	 */
	void addNotify(float time, const std::string& name,
				   const std::string& payload = "");

	/** Removes all notifies attached to this clip. */
	void clearNotifies() { m_notifies.clear(); }

	/** @return All notifies attached to this clip, sorted by time. */
	BIRD_NO_EXPORT const std::vector<AnimationNotify>& getNotifies() const {
		return m_notifies;
	}

	/**
	 * Collects notifies whose timestamps were crossed between prevTime and
	 * currTime, appending pointers to out without clearing it.
	 *
	 * Forward playback matches (prevTime, currTime] (or [0, currTime] when
	 * includeStart is true, e.g. on the very first tick of a clip). When
	 * wrapped is true on a looping clip, matches (prevTime, duration] followed
	 * by [0, currTime]. Backward playback mirrors these intervals.
	 *
	 * @param prevTime Clip-local time before advancing.
	 * @param currTime Clip-local time after advancing.
	 * @param wrapped True when the playback clock wrapped across a loop boundary.
	 * @param reversed True when playback was moving backward during the step.
	 * @param includeStart True on the first step from time 0 so notifies at
	 *                     t == 0 fire once.
	 * @param out Destination vector receiving pointers to crossed notifies.
	 */
	BIRD_NO_EXPORT void collectNotifies(
		float prevTime, float currTime, bool wrapped, bool reversed,
		bool includeStart, std::vector<const AnimationNotify*>& out) const;

   private:
	std::string m_name;
	float m_duration;
	std::vector<JointTrack> m_tracks;
	std::vector<AnimationNotify> m_notifies;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETALCLIP_H
