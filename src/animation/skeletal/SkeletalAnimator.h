#ifndef BIRD_ANIMATION_SKELETALANIMATOR_H
#define BIRD_ANIMATION_SKELETALANIMATOR_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../util/Constants.h"
#include "../../util/Math.h"
#include "../PlaybackClock.h"
#include "AnimationLayer.h"
#include "AnimationNotify.h"
#include "AnimationParameters.h"
#include "PoseNode.h"
#include "RootMotion.h"
#include "SkeletalClip.h"
#include "SkeletalDebugDraw.h"
#include "Skeleton.h"
#include "TwoBoneIK.h"

namespace bird {

/**
 * Evaluates skeletal animation poses on one skeleton instance and produces its
 * GPU skinning palette. Engine-agnostic (no scene or GPU types), so it is
 * unit-testable and reusable outside SkeletalAnimationComponent.
 *
 * Per-frame Pose Evaluation Pipeline (zero heap allocation once warm):
 * 1. Base pose: evaluates either a root PoseNode (AnimationStateMachine,
 *    BlendTree1D, BlendTree2D, ClipPoseNode) or direct play()/crossFade() clips.
 * 2. Animation layers: composites Override and Additive layers with BoneMasks.
 * 3. Root motion: extracts per-frame root bone delta and zeros root offset.
 * 4. Forward kinematics: converts joint-local Pose to model-space matrices.
 * 5. Post-pose IK: solves enabled 2-Bone Analytic IK constraints in model space.
 * 6. Skinning: packs `model * inverseBind` into the mat3x4 GPU palette.
 * 7. Notifications: dispatches crossed AnimationNotifyEvent & onFinished callbacks.
 */
class SkeletalAnimator {
   public:
	/** @throws std::invalid_argument when skeleton is null. */
	explicit SkeletalAnimator(std::shared_ptr<const Skeleton> skeleton);

	// ---- Direct clip playback (simple mode) ----

	/**
	 * Starts a clip immediately (cancelling any fade and clearing any root PoseNode).
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

	/** Stops all direct clips and clears the root PoseNode; next update shows rest pose. */
	void stop();

	// ---- Pose graph / State Machine root ----

	/**
	 * Assigns a PoseNode (such as an AnimationStateMachine, BlendTree1D, or
	 * BlendTree2D) as the base pose generator, replacing direct clip playback.
	 * @param rootNode Root PoseNode, or nullptr to return to rest pose.
	 */
	BIRD_NO_EXPORT void setRootNode(std::shared_ptr<PoseNode> rootNode);

	/** @return Active root PoseNode, or nullptr when using direct clip playback. */
	BIRD_NO_EXPORT const std::shared_ptr<PoseNode>& getRootNode() const {
		return m_rootNode;
	}

	// ---- Per-entity parameter blackboard ----

	/** @return Mutable blackboard read by state machines and blend trees. */
	AnimationParameters& getParameters() { return m_parameters; }
	const AnimationParameters& getParameters() const { return m_parameters; }

	// ---- Bone-masked & additive animation layers ----

	/**
	 * Appends an overlay AnimationLayer to the layer stack.
	 * @param layer Layer definition (source, BoneMask, Override/Additive, weight).
	 * @return Index of the newly added layer.
	 */
	BIRD_NO_EXPORT size_t addLayer(AnimationLayer layer);

	/**
	 * Removes the first layer matching name.
	 * @return true if a layer was removed.
	 */
	bool removeLayer(const std::string& name);

	/** @return Pointer to the named layer, or nullptr if not found. */
	BIRD_NO_EXPORT AnimationLayer* findLayer(const std::string& name);

	/** Sets the global blend weight [0, 1] of a named layer (no-op if absent). */
	void setLayerWeight(const std::string& name, float weight);

	/** Removes all overlay layers. */
	void clearLayers() { m_layers.clear(); }

	BIRD_NO_EXPORT const std::vector<AnimationLayer>& getLayers() const {
		return m_layers;
	}

	// ---- Animation Events (Notifies) ----

	/**
	 * Registers a callback invoked whenever any active clip crosses an
	 * AnimationNotify timestamp. Dispatched after pose/IK/palette evaluation.
	 */
	void setOnNotify(AnimationNotifyCallback callback);

	// ---- Root Motion Extraction ----

	/** Configures optional per-frame root bone motion extraction. */
	void setRootMotionConfig(const RootMotionConfig& config);
	const RootMotionConfig& getRootMotionConfig() const {
		return m_rootMotion.getConfig();
	}
	/** Resets root motion tracking state (avoids deltas across teleports/cuts). */
	void resetRootMotion() { m_rootMotion.reset(); }
	/** @return Root motion delta extracted during the most recent update(). */
	const RootMotionDelta& getRootMotionDelta() const {
		return m_rootMotion.getLastDelta();
	}

	// ---- Post-Pose 2-Bone Analytic IK ----

	/**
	 * Registers a 2-Bone Analytic IK constraint solved after pose evaluation
	 * and before skinning palette construction.
	 * @return Index of the added constraint.
	 */
	size_t addIKConstraint(TwoBoneIKConstraint constraint);

	/** Removes the first IK constraint matching name. @return true if removed. */
	bool removeIKConstraint(const std::string& name);

	/** @return Mutable pointer to the named IK constraint, or nullptr. */
	BIRD_NO_EXPORT TwoBoneIKConstraint* findIKConstraint(
		const std::string& name);

	/**
	 * Updates the model-space target position and blend weight of a named IK
	 * constraint (no-op if absent).
	 */
	void setIKTarget(const std::string& name, const Vector3& targetPosition,
					 float weight = 1.0f);

	/** Updates the model-space pole hint position of a named IK constraint. */
	void setIKPole(const std::string& name, const Vector3& polePosition);

	/** Enables or disables a named IK constraint. */
	void setIKEnabled(const std::string& name, bool enabled);

	/** Removes all IK constraints. */
	void clearIKConstraints() { m_ikConstraints.clear(); }

	BIRD_NO_EXPORT const std::vector<TwoBoneIKConstraint>& getIKConstraints()
		const {
		return m_ikConstraints;
	}

	// ---- Debug visualization ----

	/**
	 * Emits skeleton bone lines and active IK target markers into a
	 * DebugDrawList using the provided world transform.
	 */
	BIRD_NO_EXPORT void debugDraw(
		DebugDrawList& draw, const Matrix4& worldTransform,
		const SkeletalDebugSettings& settings = {}) const;

	// ---- Per-frame update & playback controls ----

	/**
	 * Advances clocks, evaluates the pose pipeline (base -> layers -> root
	 * motion -> FK -> IK -> skinning palette), and dispatches notifies.
	 * @throws std::invalid_argument when delta is negative or not finite.
	 */
	void update(float delta);

	/** Playback rate of the active clip or root PoseNode. */
	void setSpeed(float speed);
	void pause();
	void resume();
	/** Called once when a LoopMode::Once direct clip reaches its end. */
	void setOnFinished(std::function<void()> callback);

	bool isPlaying() const {
		return m_current.has_value() || m_rootNode != nullptr;
	}
	bool isFading() const { return m_previous.has_value(); }
	/** Active direct clip, or nullptr. */
	BIRD_NO_EXPORT const SkeletalClip* getCurrentClip() const;
	/** Clock of the active direct clip, or nullptr. */
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
		bool hasTicked = false;
	};

	Layer makeLayer(std::shared_ptr<const SkeletalClip> clip,
					LoopMode loop) const;
	/** Seeds out with the rest pose, then samples the layer over it. */
	void sampleLayer(Layer& layer, Pose& out);
	void evaluateLayers(float delta);
	void computeOutputs();

	std::shared_ptr<const Skeleton> m_skeleton;
	std::optional<Layer> m_current;
	std::optional<Layer> m_previous;
	float m_fadeDuration = 0.0f;
	float m_fadeElapsed = 0.0f;
	bool m_paused = false;
	std::function<void()> m_onFinished;

	std::shared_ptr<PoseNode> m_rootNode;
	AnimationParameters m_parameters;
	std::vector<AnimationLayer> m_layers;
	AnimationNotifyCallback m_onNotify;
	RootMotionExtractor m_rootMotion;
	std::vector<TwoBoneIKConstraint> m_ikConstraints;

	Pose m_local;
	Pose m_fadePose;
	Pose m_layerPose;
	Pose m_additivePose;
	std::vector<Matrix4> m_model;
	std::vector<Matrix3x4> m_palette;
	std::vector<AnimationNotifyEvent> m_pendingNotifies;
	std::vector<const AnimationNotify*> m_notifyScratch;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETALANIMATOR_H
