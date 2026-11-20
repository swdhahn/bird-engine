#ifndef BIRD_ANIMATION_SKELETALANIMATIONCOMPONENT_H
#define BIRD_ANIMATION_SKELETALANIMATIONCOMPONENT_H

#include <memory>
#include <string>
#include <vector>

#include "../../scene/components/ProcessScript.h"
#include "../../util/Constants.h"
#include "../../util/Math.h"
#include "../PlaybackClock.h"
#include "SkeletalAnimator.h"
#include "SkeletalAsset.h"

namespace bird {

/**
 * Plays a skinned model's baked clips on a WorldObject.
 *
 * Pair it with a MeshComponent for the same model path: the mesh component
 * draws the meshes, this component poses them. Ticks with the other
 * process scripts each frame, so the palette is ready before rendering.
 *
 * @code
 * object->addComponent(_own_mk<MeshComponent>("assets/models/hero.glb"));
 * auto anim = _own_mk<SkeletalAnimationComponent>("assets/models/hero.glb");
 * anim->play("Idle");
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
	/** Name of the active clip, or an empty string. */
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

	BIRD_NO_EXPORT SkeletalAnimator& getAnimator() { return m_animator; }
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
};

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETALANIMATIONCOMPONENT_H
