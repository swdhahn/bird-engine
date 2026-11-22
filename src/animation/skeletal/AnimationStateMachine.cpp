#include "AnimationStateMachine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "PoseOps.h"

namespace bird {

const std::string AnimationStateMachine::kEmptyName;

bool TransitionCondition::evaluate(const AnimationParameters& params) const {
	switch (op) {
		case ConditionOp::FloatGreater:
			return params.getFloat(parameter) > floatValue;
		case ConditionOp::FloatGreaterOrEqual:
			return params.getFloat(parameter) >= floatValue;
		case ConditionOp::FloatLess:
			return params.getFloat(parameter) < floatValue;
		case ConditionOp::FloatLessOrEqual:
			return params.getFloat(parameter) <= floatValue;
		case ConditionOp::IntEqual:
			return params.getInt(parameter) == intValue;
		case ConditionOp::IntNotEqual:
			return params.getInt(parameter) != intValue;
		case ConditionOp::IntGreater:
			return params.getInt(parameter) > intValue;
		case ConditionOp::IntLess:
			return params.getInt(parameter) < intValue;
		case ConditionOp::BoolTrue:
			return params.getBool(parameter);
		case ConditionOp::BoolFalse:
			return !params.getBool(parameter);
		case ConditionOp::Trigger:
			return params.isTriggerSet(parameter);
	}
	return false;
}

void AnimationStateMachine::addState(const std::string& name,
									 std::shared_ptr<PoseNode> node,
									 float speed) {
	if (name.empty()) {
		throw std::invalid_argument(
			"AnimationStateMachine::addState: name must not be empty");
	}
	if (!node) {
		throw std::invalid_argument(
			"AnimationStateMachine::addState: node must not be null");
	}
	if (m_stateByName.find(name) != m_stateByName.end()) {
		throw std::invalid_argument(
			"AnimationStateMachine::addState: duplicate state '" + name + "'");
	}
	const int32_t index = static_cast<int32_t>(m_states.size());
	m_states.push_back(StateEntry{name, std::move(node), speed});
	m_stateByName.emplace(name, index);
	if (m_entryState < 0) {
		m_entryState = index;
		m_currentState = index;
	}
}

void AnimationStateMachine::addClipState(
	const std::string& name, std::shared_ptr<const SkeletalClip> clip,
	LoopMode loop, float speed) {
	addState(name, std::make_shared<ClipPoseNode>(std::move(clip), loop),
			 speed);
}

void AnimationStateMachine::addTransition(AnimationTransition transition) {
	if (!std::isfinite(transition.duration) || transition.duration < 0.0f) {
		throw std::invalid_argument(
			"AnimationStateMachine::addTransition: duration must be finite and >= 0");
	}
	int32_t fromIdx = -1;
	if (!transition.fromState.empty()) {
		fromIdx = findStateIndex(transition.fromState);
		if (fromIdx < 0) {
			throw std::invalid_argument(
				"AnimationStateMachine::addTransition: unknown fromState '" +
				transition.fromState + "'");
		}
	}
	const int32_t toIdx = findStateIndex(transition.toState);
	if (toIdx < 0) {
		throw std::invalid_argument(
			"AnimationStateMachine::addTransition: unknown toState '" +
			transition.toState + "'");
	}

	CompiledTransition compiled;
	compiled.fromIndex = fromIdx;
	compiled.toIndex = toIdx;
	compiled.duration = transition.duration;
	compiled.hasExitTime = transition.hasExitTime;
	compiled.exitTime = std::clamp(transition.exitTime, 0.0f, 1.0f);
	compiled.priority = transition.priority;
	compiled.canTransitionToSelf = transition.canTransitionToSelf;
	compiled.conditions = std::move(transition.conditions);

	// Keep transitions sorted by descending priority (stable among equals).
	const auto it = std::upper_bound(
		m_transitions.begin(), m_transitions.end(), compiled.priority,
		[](int32_t prio, const CompiledTransition& t) {
			return prio > t.priority;
		});
	m_transitions.insert(it, std::move(compiled));
}

void AnimationStateMachine::setEntryState(const std::string& name) {
	const int32_t idx = findStateIndex(name);
	if (idx < 0) {
		throw std::invalid_argument(
			"AnimationStateMachine::setEntryState: unknown state '" + name +
			"'");
	}
	m_entryState = idx;
	if (m_currentState < 0 || m_previousState < 0) {
		m_currentState = idx;
		m_states[static_cast<size_t>(idx)].node->reset();
	}
}

void AnimationStateMachine::forceState(const std::string& name,
									   float fadeDuration) {
	if (!std::isfinite(fadeDuration) || fadeDuration < 0.0f) {
		throw std::invalid_argument(
			"AnimationStateMachine::forceState: fadeDuration must be finite and >= 0");
	}
	const int32_t idx = findStateIndex(name);
	if (idx < 0) {
		throw std::invalid_argument(
			"AnimationStateMachine::forceState: unknown state '" + name + "'");
	}
	beginTransition(idx, fadeDuration);
}

bool AnimationStateMachine::hasState(const std::string& name) const {
	return findStateIndex(name) >= 0;
}

const std::string& AnimationStateMachine::getCurrentStateName() const {
	if (m_currentState < 0) return kEmptyName;
	return m_states[static_cast<size_t>(m_currentState)].name;
}

const std::string& AnimationStateMachine::getPreviousStateName() const {
	if (m_previousState < 0) return kEmptyName;
	return m_states[static_cast<size_t>(m_previousState)].name;
}

float AnimationStateMachine::getTransitionProgress() const {
	if (m_previousState < 0 || m_fadeDuration <= 0.0f) return 1.0f;
	return std::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);
}

int32_t AnimationStateMachine::findStateIndex(const std::string& name) const {
	const auto it = m_stateByName.find(name);
	return it != m_stateByName.end() ? it->second : -1;
}

void AnimationStateMachine::beginTransition(int32_t targetIndex,
											float fadeDuration) {
	if (m_currentState < 0 || fadeDuration <= 0.0f) {
		m_previousState = -1;
		m_currentState = targetIndex;
		m_fadeDuration = 0.0f;
		m_fadeElapsed = 0.0f;
		m_states[static_cast<size_t>(targetIndex)].node->reset();
		return;
	}
	m_previousState = m_currentState;
	m_currentState = targetIndex;
	m_fadeDuration = fadeDuration;
	m_fadeElapsed = 0.0f;
	m_states[static_cast<size_t>(targetIndex)].node->reset();
}

void AnimationStateMachine::checkTransitions(AnimationParameters* params) {
	if (m_currentState < 0) return;
	const StateEntry& current = m_states[static_cast<size_t>(m_currentState)];
	const float normTime = current.node->getNormalizedTime();
	const bool finished = current.node->isFinished();

	for (const CompiledTransition& t : m_transitions) {
		// Match source state or Any-State (-1).
		if (t.fromIndex >= 0 && t.fromIndex != m_currentState) continue;
		if (t.fromIndex < 0 && t.toIndex == m_currentState &&
			!t.canTransitionToSelf) {
			continue;
		}
		// Exit-time gate.
		if (t.hasExitTime && !(normTime >= t.exitTime || finished)) {
			continue;
		}
		// Parameter conditions gate.
		if (!t.conditions.empty()) {
			if (!params) continue;
			bool allMet = true;
			for (const TransitionCondition& cond : t.conditions) {
				if (!cond.evaluate(*params)) {
					allMet = false;
					break;
				}
			}
			if (!allMet) continue;
		}
		// Transition selected: consume any Trigger conditions.
		if (params) {
			for (const TransitionCondition& cond : t.conditions) {
				if (cond.op == ConditionOp::Trigger) {
					params->consumeTrigger(cond.parameter);
				}
			}
		}
		beginTransition(t.toIndex, t.duration);
		return;
	}
}

void AnimationStateMachine::evaluate(const PoseEvalContext& ctx,
									 Pose& outPose) {
	if (!ctx.skeleton) {
		throw std::invalid_argument(
			"AnimationStateMachine::evaluate: skeleton is null");
	}
	if (m_currentState < 0) {
		outPose.joints = ctx.skeleton->getRestPose().joints;
		return;
	}

	// Check transitions before advancing so triggers set this frame take effect
	// immediately.
	checkTransitions(ctx.parameters);

	StateEntry& active = m_states[static_cast<size_t>(m_currentState)];
	PoseEvalContext activeCtx = ctx;
	activeCtx.deltaTime = ctx.deltaTime * active.speed * m_speed;
	active.node->evaluate(activeCtx, outPose);

	if (m_previousState >= 0) {
		m_fadeElapsed += ctx.deltaTime;
		const float weight =
			m_fadeDuration > 0.0f ? (m_fadeElapsed / m_fadeDuration) : 1.0f;
		if (weight >= 1.0f) {
			m_previousState = -1;
			m_fadeElapsed = 0.0f;
		} else {
			StateEntry& prev = m_states[static_cast<size_t>(m_previousState)];
			PoseEvalContext prevCtx = ctx;
			prevCtx.deltaTime = ctx.deltaTime * prev.speed * m_speed;
			prevCtx.outNotifies = nullptr;	// Avoid duplicate notifies on fade-out.
			prev.node->evaluate(prevCtx, m_previousPose);
			blendPoses(m_previousPose, outPose, weight, outPose);
		}
	} else {
		// Re-check exit-time transitions after advancing so a clip that just
		// crossed exitTime this frame starts blending on the next frame (or
		// immediately if needed). Checking post-advance catches finished Once clips
		// cleanly on the very frame they finish.
		const StateEntry& cur = m_states[static_cast<size_t>(m_currentState)];
		if (cur.node->isFinished()) {
			checkTransitions(ctx.parameters);
		}
	}
}

void AnimationStateMachine::reset() {
	m_previousState = -1;
	m_fadeDuration = 0.0f;
	m_fadeElapsed = 0.0f;
	m_currentState = m_entryState;
	for (StateEntry& s : m_states) {
		s.node->reset();
	}
}

float AnimationStateMachine::getNormalizedTime() const {
	if (m_currentState < 0) return 0.0f;
	return m_states[static_cast<size_t>(m_currentState)]
		.node->getNormalizedTime();
}

float AnimationStateMachine::getDuration() const {
	if (m_currentState < 0) return 0.0f;
	return m_states[static_cast<size_t>(m_currentState)].node->getDuration();
}

bool AnimationStateMachine::isFinished() const {
	if (m_currentState < 0) return false;
	return m_states[static_cast<size_t>(m_currentState)].node->isFinished();
}

}  // namespace bird
