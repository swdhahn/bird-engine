#ifndef BIRD_SHADOW_ATLAS_H
#define BIRD_SHADOW_ATLAS_H

#include <cstdint>
#include <optional>
#include <vector>

#include "../../util/Math.h"

namespace bird {

/** Square region of a shadow atlas, in texels. */
struct AtlasTile {
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t size = 0;

	/**
	 * @param atlasSize Atlas edge length in texels.
	 * @return xy = UV offset, zw = UV scale.
	 */
	Vector4 uvRect(uint32_t atlasSize) const {
		const float inv = 1.0f / static_cast<float>(atlasSize);
		return Vector4(x * inv, y * inv, size * inv, size * inv);
	}
};

/**
 * Power-of-two quadtree allocator for a square shadow atlas.
 *
 * Requests round up to a power of two between the minimum tile and the
 * atlas size. A free node larger than the request splits into quadrants;
 * every operation is O(log(atlasSize / minTile)). The atlas is rebuilt
 * every frame via reset(), so freed tiles are not coalesced.
 */
class ShadowAtlas {
   public:
	/**
	 * @param atlasSize Edge length; a power of two.
	 * @param minTileSize Smallest tile; a power of two <= atlasSize.
	 * @throws std::invalid_argument when either size is invalid.
	 */
	ShadowAtlas(uint32_t atlasSize, uint32_t minTileSize);

	/** Returns every tile to the free pool. */
	void reset();

	/**
	 * Allocates one tile of exactly the rounded size.
	 * @return The tile, or std::nullopt when no space remains.
	 */
	std::optional<AtlasTile> allocate(uint32_t requestedSize);

	/**
	 * Allocates count equally sized tiles, halving the size until all fit or
	 * the minimum is exceeded. Either every tile is granted or none.
	 * @return The tiles, or an empty vector.
	 */
	std::vector<AtlasTile> allocateGroup(uint32_t requestedSize,
										 uint32_t count);

	/** Returns a tile to the free pool without coalescing. */
	void release(const AtlasTile& tile);

	uint32_t getAtlasSize() const { return m_atlasSize; }
	uint32_t getMinTileSize() const { return m_minTileSize; }

	/** Rounds to the power-of-two tile size an allocation would use. */
	uint32_t roundTileSize(uint32_t requestedSize) const;

   private:
	uint32_t levelOf(uint32_t tileSize) const;

	uint32_t m_atlasSize;
	uint32_t m_minTileSize;
	/** Free tiles per level; level 0 holds the whole atlas. */
	std::vector<std::vector<AtlasTile>> m_freeTiles;
};

}  // namespace bird

#endif	// BIRD_SHADOW_ATLAS_H
