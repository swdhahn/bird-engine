//
// Created by Seth Hahn on 11/30/22.
//

#include "Material.h"

#include <algorithm>
#include <stdexcept>

#include "RawBuffer.h"

namespace bird {

namespace {

size_t slotIndex(TextureSlot slot) {
	const auto index = static_cast<size_t>(slot);
	if (index >= static_cast<size_t>(TextureSlot::Count)) {
		throw std::out_of_range("Material: invalid texture slot.");
	}
	return index;
}

}  // namespace

Material::Material() = default;

Material::~Material() = default;

_ref<Material> Material::create() {
	return std::make_shared<Material>();
}

void Material::read() {}

void Material::write() {}

Material& Material::markDirty() {
	m_gpuDirty = true;
	return *this;
}

Material& Material::setBaseColor(const Vector4& color) {
	m_baseColor = color;
	return markDirty();
}

Material& Material::setMetallic(float metallic) {
	m_metallic = std::clamp(metallic, 0.0f, 1.0f);
	return markDirty();
}

Material& Material::setRoughness(float roughness) {
	// A floor avoids the singular GGX lobe of a perfect mirror.
	m_roughness = std::clamp(roughness, 0.02f, 1.0f);
	return markDirty();
}

Material& Material::setEmissive(const Vector3& color) {
	m_emissive = glm::max(color, Vector3(0.0f));
	return markDirty();
}

Material& Material::setEmissiveStrength(float strength) {
	m_emissiveStrength = std::max(strength, 0.0f);
	return markDirty();
}

Material& Material::setAlphaCutoff(float cutoff) {
	m_alphaCutoff = std::clamp(cutoff, 0.0f, 1.0f);
	return markDirty();
}

Material& Material::setNormalScale(float scale) {
	m_normalScale = scale;
	return markDirty();
}

Material& Material::setOcclusionStrength(float strength) {
	m_occlusionStrength = std::clamp(strength, 0.0f, 1.0f);
	return markDirty();
}

Material& Material::setDomain(MaterialDomain domain) {
	m_domain = domain;
	return markDirty();
}

Material& Material::setBlendMode(BlendMode mode) {
	m_blendMode = mode;
	return markDirty();
}

Material& Material::setDoubleSided(bool doubleSided) {
	m_doubleSided = doubleSided;
	return *this;
}

Material& Material::setForceForward(bool forceForward) {
	m_forceForward = forceForward;
	return *this;
}

Material& Material::setCastsShadows(bool castsShadows) {
	m_castsShadows = castsShadows;
	return *this;
}

bool Material::castsShadows() const {
	return m_castsShadows && (m_blendMode == BlendMode::Opaque ||
							  m_blendMode == BlendMode::Masked);
}

Material& Material::setTexture(TextureSlot slot,
							   std::shared_ptr<Texture> texture) {
	m_textures[slotIndex(slot)] = std::move(texture);
	return markDirty();
}

const std::shared_ptr<Texture>& Material::getTexture(TextureSlot slot) const {
	return m_textures[slotIndex(slot)];
}

MaterialUBOData Material::buildGpuData() const {
	MaterialUBOData data{};
	data.baseColor = m_baseColor;
	data.emissive = Vector4(m_emissive, m_emissiveStrength);
	data.metallic = m_metallic;
	data.roughness = m_roughness;
	data.alphaCutoff = m_alphaCutoff;
	data.normalScale = m_normalScale;
	data.occlusionStrength = m_occlusionStrength;

	static constexpr uint32_t slotBits[] = {
		MATERIAL_TEX_BASE_COLOR, MATERIAL_TEX_METALLIC_ROUGHNESS,
		MATERIAL_TEX_NORMAL, MATERIAL_TEX_OCCLUSION, MATERIAL_TEX_EMISSIVE};
	for (size_t i = 0; i < m_textures.size(); i++) {
		if (m_textures[i]) data.textureMask |= slotBits[i];
	}
	if (m_blendMode == BlendMode::Masked) {
		data.flags |= MATERIAL_FLAG_ALPHA_TEST;
	}
	if (m_domain == MaterialDomain::Unlit) data.flags |= MATERIAL_FLAG_UNLIT;
	return data;
}

Buffer<MaterialUBOData>* Material::acquireUniformBuffer() {
	if (!m_uniformBuffer) {
		auto initial = std::make_unique<MaterialUBOData[]>(1);
		initial[0] = buildGpuData();
		m_uniformBuffer = createBuffer<MaterialUBOData>(
			static_cast<BufferMode>(BUFFER_UNIFORM | BUFFER_STAGED),
			std::move(initial), 1);
		m_uniformBuffer->setBindingPoint(UBO_BINDING_MATERIAL);
		m_uniformBuffer->initialize();
		m_gpuDirty = false;
	} else if (m_gpuDirty) {
		MaterialUBOData data = buildGpuData();
		m_uniformBuffer->update(&data, 1, 0);
		m_gpuDirty = false;
	}
	return m_uniformBuffer.get();
}

}  // namespace bird
