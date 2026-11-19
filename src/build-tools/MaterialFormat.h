#ifndef BIRD_MATERIAL_FORMAT_H
#define BIRD_MATERIAL_FORMAT_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * Baked material file (<model><index>.mat.bird), shared by the offline baker
 * and the runtime loader. Header-only and free of engine types so that both
 * sides link the same definition.
 *
 * Version 2 layout (little endian, packed):
 *   char     magic[4] = "BMAT"
 *   uint16   version
 *   float    baseColor[4], emissive[3], emissiveStrength,
 *            metallic, roughness, alphaCutoff, normalScale, occlusionStrength
 *   uint8    blendMode, doubleSided, unlit, textureCount
 *   textureCount x { uint8 slot, uint16 pathLength, char path[pathLength] }
 *
 * Legacy (unversioned) files start directly with a Blinn-Phong diffuse color
 * and are converted on read.
 */
namespace bird::format {

constexpr char MATERIAL_MAGIC[4] = {'B', 'M', 'A', 'T'};
constexpr uint16_t MATERIAL_FORMAT_VERSION = 2;

/** Texture role identifiers; values equal bird::TextureSlot. */
enum MaterialTextureSlot : uint8_t {
	MATERIAL_SLOT_BASE_COLOR = 0,
	MATERIAL_SLOT_METALLIC_ROUGHNESS = 1,
	MATERIAL_SLOT_NORMAL = 2,
	MATERIAL_SLOT_OCCLUSION = 3,
	MATERIAL_SLOT_EMISSIVE = 4,
	MATERIAL_SLOT_COUNT = 5
};

/** Blend identifiers; values equal bird::BlendMode. */
enum MaterialBlend : uint8_t {
	MATERIAL_BLEND_OPAQUE = 0,
	MATERIAL_BLEND_MASKED = 1,
	MATERIAL_BLEND_TRANSPARENT = 2,
	MATERIAL_BLEND_ADDITIVE = 3
};

/** Decoded material file contents. */
struct MaterialFileData {
	float baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float emissive[3] = {0.0f, 0.0f, 0.0f};
	float emissiveStrength = 1.0f;
	float metallic = 0.0f;
	float roughness = 0.5f;
	float alphaCutoff = 0.5f;
	float normalScale = 1.0f;
	float occlusionStrength = 1.0f;
	uint8_t blendMode = MATERIAL_BLEND_OPAQUE;
	uint8_t doubleSided = 0;
	uint8_t unlit = 0;
	/** (slot, path relative to the material file's directory). */
	std::vector<std::pair<uint8_t, std::string>> textures;
	/** True when decoded from the legacy Blinn-Phong layout. */
	bool legacy = false;
};

/**
 * Maps a Blinn-Phong specular exponent to perceptual roughness via the
 * common equivalence alpha^2 = 2 / (n + 2), roughness = sqrt(alpha).
 */
inline float shininessToRoughness(float shininess) {
	const float alphaSquared = 2.0f / (std::max(shininess, 0.0f) + 2.0f);
	return std::sqrt(std::sqrt(alphaSquared));
}

namespace detail {

template <typename T>
void readValue(std::istream& in, T& value) {
	in.read(reinterpret_cast<char*>(&value), sizeof(T));
	if (!in) throw std::runtime_error("Truncated material file.");
}

template <typename T>
void writeValue(std::ostream& out, const T& value) {
	out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

inline std::string readString(std::istream& in) {
	uint16_t length = 0;
	readValue(in, length);
	std::string text(length, '\0');
	if (length > 0) in.read(&text[0], length);
	if (!in) throw std::runtime_error("Truncated material file.");
	return text;
}

inline MaterialFileData readLegacy(std::istream& in) {
	MaterialFileData data;
	data.legacy = true;
	float diffuse[3], specular[3], ambient[3], shininess, opacity,
		emission[3], reflectivity;
	for (float& v : diffuse) readValue(in, v);
	for (float& v : specular) readValue(in, v);
	for (float& v : ambient) readValue(in, v);
	readValue(in, shininess);
	readValue(in, opacity);
	for (float& v : emission) readValue(in, v);
	readValue(in, reflectivity);

	std::memcpy(data.baseColor, diffuse, sizeof(diffuse));
	data.baseColor[3] = opacity;
	std::memcpy(data.emissive, emission, sizeof(emission));
	data.roughness = shininessToRoughness(shininess);
	data.metallic = 0.0f;
	if (opacity < 0.999f) data.blendMode = MATERIAL_BLEND_TRANSPARENT;
	// The legacy renderer never culled faces; keep that for old assets.
	data.doubleSided = 1;

	uint8_t textureCount = 0;
	readValue(in, textureCount);
	for (uint8_t i = 0; i < textureCount; i++) {
		std::string path = readString(in);
		// Legacy files discarded texture roles; the first texture was the
		// only one ever sampled, as diffuse.
		if (i == 0) data.textures.emplace_back(MATERIAL_SLOT_BASE_COLOR, path);
	}
	return data;
}

}  // namespace detail

/**
 * Decodes a material stream of either version.
 * @param in Binary input stream positioned at the start of the file.
 * @return Decoded data.
 * @throws std::runtime_error on truncation, unknown version, or bad slot.
 */
inline MaterialFileData readMaterialFile(std::istream& in) {
	char magic[4] = {};
	in.read(magic, sizeof(magic));
	if (!in) throw std::runtime_error("Truncated material file.");
	if (std::memcmp(magic, MATERIAL_MAGIC, sizeof(magic)) != 0) {
		in.clear();
		in.seekg(0);
		return detail::readLegacy(in);
	}

	uint16_t version = 0;
	detail::readValue(in, version);
	if (version != MATERIAL_FORMAT_VERSION) {
		throw std::runtime_error("Unsupported material format version " +
								 std::to_string(version));
	}
	MaterialFileData data;
	for (float& v : data.baseColor) detail::readValue(in, v);
	for (float& v : data.emissive) detail::readValue(in, v);
	detail::readValue(in, data.emissiveStrength);
	detail::readValue(in, data.metallic);
	detail::readValue(in, data.roughness);
	detail::readValue(in, data.alphaCutoff);
	detail::readValue(in, data.normalScale);
	detail::readValue(in, data.occlusionStrength);
	detail::readValue(in, data.blendMode);
	detail::readValue(in, data.doubleSided);
	detail::readValue(in, data.unlit);
	if (data.blendMode > MATERIAL_BLEND_ADDITIVE) {
		throw std::runtime_error("Invalid material blend mode.");
	}
	uint8_t textureCount = 0;
	detail::readValue(in, textureCount);
	for (uint8_t i = 0; i < textureCount; i++) {
		uint8_t slot = 0;
		detail::readValue(in, slot);
		if (slot >= MATERIAL_SLOT_COUNT) {
			throw std::runtime_error("Invalid material texture slot.");
		}
		data.textures.emplace_back(slot, detail::readString(in));
	}
	return data;
}

/**
 * Encodes material data in the current version.
 * @throws std::invalid_argument when a texture path exceeds 65535 bytes or
 *         more than 255 textures are present.
 */
inline void writeMaterialFile(std::ostream& out, const MaterialFileData& data) {
	if (data.textures.size() > 255) {
		throw std::invalid_argument("Too many material textures.");
	}
	out.write(MATERIAL_MAGIC, sizeof(MATERIAL_MAGIC));
	detail::writeValue(out, MATERIAL_FORMAT_VERSION);
	for (float v : data.baseColor) detail::writeValue(out, v);
	for (float v : data.emissive) detail::writeValue(out, v);
	detail::writeValue(out, data.emissiveStrength);
	detail::writeValue(out, data.metallic);
	detail::writeValue(out, data.roughness);
	detail::writeValue(out, data.alphaCutoff);
	detail::writeValue(out, data.normalScale);
	detail::writeValue(out, data.occlusionStrength);
	detail::writeValue(out, data.blendMode);
	detail::writeValue(out, data.doubleSided);
	detail::writeValue(out, data.unlit);
	detail::writeValue(out, static_cast<uint8_t>(data.textures.size()));
	for (const auto& [slot, path] : data.textures) {
		if (path.size() > 0xFFFF) {
			throw std::invalid_argument("Material texture path too long.");
		}
		detail::writeValue(out, slot);
		detail::writeValue(out, static_cast<uint16_t>(path.size()));
		out.write(path.data(), static_cast<std::streamsize>(path.size()));
	}
}

}  // namespace bird::format

#endif	// BIRD_MATERIAL_FORMAT_H
