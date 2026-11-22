#ifndef BIRD_ANIMATION_SKELETALANIMATIONCOMPONENT_H
#define BIRD_ANIMATION_SKELETALANIMATIONCOMPONENT_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../../scene/components/ProcessScript.h"
#include "../../util/Constants.h"
#include "../../util/Math.h"
#include "../PlaybackClock.h"
#include "AnimationLayer.h"
#include "AnimationNotify.h"
#include "AnimationParameters.h"
#include "PoseNode.h"
#include "RootMotion.h"
#include "SkeletalAnimator.h"
#include "SkeletalAsset.h"
#include "SkeletalDebugDraw.h"
#include "TwoBoneIK.h"

namespace bird {

/**
 * Plays a skinned model's baked clips, state machines, blend trees, layered
 * masks, 2-bone IK, and root motion on a WorldObject.
 *
 * Pair it with a MeshComponent for the same model path: the mesh component
 * draws the meshes, this component poses them. Ticks with the other
 * process scripts each frame, so the palette is ready before rendering.
 *
 * Dart and C++ scripts can drive transitions and blend trees each frame using
 * the per-entity parameter blackboard (`setFloat`, `setBool`, `setInt`,
 * `setTrigger`).
 *
 * @code
 * object->addComponent(_own_mk<MeshComponent>("assets/models/hero.glb"));
 * auto anim = _own_mk<SkeletalAnimationComponent>("assets/models/hero.glb");
 * anim->play("Idle");
 * anim->setFloat("speed", 4.2f);
 * anim->setDebugDraw(true);
 * object->addComponent(std::move(anim));
 * @endcode
 */
class SkeletalAnimationComponent : public ProcessScriptComponent {
   public:
	/**
	 * Loads <modelPath>.skel.bird through RESOURCE_MANAGER.
	 * @throws std::runtime_error when the model has no skeleton bake or
	 *         the engine has no resource manager.
	 */
	explicit SkeletalAnimationComponent(const std::string& modelPath);
	/**
	 * Uses an already loaded asset (tests, procedural rigs).
	 * @throws std::invalid_argument when asset or its skeleton is null.
	 */
	BIRD_NO_EXPORT explicit SkeletalAnimationComponent(
		std::shared_ptr<const SkeletalAsset> asset);

	void init() override {}
	void deinit() override {}
	void process(float delta) override;

	// ---- Direct clip playback ----

	/**
	 * Starts a clip by name immediately.
	 * @return false when the model has no clip with that name.
	 */
	bool play(const std::string& clipName, LoopMode loop = LoopMode::Loop);
	/**
	 * Blends into a clip by name over fadeSeconds.
	 * @return false when the model has no clip with that name.
	 * @throws std::invalid_argument when fadeSeconds is negative.
	 */
	bool crossFade(const std::string& clipName, float fadeSeconds,
				   LoopMode loop = LoopMode::Loop);
	void stop();
	void pause();
	void resume();
	void setSpeed(float speed);
	/** Called when a LoopMode::Once clip reaches its end. */
	void setOnFinished(std::function<void()> callback);

	bool isPlaying() const;
	/** Name of the active direct clip, or an empty string. */
	std::string getCurrentClipName() const;
	/** Names of every clip baked into the model. */
	std::vector<std::string> getClipNames() const;
	/** @return the joint index, or -1 when absent. */
	int32_t findJoint(const std::string& name) const;
	/**
	 * Joint transform in the object's local (model) space, for attaching
	 * props: multiply by the object's transform for world space.
	 * @throws std::out_of_range for an invalid joint index.
	 */
	Matrix4 getJointModelMatrix(int32_t joint) const;

	// ---- Per-entity parameter blackboard (C++ & Dart) ----

	/** Sets a float parameter on this entity's animation blackboard. */
	void setFloat(const std::string& name, float value);
	/** Reads a float parameter from this entity's animation blackboard. */
	float getFloat(const std::string& name, float defaultValue = 0.0f) const;
	/** Sets a boolean parameter on this entity's animation blackboard. */
	void setBool(const std::string& name, bool value);
	/** Reads a boolean parameter from this entity's animation blackboard. */
	bool getBool(const std::string& name, bool defaultValue = false) const;
	/** Sets an integer parameter on this entity's animation blackboard. */
	void setInt(const std::string& name, int32_t value);
	/** Reads an integer parameter from this entity's animation blackboard. */
	int32_t getInt(const std::string& name, int32_t defaultValue = 0) const;
	/** Arms a one-shot trigger parameter consumed by state transitions. */
	void setTrigger(const std::string& name);
	/** Clears an armed trigger parameter. */
	void resetTrigger(const std::string& name);
	/** @return true if the named trigger is currently armed. */
	bool isTriggerSet(const std::string& name) const;

	/** Direct access to the underlying AnimationParameters blackboard. */
	AnimationParameters& getParameters() { return m_animator.getParameters(); }
	const AnimationParameters& getParameters() const {
		return m_animator.getParameters();
	}

	// ---- Pose graph, state machine & layers ----

	/** Assigns a PoseNode (e.g. AnimationStateMachine or BlendTree) as root. */
	BIRD_NO_EXPORT void setRootNode(std::shared_ptr<PoseNode> rootNode);

	/** Appends a bone-masked Override or Additive AnimationLayer. */
	BIRD_NO_EXPORT size_t addLayer(AnimationLayer layer);
	/** Sets the global blend weight [0, 1] of a named layer. */
	void setLayerWeight(const std::string& name, float weight);
	/** Removes a named layer. @return true if found and removed. */
	bool removeLayer(const std::string& name);
	/** Removes all overlay layers. */
	void clearLayers();

	// ---- Animation Events (Notifies) ----

	/**
	 * Registers a listener for AnimationNotifyEvent dispatches (can plug into
	 * an engine Event System or Dart script callback).
	 */
	void setOnNotify(AnimationNotifyCallback callback);

	/**
	 * Attaches a timestamped notify to one of the model's loaded clips by name.
	 * @return false when no clip with clipName exists on this asset.
	 */
	bool addClipNotify(const std::string& clipName, float time,
					   const std::string& eventName,
					   const std::string& payload = "");

	// ---- Root Motion ----

	/** Enables or disables horizontal (XZ) root motion extraction on rootJoint. */
	void setRootMotionEnabled(bool enabled, int32_t rootJoint = 0);
	/** Applies a detailed RootMotionConfig. */
	void setRootMotionConfig(const RootMotionConfig& config);
	const RootMotionConfig& getRootMotionConfig() const;
	/** @return Root motion delta extracted during the last process() tick. */
	const RootMotionDelta& getRootMotionDelta() const;
	/** @return Model-space root translation delta for the last tick. */
	Vector3 getRootMotionTranslation() const;
	/**
	 * When true, process() automatically rotates the extracted root motion
	 * translation by the owning WorldObject's world rotation and translates it.
	 */
	void setAutoApplyRootMotion(bool enabled) { m_autoApplyRootMotion = enabled; }
	bool isAutoApplyRootMotionEnabled() const { return m_autoApplyRootMotion; }
	/**
	 * Manually applies the most recent root motion delta to the owning
	 * WorldObject's world position (and optional yaw rotation).
	 */
	void applyRootMotionToOwner();

	// ---- 2-Bone Analytic IK ----

	/** Registers a 2-Bone IK constraint solved post-pose and pre-skinning. */
	void addIKConstraint(const TwoBoneIKConstraint& constraint);
	/** Updates a named IK constraint's model-space target and blend weight. */
	void setIKTarget(const std::string& name, const Vector3& targetPosition,
					 float weight = 1.0f);
	/** Updates a named IK constraint's model-space pole hint position. */
	void setIKPole(const std::string& name, const Vector3& polePosition);
	/** Enables or disables a named IK constraint. */
	void setIKEnabled(const std::string& name, bool enabled);
	/** Removes a named IK constraint. @return true if removed. */
	bool removeIKConstraint(const std::string& name);
	/** Removes all IK constraints. */
	void clearIKConstraints();

	// ---- Debug Visualization ----

	/**
	 * Enables or disables per-frame skeleton bone and IK target debug drawing
	 * via GraphicsPipeline::getDebugDraw().
	 */
	void setDebugDraw(bool enabled) { m_debugDraw = enabled; }
	bool isDebugDrawEnabled() const { return m_debugDraw; }
	SkeletalDebugSettings& getDebugSettings() { return m_debugSettings; }
	const SkeletalDebugSettings& getDebugSettings() const {
		return m_debugSettings;
	}

	/**
	 * Emits this component's skeleton and IK debug lines into an explicit
	 * DebugDrawList using the owning WorldObject's transform (or identity).
	 */
	BIRD_NO_EXPORT void debugDraw(DebugDrawList& draw) const;

	// ---- Engine / renderer accessors ----

	BIRD_NO_EXPORT SkeletalAnimator& getAnimator() { return m_animator; }
	BIRD_NO_EXPORT const SkeletalAnimator& getAnimator() const {
		return m_animator;
	}
	BIRD_NO_EXPORT const std::shared_ptr<const SkeletalAsset>& getAsset() const {
		return m_asset;
	}
	/** Palette for the renderer's skinning uniform block. */
	BIRD_NO_EXPORT const std::vector<Matrix3x4>& getSkinningPalette() const {
		return m_animator.getSkinningPalette();
	}

   private:
	static std::shared_ptr<const SkeletalAsset> requireAsset(
		std::shared_ptr<const SkeletalAsset> asset);

	std::shared_ptr<const SkeletalAsset> m_asset;
	SkeletalAnimator m_animator;
	bool m_autoApplyRootMotion = false;
	bool m_debugDraw = false;
	SkeletalDebugSettings m_debugSettings;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETALANIMATIONCOMPONENT_H
