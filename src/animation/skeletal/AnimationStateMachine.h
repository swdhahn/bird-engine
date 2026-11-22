#ifndef BIRD_ANIMATION_ANIMATIONSTATEMACHINE_H
#define BIRD_ANIMATION_ANIMATIONSTATEMACHINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../util/Constants.h"
#include "AnimationParameters.h"
#include "PoseNode.h"
#include "SkeletalClip.h"

namespace bird {

/**
 * Comparison operator used by TransitionCondition to evaluate an
 * AnimationParameters blackboard entry.
 */
enum class ConditionOp : uint8_t {
	FloatGreater = 0,
	FloatGreaterOrEqual = 1,
	FloatLess = 2,
	FloatLessOrEqual = 3,
	IntEqual = 4,
	IntNotEqual = 5,
	IntGreater = 6,
	IntLess = 7,
	BoolTrue = 8,
	BoolFalse = 9,
	/** Passes when a trigger is armed; consumed automatically when the transition fires. */
	Trigger = 10
};

/**
 * Single boolean predicate evaluated against an AnimationParameters blackboard.
 */
struct TransitionCondition {
	/** Name of the parameter in AnimationParameters. */
	std::string parameter;
	/** Comparison operation to perform. */
	ConditionOp op = ConditionOp::BoolTrue;
	/** Threshold used by Float* operators. */
	float floatValue = 0.0f;
	/** Threshold used by Int* operators. */
	int32_t intValue = 0;

	/**
	 * Evaluates this condition without mutating parameters (triggers are only
	 * consumed once an entire transition is selected).
	 * @param params Entity blackboard.
	 * @return true when the condition is satisfied.
	 */
	bool evaluate(const AnimationParameters& params) const;

	static TransitionCondition floatGreater(std::string param, float value) {
		return {std::move(param), ConditionOp::FloatGreater, value, 0};
	}
	static TransitionCondition floatGreaterOrEqual(std::string param,
												   float value) {
		return {std::move(param), ConditionOp::FloatGreaterOrEqual, value, 0};
	}
	static TransitionCondition floatLess(std::string param, float value) {
		return {std::move(param), ConditionOp::FloatLess, value, 0};
	}
	static TransitionCondition floatLessOrEqual(std::string param,
												float value) {
		return {std::move(param), ConditionOp::FloatLessOrEqual, value, 0};
	}
	static TransitionCondition intEqual(std::string param, int32_t value) {
		return {std::move(param), ConditionOp::IntEqual, 0.0f, value};
	}
	static TransitionCondition intNotEqual(std::string param, int32_t value) {
		return {std::move(param), ConditionOp::IntNotEqual, 0.0f, value};
	}
	static TransitionCondition intGreater(std::string param, int32_t value) {
		return {std::move(param), ConditionOp::IntGreater, 0.0f, value};
	}
	static TransitionCondition intLess(std::string param, int32_t value) {
		return {std::move(param), ConditionOp::IntLess, 0.0f, value};
	}
	static TransitionCondition boolTrue(std::string param) {
		return {std::move(param), ConditionOp::BoolTrue, 0.0f, 0};
	}
	static TransitionCondition boolFalse(std::string param) {
		return {std::move(param), ConditionOp::BoolFalse, 0.0f, 0};
	}
	static TransitionCondition trigger(std::string param) {
		return {std::move(param), ConditionOp::Trigger, 0.0f, 0};
	}
};

/**
 * Directed transition between two states in an AnimationStateMachine.
 *
 * Leaving fromState empty ("") creates an Any-State transition that can fire
 * from any active state (subject to canTransitionToSelf).
 */
struct AnimationTransition {
	/** Source state name, or empty string for an Any-State transition. */
	std::string fromState;
	/** Destination state name (must exist in the state machine). */
	std::string toState;
	/** Cross-fade duration in seconds (>= 0; 0 performs an immediate cut). */
	float duration = 0.2f;
	/** When true, the source state must reach exitTime before transitioning. */
	bool hasExitTime = false;
	/** Normalized source state time in [0, 1] required when hasExitTime is true. */
	float exitTime = 1.0f;
	/** Higher priority transitions are checked first. */
	int32_t priority = 0;
	/** For Any-State transitions, whether the active state may re-enter itself. */
	bool canTransitionToSelf = false;
	/** All conditions must be satisfied for the transition to fire. */
	std::vector<TransitionCondition> conditions;
};

/**
 * Finite State Machine PoseNode driving skeletal animation states, cross-fade
 * transitions, exit-time transitions, and blackboard-driven conditions.
 *
 * Each state wraps any PoseNode (a single ClipPoseNode, a BlendTree1D, a
 * BlendTree2D, or even a nested AnimationStateMachine). Because
 * AnimationStateMachine itself implements PoseNode, it can be used as the base
 * pose source or inside an upper-body AnimationLayer.
 *
 * @code
 * auto sm = std::make_shared<AnimationStateMachine>();
 * sm->addState("Locomotion", locomotionBlendTree);
 * sm->addClipState("Jump", jumpClip, LoopMode::Once);
 * sm->setEntryState("Locomotion");
 * sm->addTransition({"Locomotion", "Jump", 0.1f, false, 1.0f, 0, false,
 *                    {TransitionCondition::trigger("jump")}});
 * sm->addTransition({"Jump", "Locomotion", 0.15f, true, 0.9f, 0, false, {}});
 * @endcode
 */
class BIRD_NO_EXPORT AnimationStateMachine : public PoseNode {
   public:
	AnimationStateMachine() = default;

	/**
	 * Adds a named state backed by an arbitrary PoseNode.
	 * @param name Unique non-empty state name.
	 * @param node Non-null PoseNode (ClipPoseNode, BlendTree1D, BlendTree2D, etc.).
	 * @param speed Per-state playback speed multiplier.
	 * @throws std::invalid_argument on empty/duplicate name or null node.
	 */
	void addState(const std::string& name, std::shared_ptr<PoseNode> node,
				  float speed = 1.0f);

	/**
	 * Convenience helper that wraps a SkeletalClip in a ClipPoseNode and adds
	 * it as a state.
	 * @param name Unique non-empty state name.
	 * @param clip Non-null SkeletalClip.
	 * @param loop Looping mode for the clip.
	 * @param speed Per-state playback speed multiplier.
	 */
	void addClipState(const std::string& name,
					  std::shared_ptr<const SkeletalClip> clip,
					  LoopMode loop = LoopMode::Loop, float speed = 1.0f);

	/**
	 * Registers a transition between states.
	 * @param transition Transition definition; toState (and non-empty fromState)
	 *                   must refer to states already added, and duration >= 0.
	 * @throws std::invalid_argument on unknown state names or negative duration.
	 */
	void addTransition(AnimationTransition transition);

	/**
	 * Sets the initial entry state and activates it if no state is active yet.
	 * @param name Name of an existing state.
	 * @throws std::invalid_argument when the state does not exist.
	 */
	void setEntryState(const std::string& name);

	/**
	 * Immediately transitions to a named state, bypassing transition conditions.
	 * @param name Target state name.
	 * @param fadeDuration Cross-fade duration in seconds (0 = instant cut).
	 * @throws std::invalid_argument on unknown state or negative fadeDuration.
	 */
	void forceState(const std::string& name, float fadeDuration = 0.0f);

	/** @return true when a state with the given name exists. */
	bool hasState(const std::string& name) const;

	/** @return Active state name, or empty string if uninitialized. */
	const std::string& getCurrentStateName() const;

	/** @return Outgoing state name during a cross-fade, or empty string. */
	const std::string& getPreviousStateName() const;

	/** @return true while a cross-fade transition is in progress. */
	bool isTransitioning() const { return m_previousState >= 0; }

	/** @return Cross-fade progress in [0, 1] (1 when not transitioning). */
	float getTransitionProgress() const;

	void evaluate(const PoseEvalContext& ctx, Pose& outPose) override;
	void reset() override;
	float getNormalizedTime() const override;
	float getDuration() const override;
	bool isFinished() const override;
	void setSpeed(float speed) override { m_speed = speed; }

   private:
	struct StateEntry {
		std::string name;
		std::shared_ptr<PoseNode> node;
		float speed = 1.0f;
	};

	struct CompiledTransition {
		int32_t fromIndex = -1;	 // -1 means Any State
		int32_t toIndex = -1;
		float duration = 0.2f;
		bool hasExitTime = false;
		float exitTime = 1.0f;
		int32_t priority = 0;
		bool canTransitionToSelf = false;
		std::vector<TransitionCondition> conditions;
	};

	int32_t findStateIndex(const std::string& name) const;
	void beginTransition(int32_t targetIndex, float fadeDuration);
	void checkTransitions(AnimationParameters* params);

	std::vector<StateEntry> m_states;
	std::unordered_map<std::string, int32_t> m_stateByName;
	std::vector<CompiledTransition> m_transitions;

	int32_t m_entryState = -1;
	int32_t m_currentState = -1;
	int32_t m_previousState = -1;
	float m_fadeDuration = 0.0f;
	float m_fadeElapsed = 0.0f;
	float m_speed = 1.0f;

	Pose m_previousPose;
	static const std::string kEmptyName;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_ANIMATIONSTATEMACHINE_H
