#include "SkeletalAnimator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "PoseOps.h"

namespace bird {

SkeletalAnimator::SkeletalAnimator(std::shared_ptr<const Skeleton> skeleton)
	: m_skeleton(std::move(skeleton)) {
	if (!m_skeleton) {
		throw std::invalid_argument("SkeletalAnimator: skeleton is null");
	}
	m_local = m_skeleton->getRestPose();
	computeOutputs();
}

SkeletalAnimator::Layer SkeletalAnimator::makeLayer(
	std::shared_ptr<const SkeletalClip> clip, LoopMode loop) const {
	if (!clip) {
		throw std::invalid_argument("SkeletalAnimator: clip is null");
	}
	if (!clip->isCompatibleWith(*m_skeleton)) {
		throw std::invalid_argument("SkeletalAnimator: clip '" +
									clip->getName() +
									"' targets joints this skeleton lacks");
	}
	Layer layer{clip, PlaybackClock(clip->getDuration()), ClipCursor{}, false};
	layer.clock.setLoopMode(loop);
	return layer;
}

void SkeletalAnimator::play(std::shared_ptr<const SkeletalClip> clip,
							LoopMode loop) {
	m_rootNode.reset();
	m_current = makeLayer(std::move(clip), loop);
	m_previous.reset();
	m_rootMotion.reset();
}

void SkeletalAnimator::crossFade(std::shared_ptr<const SkeletalClip> clip,
								 float fadeSeconds, LoopMode loop) {
	if (!std::isfinite(fadeSeconds) || fadeSeconds < 0.0f) {
		throw std::invalid_argument(
			"SkeletalAnimator::crossFade: fade must be finite and >= 0");
	}
	m_rootNode.reset();
	Layer next = makeLayer(std::move(clip), loop);
	if (!m_current || fadeSeconds == 0.0f) {
		m_current = std::move(next);
		m_previous.reset();
		m_rootMotion.reset();
		return;
	}
	// Fading again mid-fade drops the oldest clip; the visible pose may
	// jump slightly, which is the usual trade-off for two-layer fades.
	m_previous = std::move(m_current);
	m_current = std::move(next);
	m_fadeDuration = fadeSeconds;
	m_fadeElapsed = 0.0f;
}

void SkeletalAnimator::stop() {
	m_current.reset();
	m_previous.reset();
	m_rootNode.reset();
	m_rootMotion.reset();
}

void SkeletalAnimator::setRootNode(std::shared_ptr<PoseNode> rootNode) {
	m_current.reset();
	m_previous.reset();
	m_rootNode = std::move(rootNode);
	m_rootMotion.reset();
}

size_t SkeletalAnimator::addLayer(AnimationLayer layer) {
	m_layers.push_back(std::move(layer));
	return m_layers.size() - 1;
}

bool SkeletalAnimator::removeLayer(const std::string& name) {
	const auto it = std::find_if(
		m_layers.begin(), m_layers.end(),
		[&](const AnimationLayer& l) { return l.name == name; });
	if (it == m_layers.end()) return false;
	m_layers.erase(it);
	return true;
}

AnimationLayer* SkeletalAnimator::findLayer(const std::string& name) {
	for (AnimationLayer& l : m_layers) {
		if (l.name == name) return &l;
	}
	return nullptr;
}

void SkeletalAnimator::setLayerWeight(const std::string& name, float weight) {
	if (AnimationLayer* layer = findLayer(name)) {
		layer->weight = std::clamp(weight, 0.0f, 1.0f);
	}
}

void SkeletalAnimator::setOnNotify(AnimationNotifyCallback callback) {
	m_onNotify = std::move(callback);
}

void SkeletalAnimator::setRootMotionConfig(const RootMotionConfig& config) {
	m_rootMotion.setConfig(config);
}

size_t SkeletalAnimator::addIKConstraint(TwoBoneIKConstraint constraint) {
	m_ikConstraints.push_back(std::move(constraint));
	return m_ikConstraints.size() - 1;
}

bool SkeletalAnimator::removeIKConstraint(const std::string& name) {
	const auto it = std::find_if(
		m_ikConstraints.begin(), m_ikConstraints.end(),
		[&](const TwoBoneIKConstraint& c) { return c.name == name; });
	if (it == m_ikConstraints.end()) return false;
	m_ikConstraints.erase(it);
	return true;
}

TwoBoneIKConstraint* SkeletalAnimator::findIKConstraint(
	const std::string& name) {
	for (TwoBoneIKConstraint& c : m_ikConstraints) {
		if (c.name == name) return &c;
	}
	return nullptr;
}

void SkeletalAnimator::setIKTarget(const std::string& name,
								   const Vector3& targetPosition,
								   float weight) {
	if (TwoBoneIKConstraint* c = findIKConstraint(name)) {
		c->targetPosition = targetPosition;
		c->weight = std::clamp(weight, 0.0f, 1.0f);
	}
}

void SkeletalAnimator::setIKPole(const std::string& name,
								 const Vector3& polePosition) {
	if (TwoBoneIKConstraint* c = findIKConstraint(name)) {
		c->polePosition = polePosition;
		c->usePolePosition = true;
	}
}

void SkeletalAnimator::setIKEnabled(const std::string& name, bool enabled) {
	if (TwoBoneIKConstraint* c = findIKConstraint(name)) {
		c->enabled = enabled;
	}
}

void SkeletalAnimator::debugDraw(DebugDrawList& draw,
								 const Matrix4& worldTransform,
								 const SkeletalDebugSettings& settings) const {
	drawSkeletonDebug(draw, *m_skeleton, m_model, worldTransform,
					  m_ikConstraints, settings);
}

void SkeletalAnimator::setSpeed(float speed) {
	if (m_current) m_current->clock.setSpeed(speed);
	if (m_rootNode) m_rootNode->setSpeed(speed);
}

void SkeletalAnimator::pause() {
	m_paused = true;
	if (m_current) m_current->clock.pause();
	if (m_previous) m_previous->clock.pause();
}

void SkeletalAnimator::resume() {
	m_paused = false;
	if (m_current) m_current->clock.resume();
	if (m_previous) m_previous->clock.resume();
}

void SkeletalAnimator::setOnFinished(std::function<void()> callback) {
	m_onFinished = std::move(callback);
}

const SkeletalClip* SkeletalAnimator::getCurrentClip() const {
	return m_current ? m_current->clip.get() : nullptr;
}

PlaybackClock* SkeletalAnimator::getClock() {
	return m_current ? &m_current->clock : nullptr;
}

void SkeletalAnimator::sampleLayer(Layer& layer, Pose& out) {
	out.joints = m_skeleton->getRestPose().joints;	// no realloc once warm
	layer.clip->sample(layer.clock.getTime(), out, layer.cursor);
}

void SkeletalAnimator::evaluateLayers(float delta) {
	if (m_layers.empty()) return;
	const float effectiveDelta = m_paused ? 0.0f : delta;
	PoseEvalContext layerCtx{
		m_skeleton.get(), &m_parameters,
		m_onNotify ? &m_pendingNotifies : nullptr, effectiveDelta};

	for (AnimationLayer& layer : m_layers) {
		if (!layer.enabled || !layer.source || layer.weight <= 0.0f) continue;
		layer.source->evaluate(layerCtx, m_layerPose);
		if (layer.blendMode == LayerBlendMode::Override) {
			blendPosesMasked(m_local, m_layerPose, layer.mask, layer.weight,
							 m_local);
		} else {
			const Pose& refPose = layer.referencePose.joints.empty()
									  ? m_skeleton->getRestPose()
									  : layer.referencePose;
			makeAdditivePose(m_layerPose, refPose, m_additivePose);
			applyAdditivePoseMasked(m_local, m_additivePose, layer.mask,
									layer.weight, m_local);
		}
	}
}

void SkeletalAnimator::update(float delta) {
	if (!std::isfinite(delta) || delta < 0.0f) {
		throw std::invalid_argument(
			"SkeletalAnimator::update: delta must be finite and >= 0");
	}
	m_pendingNotifies.clear();
	bool finished = false;
	bool loopWrapped = false;

	if (m_rootNode) {
		const float effectiveDelta = m_paused ? 0.0f : delta;
		const float prevNorm = m_rootNode->getNormalizedTime();
		PoseEvalContext ctx{m_skeleton.get(), &m_parameters,
							m_onNotify ? &m_pendingNotifies : nullptr,
							effectiveDelta};
		m_rootNode->evaluate(ctx, m_local);
		const float currNorm = m_rootNode->getNormalizedTime();
		if (effectiveDelta > 0.0f && currNorm < prevNorm) {
			loopWrapped = true;
		}
	} else if (!m_current) {
		m_local.joints = m_skeleton->getRestPose().joints;
	} else {
		const float prevTime = m_current->clock.getTime();
		const bool includeStart = !m_current->hasTicked;
		const PlaybackEvents ev = m_current->clock.advance(delta);
		const float currTime = m_current->clock.getTime();
		m_current->hasTicked = true;
		finished = ev.finished;
		loopWrapped = ev.looped;

		sampleLayer(*m_current, m_local);

		if (m_onNotify && !m_current->clip->getNotifies().empty() &&
			(delta > 0.0f || includeStart)) {
			const bool reversed = m_current->clock.getSpeed() < 0.0f;
			m_notifyScratch.clear();
			m_current->clip->collectNotifies(prevTime, currTime, ev.looped,
											 reversed, includeStart,
											 m_notifyScratch);
			for (const AnimationNotify* n : m_notifyScratch) {
				m_pendingNotifies.push_back(AnimationNotifyEvent{
					m_current->clip->getName(), n->name, n->payload, n->time});
			}
		}

		if (m_previous && !m_previous->clock.isPaused()) {
			m_previous->clock.advance(delta);
			m_fadeElapsed += delta;
		}
		if (m_previous) {
			const float weight = m_fadeElapsed / m_fadeDuration;
			if (weight >= 1.0f) {
				m_previous.reset();
			} else {
				sampleLayer(*m_previous, m_fadePose);
				blendPoses(m_fadePose, m_local, weight, m_local);
			}
		}
	}

	// Stage 2: Composite bone-masked Override and Additive layers.
	evaluateLayers(delta);

	// Stage 3: Extract optional Root Motion and strip root offset from pose.
	m_rootMotion.extract(*m_skeleton, m_local, loopWrapped);

	// Stages 4-6: FK -> 2-Bone Analytic IK -> GPU skinning palette.
	computeOutputs();

	// Stage 7: Dispatch notifies and completion callbacks after pose is final.
	if (m_onNotify && !m_pendingNotifies.empty()) {
		// Copy queue in case a callback mutates the animator synchronously.
		const std::vector<AnimationNotifyEvent> events = m_pendingNotifies;
		for (const AnimationNotifyEvent& e : events) {
			m_onNotify(e);
		}
	}
	if (finished && m_onFinished) m_onFinished();
}

void SkeletalAnimator::computeOutputs() {
	localToModel(*m_skeleton, m_local, m_model);
	for (const TwoBoneIKConstraint& ik : m_ikConstraints) {
		solveTwoBoneIK(*m_skeleton, m_local, m_model, ik);
	}
	buildSkinningPalette(*m_skeleton, m_model, m_palette);
}

Matrix4 SkeletalAnimator::getJointModelMatrix(int32_t joint) const {
	if (joint < 0 || static_cast<size_t>(joint) >= m_model.size()) {
		throw std::out_of_range(
			"SkeletalAnimator::getJointModelMatrix: bad joint index");
	}
	return m_model[static_cast<size_t>(joint)];
}

}  // namespace bird
