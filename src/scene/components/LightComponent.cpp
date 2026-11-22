#include "LightComponent.h"

#include <algorithm>
#include <stdexcept>

#include "../../Application.h"

namespace bird {

namespace {
constexpr float MAX_CONE_ANGLE = 1.5533430f;  // 89 degrees
constexpr float MIN_CONE_ANGLE = 0.001f;
}  // namespace

LightComponent::LightComponent(LightType type) : Component(), m_type(type) {}

void LightComponent::init() {
	if (APPLICATION && APPLICATION->getGraphicsPipeline()) {
		APPLICATION->getGraphicsPipeline()->addLight(this);
	}
}

void LightComponent::deinit() {
	if (APPLICATION && APPLICATION->getGraphicsPipeline()) {
		APPLICATION->getGraphicsPipeline()->removeLight(this);
	}
}

void LightComponent::setColor(const Vector3& color) {
	m_color = glm::max(color, Vector3(0.0f));
}

void LightComponent::setIntensity(float intensity) {
	m_intensity = std::max(intensity, 0.0f);
}

void LightComponent::setRange(float range) {
	if (!(range > 0.0f)) {
		throw std::invalid_argument("LightComponent::setRange: range must be > 0.");
	}
	m_range = range;
}

void LightComponent::setConeAngles(float innerAngle, float outerAngle) {
	m_outerConeAngle = std::clamp(outerAngle, MIN_CONE_ANGLE, MAX_CONE_ANGLE);
	m_innerConeAngle = std::clamp(innerAngle, 0.0f, m_outerConeAngle);
}

void LightComponent::setCastsShadows(bool castsShadows) {
	m_castsShadows = castsShadows;
}

void LightComponent::setShadowDepthBias(float bias) {
	m_shadowDepthBias = std::max(bias, 0.0f);
}

void LightComponent::setShadowNormalBias(float bias) {
	m_shadowNormalBias = std::max(bias, 0.0f);
}

Vector3 LightComponent::getWorldPosition() {
	return Vector3(world_object()->getTransformMatrix()[3]);
}

Vector3 LightComponent::getWorldDirection() {
	const Vector3 forward =
		-Vector3(world_object()->getTransformMatrix()[2]);
	const float length = glm::length(forward);
	return length > 0.0f ? forward / length : Vector3(0.0f, -1.0f, 0.0f);
}

}  // namespace bird
