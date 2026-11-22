#ifndef BIRD_ANIMATION_SKELETALANIMATOR_H
#define BIRD_ANIMATION_SKELETALANIMATOR_H

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "../../util/Constants.h"
#include "../../util/Math.h"
#include "../PlaybackClock.h"
#include "SkeletalClip.h"
#include "Skeleton.h"

namespace bird {

/**
 * Plays SkeletalClips on one skeleton instance and produces its skinning
 * palette. Engine-agnostic (no scene or GPU types), so it is unit-testable
 * and reusable outside SkeletalAnimationComponent.
 *
 * Supports one active clip plus an optional fade-out clip for crossfades.
 * State machines and blend trees build on this later without changing it.
 *
 * Per update: O(animated channels + joints), no allocation once warm.
 */
class SkeletalAnimator {
   public:
	/** @throws std::invalid_argument when skeleton is null. */
	explicit SkeletalAnimator(std::shared_ptr<const Skeleton> skeleton);

	/**
	 * Starts a clip immediately (cancelling any fade).
	 * @param clip Clip authored for this skeleton.
	 * @param loop Looping behaviour.
	 * @throws std::invalid_argument when the clip is null or targets joints
	 *         the skeleton lacks.
	 */
	void play(std::shared_ptr<const SkeletalClip> clip,
			  LoopMode loop = LoopMode::Loop);

	/**
	 * Blends from the current pose to a new clip over fadeSeconds. With no
	 * clip playing this behaves like play(). A fade of zero is a cut.
	 * @throws std::invalid_argument on a null/incompatible clip or a
	 *         negative fade.
	 */
	void crossFade(std::shared_ptr<const SkeletalClip> clip, float fadeSeconds,
				   LoopMode loop = LoopMode::Loop);

	/** Stops all clips; the next update shows the rest pose. */
	void stop();

	/**
	 * Advances clocks and recomputes local pose, model matrices and the
	 * skinning palette.
	 * @throws std::invalid_argument when delta is negative or not finite.
	 */
	void update(float delta);

	/** Playback rate of the active clip (negative plays backwards). */
	void setSpeed(float speed);
	void pause();
	void resume();
	/** Called once when a LoopMode::Once clip reaches its end. */
	void setOnFinished(std::function<void()> callback);

	bool isPlaying() const { return m_current.has_value(); }
	bool isFading() const { return m_previous.has_value(); }
	/** Active clip, or nullptr. */
	BIRD_NO_EXPORT const SkeletalClip* getCurrentClip() const;
	/** Clock of the active clip, or nullptr. */
	BIRD_NO_EXPORT PlaybackClock* getClock();

	BIRD_NO_EXPORT const std::shared_ptr<const Skeleton>& getSkeleton() const {
		return m_skeleton;
	}
	BIRD_NO_EXPORT const Pose& getLocalPose() const { return m_local; }
	BIRD_NO_EXPORT const std::vector<Matrix4>& getModelMatrices() const {
		return m_model;
	}
	BIRD_NO_EXPORT const std::vector<Matrix3x4>& getSkinningPalette() const {
		return m_palette;
	}
	/**
	 * Model-space transform of a joint (attach weapons, hats, effects).
	 * @throws std::out_of_range for an invalid index.
	 */
	Matrix4 getJointModelMatrix(int32_t joint) const;

   private:
	struct Layer {
		std::shared_ptr<const SkeletalClip> clip;
		PlaybackClock clock;
		ClipCursor cursor;
	};

	Layer makeLayer(std::shared_ptr<const SkeletalClip> clip,
					LoopMode loop) const;
	/** Seeds out with the rest pose, then samples the layer over it. */
	void sampleLayer(Layer& layer, Pose& out);
	void computeOutputs();

	std::shared_ptr<const Skeleton> m_skeleton;
	std::optional<Layer> m_current;
	std::optional<Layer> m_previous;
	float m_fadeDuration = 0.0f;
	float m_fadeElapsed = 0.0f;
	std::function<void()> m_onFinished;

	Pose m_local;
	Pose m_fadePose;
	std::vector<Matrix4> m_model;
	std::vector<Matrix3x4> m_palette;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETALANIMATOR_H
