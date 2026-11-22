#ifndef BIRD_ANIMATION_SKELETALASSET_H
#define BIRD_ANIMATION_SKELETALASSET_H

#include <array>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "../../util/Constants.h"
#include "SkeletalClip.h"
#include "Skeleton.h"

namespace bird {

/**
 * Per-vertex skinning influences for one mesh of the model: the four
 * strongest joints with weights normalized to sum to one. Stored beside
 * the skeleton so the existing mesh format stays unchanged.
 */
struct SkinStream {
	/** Index of the mesh inside the model's baked mesh file. */
	uint32_t meshIndex = 0;
	std::vector<std::array<uint16_t, 4>> joints;
	std::vector<std::array<float, 4>> weights;
};

/**
 * A model's skeleton plus every clip and skin authored for it: the unit
 * that the asset baker writes (`<model>.skel.bird`) and the runtime loads.
 */
struct SkeletalAsset {
	std::shared_ptr<Skeleton> skeleton;
	std::vector<std::shared_ptr<SkeletalClip>> clips;
	std::vector<SkinStream> skins;

	/** @return the clip with this name, or nullptr. O(clips). */
	std::shared_ptr<SkeletalClip> findClip(const std::string& name) const;
};

/*
 * Binary layout (version 1, little-endian, tightly packed):
 *
 *   char[4] "BSKL", uint32 version, uint32 jointCount, uint32 clipCount
 *   joint  x jointCount: string name, int32 parent, float[16] inverseBind,
 *                        float[3] T, float[4] R (x, y, z, w), float[3] S
 *   clip   x clipCount:  string name, float duration, uint32 trackCount,
 *     track x trackCount: int32 joint, then channels T, R, S each as
 *       uint8 interpolation, uint32 keyCount,
 *       key x keyCount: float time, value, inTangent, outTangent
 *   uint32 skinCount
 *   skin   x skinCount:  uint32 meshIndex, uint32 vertexCount,
 *                        uint16[4] joints x vertexCount,
 *                        float[4] weights x vertexCount
 *   string = uint16 byteLength + UTF-8 bytes
 */
constexpr uint32_t SKELETAL_ASSET_VERSION = 1;

/**
 * Serializes an asset.
 * @throws std::invalid_argument when the skeleton is null.
 * @throws std::runtime_error when the stream fails or a name is too long.
 */
BIRD_NO_EXPORT void writeSkeletalAsset(std::ostream& out,
									   const SkeletalAsset& asset);

/**
 * Deserializes and validates an asset; every count and index is checked
 * before use, so a truncated or corrupt file fails fast instead of
 * allocating unbounded memory or indexing out of range.
 * @throws std::runtime_error on a malformed or unsupported file.
 */
BIRD_NO_EXPORT SkeletalAsset readSkeletalAsset(std::istream& in);

/** File-path conveniences around the stream functions. */
BIRD_NO_EXPORT void saveSkeletalAsset(const std::string& path,
									  const SkeletalAsset& asset);
/** @throws std::runtime_error when the file is missing or malformed. */
BIRD_NO_EXPORT SkeletalAsset loadSkeletalAsset(const std::string& path);

}  // namespace bird

#endif	// BIRD_ANIMATION_SKELETALASSET_H
