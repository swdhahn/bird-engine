#include "SkeletalAnimationComponent.h"

#include <stdexcept>
#include <utility>

#include "../../Application.h"
#include "../../scene/WorldObject.h"

namespace bird {

namespace {

std::shared_ptr<const SkeletalAsset> loadFromResources(
	const std::string& modelPath) {
	if (!RESOURCE_MANAGER) {
		throw std::runtime_error(
			"SkeletalAnimationComponent: RESOURCE_MANAGER is null");
	}
	auto asset = RESOURCE_MANAGER->loadSkeletalAsset(modelPath);
	if (!asset) {
		throw std::runtime_error("SkeletalAnimationComponent: '" + modelPath +
								 "' has no baked skeleton (.skel.bird)");
	}
	return asset;
}

}  // namespace

std::shared_ptr<const SkeletalAsset> SkeletalAnimationComponent::requireAsset(
	std::shared_ptr<const SkeletalAsset> asset) {
	if (!asset || !asset->skeleton) {
		throw std::invalid_argument(
			"SkeletalAnimationComponent: asset or skeleton is null");
	}
	return asset;
}

SkeletalAnimationComponent::SkeletalAnimationComponent(
	const std::string& modelPath)
	: SkeletalAnimationComponent(loadFromResources(modelPath)) {}

SkeletalAnimationComponent::SkeletalAnimationComponent(
	std::shared_ptr<const SkeletalAsset> asset)
	: m_asset(requireAsset(std::move(asset))), m_animator(m_asset->skeleton) {}

void SkeletalAnimationComponent::process(float delta) {
	m_animator.update(delta);
	if (m_autoApplyRootMotion) {
		applyRootMotionToOwner();
	}
	if (m_debugDraw && APPLICATION && APPLICATION->getGraphicsPipeline()) {
		debugDraw(APPLICATION->getGraphicsPipeline()->getDebugDraw());
	}
}

bool SkeletalAnimationComponent::play(const std::string& clipName,
									  LoopMode loop) {
	auto clip = m_asset->findClip(clipName);
	if (!clip) return false;
	m_animator.play(std::move(clip), loop);
	return true;
}

bool SkeletalAnimationComponent::crossFade(const std::string& clipName,
										   float fadeSeconds, LoopMode loop) {
	auto clip = m_asset->findClip(clipName);
	if (!clip) return false;
	m_animator.crossFade(std::move(clip), fadeSeconds, loop);
	return true;
}

void SkeletalAnimationComponent::stop() {
	m_animator.stop();
}

void SkeletalAnimationComponent::pause() {
	m_animator.pause();
}

void SkeletalAnimationComponent::resume() {
	m_animator.resume();
}

void SkeletalAnimationComponent::setSpeed(float speed) {
	m_animator.setSpeed(speed);
}

void SkeletalAnimationComponent::setOnFinished(std::function<void()> callback) {
	m_animator.setOnFinished(std::move(callback));
}

bool SkeletalAnimationComponent::isPlaying() const {
	return m_animator.isPlaying();
}

std::string SkeletalAnimationComponent::getCurrentClipName() const {
	const SkeletalClip* clip = m_animator.getCurrentClip();
	return clip ? clip->getName() : std::string();
}

std::vector<std::string> SkeletalAnimationComponent::getClipNames() const {
	std::vector<std::string> names;
	names.reserve(m_asset->clips.size());
	for (const auto& clip : m_asset->clips) names.push_back(clip->getName());
	return names;
}

int32_t SkeletalAnimationComponent::findJoint(const std::string& name) const {
	return m_asset->skeleton->findJoint(name);
}

Matrix4 SkeletalAnimationComponent::getJointModelMatrix(int32_t joint) const {
	return m_animator.getJointModelMatrix(joint);
}

void SkeletalAnimationComponent::setFloat(const std::string& name,
										  float value) {
	m_animator.getParameters().setFloat(name, value);
}

float SkeletalAnimationComponent::getFloat(const std::string& name,
										   float defaultValue) const {
	return m_animator.getParameters().getFloat(name, defaultValue);
}

void SkeletalAnimationComponent::setBool(const std::string& name, bool value) {
	m_animator.getParameters().setBool(name, value);
}

bool SkeletalAnimationComponent::getBool(const std::string& name,
										 bool defaultValue) const {
	return m_animator.getParameters().getBool(name, defaultValue);
}

void SkeletalAnimationComponent::setInt(const std::string& name,
										int32_t value) {
	m_animator.getParameters().setInt(name, value);
}

int32_t SkeletalAnimationComponent::getInt(const std::string& name,
										   int32_t defaultValue) const {
	return m_animator.getParameters().getInt(name, defaultValue);
}

void SkeletalAnimationComponent::setTrigger(const std::string& name) {
	m_animator.getParameters().setTrigger(name);
}

void SkeletalAnimationComponent::resetTrigger(const std::string& name) {
	m_animator.getParameters().resetTrigger(name);
}

bool SkeletalAnimationComponent::isTriggerSet(const std::string& name) const {
	return m_animator.getParameters().isTriggerSet(name);
}

void SkeletalAnimationComponent::setRootNode(
	std::shared_ptr<PoseNode> rootNode) {
	m_animator.setRootNode(std::move(rootNode));
}

size_t SkeletalAnimationComponent::addLayer(AnimationLayer layer) {
	return m_animator.addLayer(std::move(layer));
}

void SkeletalAnimationComponent::setLayerWeight(const std::string& name,
												float weight) {
	m_animator.setLayerWeight(name, weight);
}

bool SkeletalAnimationComponent::removeLayer(const std::string& name) {
	return m_animator.removeLayer(name);
}

void SkeletalAnimationComponent::clearLayers() {
	m_animator.clearLayers();
}

void SkeletalAnimationComponent::setOnNotify(AnimationNotifyCallback callback) {
	m_animator.setOnNotify(std::move(callback));
}

bool SkeletalAnimationComponent::addClipNotify(const std::string& clipName,
											   float time,
											   const std::string& eventName,
											   const std::string& payload) {
	auto clip = m_asset->findClip(clipName);
	if (!clip) return false;
	clip->addNotify(time, eventName, payload);
	return true;
}

void SkeletalAnimationComponent::setRootMotionEnabled(bool enabled,
													  int32_t rootJoint) {
	RootMotionConfig cfg = m_animator.getRootMotionConfig();
	cfg.enabled = enabled;
	cfg.rootJoint = rootJoint;
	m_animator.setRootMotionConfig(cfg);
}

void SkeletalAnimationComponent::setRootMotionConfig(
	const RootMotionConfig& config) {
	m_animator.setRootMotionConfig(config);
}

const RootMotionConfig& SkeletalAnimationComponent::getRootMotionConfig()
	const {
	return m_animator.getRootMotionConfig();
}

const RootMotionDelta& SkeletalAnimationComponent::getRootMotionDelta() const {
	return m_animator.getRootMotionDelta();
}

Vector3 SkeletalAnimationComponent::getRootMotionTranslation() const {
	return m_animator.getRootMotionDelta().translation;
}

void SkeletalAnimationComponent::applyRootMotionToOwner() {
	WorldObject* owner = world_object();
	if (!owner) return;
	const RootMotionDelta& delta = m_animator.getRootMotionDelta();
	const Vector3 worldDelta =
		owner->getWorldRotation() * (delta.translation * owner->getScale());
	if (glm::dot(worldDelta, worldDelta) > 1e-12f) {
		owner->translate(worldDelta);
	}
	if (m_animator.getRootMotionConfig().extractRotationY) {
		owner->setWorldRotation(
			glm::normalize(delta.rotation * owner->getWorldRotation()));
	}
}

void SkeletalAnimationComponent::addIKConstraint(
	const TwoBoneIKConstraint& constraint) {
	m_animator.addIKConstraint(constraint);
}

void SkeletalAnimationComponent::setIKTarget(const std::string& name,
											 const Vector3& targetPosition,
											 float weight) {
	m_animator.setIKTarget(name, targetPosition, weight);
}

void SkeletalAnimationComponent::setIKPole(const std::string& name,
										   const Vector3& polePosition) {
	m_animator.setIKPole(name, polePosition);
}

void SkeletalAnimationComponent::setIKEnabled(const std::string& name,
											  bool enabled) {
	m_animator.setIKEnabled(name, enabled);
}

bool SkeletalAnimationComponent::removeIKConstraint(const std::string& name) {
	return m_animator.removeIKConstraint(name);
}

void SkeletalAnimationComponent::clearIKConstraints() {
	m_animator.clearIKConstraints();
}

void SkeletalAnimationComponent::debugDraw(DebugDrawList& draw) const {
	const WorldObject* owner = m_object;
	const Matrix4 worldTransform =
		owner ? owner->getTransformMatrix() : Matrix4(1.0f);
	m_animator.debugDraw(draw, worldTransform, m_debugSettings);
}

}  // namespace bird
