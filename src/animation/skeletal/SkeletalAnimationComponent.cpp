#include "SkeletalAnimationComponent.h"

#include <stdexcept>
#include <utility>

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

}  // namespace bird
