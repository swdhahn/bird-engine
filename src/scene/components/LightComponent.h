#ifndef BIRD_LIGHT_COMPONENT_H
#define BIRD_LIGHT_COMPONENT_H

#include <cstdint>

#include "Component.h"

namespace bird {

/** Emission geometry of a light. */
enum class LightType : uint8_t {
	/** Parallel rays along the host's -Z axis (sun, moon). */
	Directional = 0,
	/** Omnidirectional from the host's position, bounded by range. */
	Point = 1,
	/** Cone along the host's -Z axis, bounded by range. */
	Spot = 2
};

/**
 * Dynamic light attached to a WorldObject.
 *
 * Position and direction follow the host transform: the light shines along
 * the host's local -Z axis, matching Camera conventions. Registers with the
 * active GraphicsPipeline during init() and unregisters during deinit().
 * Radiance equals color * intensity; point and spot lights fall off with the
 * inverse square of distance, windowed smoothly to zero at range.
 */
class LightComponent : public Component {
   public:
	/** @param type Emission geometry; immutable after construction. */
	explicit LightComponent(LightType type);

	void init() override;
	void deinit() override;

	LightType getType() const { return m_type; }

	/** Sets linear RGB color; negative components are clamped to zero. */
	void setColor(const Vector3& color);
	const Vector3& getColor() const { return m_color; }

	/** Sets a non-negative brightness multiplier. */
	void setIntensity(float intensity);
	float getIntensity() const { return m_intensity; }

	/** Sets the influence radius for point and spot lights (> 0). */
	void setRange(float range);
	float getRange() const { return m_range; }

	/**
	 * Sets the spot cone in radians: full intensity inside innerAngle,
	 * fading to zero at outerAngle. Angles are clamped to (0, 89 degrees]
	 * and inner never exceeds outer.
	 */
	void setConeAngles(float innerAngle, float outerAngle);
	float getInnerConeAngle() const { return m_innerConeAngle; }
	float getOuterConeAngle() const { return m_outerConeAngle; }

	void setCastsShadows(bool castsShadows);
	bool getCastsShadows() const { return m_castsShadows; }

	/** Constant depth offset in shadow-map texels; combats acne. */
	void setShadowDepthBias(float bias);
	float getShadowDepthBias() const { return m_shadowDepthBias; }

	/** Receiver offset along the normal in shadow-map texels; combats acne. */
	void setShadowNormalBias(float bias);
	float getShadowNormalBias() const { return m_shadowNormalBias; }

	/** World-space position of the host. */
	Vector3 getWorldPosition();

	/** Normalized world-space emission direction (host -Z). */
	Vector3 getWorldDirection();

   private:
	LightType m_type;
	Vector3 m_color{1.0f};
	float m_intensity = 1.0f;
	float m_range = 10.0f;
	float m_innerConeAngle = 0.35f;
	float m_outerConeAngle = 0.5f;
	bool m_castsShadows = false;
	float m_shadowDepthBias = 1.0f;
	float m_shadowNormalBias = 1.5f;
};

}  // namespace bird

#endif	// BIRD_LIGHT_COMPONENT_H
