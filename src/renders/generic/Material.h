//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MATERIAL_H
#define BIRD_MATERIAL_H

#include <array>
#include <cstdint>
#include <memory>

#include "../../util/Math.h"
#include "../../util/Memory.hpp"
#include "../../util/Resource.h"
#include "Buffer.h"
#include "GpuLayouts.h"
#include "Texture.h"

namespace bird {

/** Lighting model applied to a material. */
enum class MaterialDomain : uint8_t {
	/** Metallic-roughness PBR, lit by dynamic lights. */
	Lit = 0,
	/** Base color plus emissive only; ignores lights. */
	Unlit = 1
};

/** How a material's coverage combines with what lies behind. */
enum class BlendMode : uint8_t {
	/** Fully opaque; deferred when lit. */
	Opaque = 0,
	/** Opaque with alpha-tested cutout; deferred when lit. */
	Masked = 1,
	/** Alpha blended; always forward, sorted back to front. */
	Transparent = 2,
	/** Additively blended; always forward, sorted back to front. */
	Additive = 3
};

/** Named texture roles; the value equals the sampler binding. */
enum class TextureSlot : uint8_t {
	BaseColor = 0,
	MetallicRoughness = 1,
	Normal = 2,
	Occlusion = 3,
	Emissive = 4,
	Count = 5
};

/**
 * Metallic-roughness PBR surface description.
 *
 * Holds scalar factors and up to one texture per TextureSlot. A material
 * maps onto a std140 uniform block that is uploaded lazily, only after a
 * setter has changed a value. The pipeline selects the shader from the
 * domain and blend mode, so materials carry no shader reference.
 */
class Material : public Resource {
   public:
	Material();
	~Material() override;

	/** Creates a shared material, assignable to meshes and scripts. */
	static _ref<Material> create();

	void read() override;
	void write() override;

	/** Sets the linear base color; alpha drives masking and blending. */
	Material& setBaseColor(const Vector4& color);
	/** Sets metalness in [0, 1]; values are clamped. */
	Material& setMetallic(float metallic);
	/** Sets perceptual roughness in [0.02, 1]; values are clamped. */
	Material& setRoughness(float roughness);
	/** Sets linear emissive color. */
	Material& setEmissive(const Vector3& color);
	/** Sets emissive multiplier, allowing HDR glow above 1. */
	Material& setEmissiveStrength(float strength);
	/** Sets the alpha-test threshold for BlendMode::Masked. */
	Material& setAlphaCutoff(float cutoff);
	/** Scales tangent-space normal perturbation. */
	Material& setNormalScale(float scale);
	/** Sets how strongly the occlusion texture darkens ambient light. */
	Material& setOcclusionStrength(float strength);
	Material& setDomain(MaterialDomain domain);
	Material& setBlendMode(BlendMode mode);
	/** Disables back-face culling for this material. */
	Material& setDoubleSided(bool doubleSided);
	/** Routes an opaque lit material through the forward pass. */
	Material& setForceForward(bool forceForward);
	/** Sets castsShadows; transparent materials never cast. */
	Material& setCastsShadows(bool castsShadows);

	/**
	 * Assigns a texture to a role; nullptr clears the slot.
	 * @throws std::out_of_range for TextureSlot::Count.
	 */
	Material& setTexture(TextureSlot slot, std::shared_ptr<Texture> texture);

	const Vector4& getBaseColor() const { return m_baseColor; }
	float getMetallic() const { return m_metallic; }
	float getRoughness() const { return m_roughness; }
	const Vector3& getEmissive() const { return m_emissive; }
	float getEmissiveStrength() const { return m_emissiveStrength; }
	float getAlphaCutoff() const { return m_alphaCutoff; }
	float getNormalScale() const { return m_normalScale; }
	float getOcclusionStrength() const { return m_occlusionStrength; }
	MaterialDomain getDomain() const { return m_domain; }
	BlendMode getBlendMode() const { return m_blendMode; }
	bool isDoubleSided() const { return m_doubleSided; }
	bool isForceForward() const { return m_forceForward; }
	bool castsShadows() const;

	/** @return Texture in the slot, or nullptr. */
	const std::shared_ptr<Texture>& getTexture(TextureSlot slot) const;

	/** Packs the current parameters into the std140 block. Pure CPU. */
	MaterialUBOData buildGpuData() const BIRD_NO_EXPORT;

	/**
	 * Returns the material's uniform buffer, creating or re-uploading the
	 * buffer when parameters changed. Requires a live graphics context.
	 */
	Buffer<MaterialUBOData>* acquireUniformBuffer() BIRD_NO_EXPORT;

   private:
	Material& markDirty();

	Vector4 m_baseColor{1.0f};
	Vector3 m_emissive{0.0f};
	float m_emissiveStrength = 1.0f;
	float m_metallic = 0.0f;
	float m_roughness = 0.5f;
	float m_alphaCutoff = 0.5f;
	float m_normalScale = 1.0f;
	float m_occlusionStrength = 1.0f;
	MaterialDomain m_domain = MaterialDomain::Lit;
	BlendMode m_blendMode = BlendMode::Opaque;
	bool m_doubleSided = false;
	bool m_forceForward = false;
	bool m_castsShadows = true;

	std::array<std::shared_ptr<Texture>,
			   static_cast<size_t>(TextureSlot::Count)>
		m_textures;

	std::unique_ptr<Buffer<MaterialUBOData>> m_uniformBuffer;
	bool m_gpuDirty = true;
};

}  // namespace bird

#endif	// BIRD_MATERIAL_H
