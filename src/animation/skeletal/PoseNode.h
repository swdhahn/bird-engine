#ifndef BIRD_ANIMATION_POSENODE_H
#define BIRD_ANIMATION_POSENODE_H

#include <memory>
#include <vector>

#include "../../util/Constants.h"
#include "../PlaybackClock.h"
#include "AnimationNotify.h"
#include "AnimationParameters.h"
#include "SkeletalClip.h"
#include "Skeleton.h"

namespace bird {

/**
 * Per-frame evaluation context passed down the Pose Evaluation Pipeline.
 *
 * Every PoseNode reads the target skeleton, delta time, and optional
 * AnimationParameters blackboard from this struct, and appends any crossed
 * AnimationNotifyEvent items into outNotifies so the animator can dispatch
 * callbacks safely after pose evaluation completes.
 */
struct PoseEvalContext {
	/** Target skeleton (must be non-null during evaluate()). */
	const Skeleton* skeleton = nullptr;
	/** Per-entity blackboard read by state machines and blend trees (may be null). */
	AnimationParameters* parameters = nullptr;
	/** Deferred notify output buffer (may be null to ignore notifies). */
	std::vector<AnimationNotifyEvent>* outNotifies = nullptr;
	/** Frame delta time in seconds (>= 0). */
	float deltaTime = 0.0f;
};

/**
 * Abstract node in the Pose Evaluation Pipeline.
 *
 * Concrete nodes (ClipPoseNode, BlendTree1D, BlendTree2D,
 * AnimationStateMachine, or custom procedural nodes) advance their internal
 * playback state and write a full joint-local Pose into a caller-provided
 * buffer without per-frame heap allocations once warm.
 */
class BIRD_NO_EXPORT PoseNode {
   public:
	virtual ~PoseNode() = default;

	/**
	 * Advances internal time by ctx.deltaTime and writes the resulting local
	 * pose into outPose.
	 *
	 * @param ctx Per-frame evaluation context (skeleton, parameters, notifies, dt).
	 * @param outPose Destination pose buffer, resized to skeleton joint count.
	 */
	virtual void evaluate(const PoseEvalContext& ctx, Pose& outPose) = 0;

	/** Resets playback state to the beginning (e.g. when re-entering a state). */
	virtual void reset() = 0;

	/** @return Normalized playback progress in [0, 1] (used for exit-time transitions). */
	virtual float getNormalizedTime() const { return 0.0f; }

	/** @return Nominal cycle duration in seconds (0 if stateless). */
	virtual float getDuration() const { return 0.0f; }

	/** @return true when a non-looping node has reached its end. */
	virtual bool isFinished() const { return false; }

	/** Sets the playback rate multiplier (negative plays backward where supported). */
	virtual void setSpeed(float speed) { (void)speed; }

	/** Sets the looping mode (Once, Loop, PingPong). */
	virtual void setLoopMode(LoopMode loop) { (void)loop; }
};

/**
 * Leaf PoseNode that plays a single SkeletalClip using a PlaybackClock and
 * amortized-O(1) ClipCursor, emitting any crossed AnimationNotifies.
 */
class BIRD_NO_EXPORT ClipPoseNode : public PoseNode {
   public:
	/**
	 * @param clip SkeletalClip to sample; must be non-null.
	 * @param loop Looping behaviour (defaults to LoopMode::Loop).
	 * @throws std::invalid_argument when clip is null.
	 */
	explicit ClipPoseNode(std::shared_ptr<const SkeletalClip> clip,
						  LoopMode loop = LoopMode::Loop);

	void evaluate(const PoseEvalContext& ctx, Pose& outPose) override;
	void reset() override;
	float getNormalizedTime() const override;
	float getDuration() const override;
	bool isFinished() const override;
	void setSpeed(float speed) override;
	void setLoopMode(LoopMode loop) override;

	/** @return The underlying SkeletalClip. */
	const std::shared_ptr<const SkeletalClip>& getClip() const { return m_clip; }
	/** @return Mutable access to the playback clock. */
	PlaybackClock& getClock() { return m_clock; }
	const PlaybackClock& getClock() const { return m_clock; }
	/** @return true if the most recent evaluate() wrapped across a loop boundary. */
	bool wasLastStepLooped() const { return m_lastLooped; }

   private:
	std::shared_ptr<const SkeletalClip> m_clip;
	PlaybackClock m_clock;
	ClipCursor m_cursor;
	bool m_hasTicked = false;
	bool m_lastLooped = false;
	std::vector<const AnimationNotify*> m_notifyScratch;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_POSENODE_H
