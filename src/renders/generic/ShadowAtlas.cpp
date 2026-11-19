#include "ShadowAtlas.h"

#include <algorithm>
#include <stdexcept>

namespace bird {

namespace {

bool isPowerOfTwo(uint32_t value) {
	return value != 0 && (value & (value - 1)) == 0;
}

uint32_t nextPowerOfTwo(uint32_t value) {
	uint32_t result = 1;
	while (result < value && result < (1u << 31)) result <<= 1;
	return result;
}

}  // namespace

ShadowAtlas::ShadowAtlas(uint32_t atlasSize, uint32_t minTileSize)
	: m_atlasSize(atlasSize), m_minTileSize(minTileSize) {
	if (!isPowerOfTwo(atlasSize) || !isPowerOfTwo(minTileSize) ||
		minTileSize > atlasSize) {
		throw std::invalid_argument(
			"ShadowAtlas: sizes must be powers of two with min <= atlas.");
	}
	m_freeTiles.resize(levelOf(minTileSize) + 1);
	reset();
}

uint32_t ShadowAtlas::levelOf(uint32_t tileSize) const {
	uint32_t level = 0;
	for (uint32_t size = m_atlasSize; size > tileSize; size >>= 1) level++;
	return level;
}

uint32_t ShadowAtlas::roundTileSize(uint32_t requestedSize) const {
	return std::clamp(nextPowerOfTwo(requestedSize), m_minTileSize,
					  m_atlasSize);
}

void ShadowAtlas::reset() {
	for (auto& level : m_freeTiles) level.clear();
	m_freeTiles[0].push_back(AtlasTile{0, 0, m_atlasSize});
}

std::optional<AtlasTile> ShadowAtlas::allocate(uint32_t requestedSize) {
	const uint32_t size = roundTileSize(requestedSize);
	const uint32_t targetLevel = levelOf(size);

	// Find the smallest free tile at least as large as the request.
	int32_t level = static_cast<int32_t>(targetLevel);
	while (level >= 0 && m_freeTiles[level].empty()) level--;
	if (level < 0) return std::nullopt;

	// Split downward; the first quadrant continues, the rest become free.
	while (static_cast<uint32_t>(level) < targetLevel) {
		const AtlasTile parent = m_freeTiles[level].back();
		m_freeTiles[level].pop_back();
		const uint32_t half = parent.size / 2;
		auto& children = m_freeTiles[level + 1];
		// Pushed in reverse so that back() yields the top-left quadrant,
		// keeping allocations compact and deterministic.
		children.push_back({parent.x + half, parent.y + half, half});
		children.push_back({parent.x, parent.y + half, half});
		children.push_back({parent.x + half, parent.y, half});
		children.push_back({parent.x, parent.y, half});
		level++;
	}
	const AtlasTile tile = m_freeTiles[targetLevel].back();
	m_freeTiles[targetLevel].pop_back();
	return tile;
}

std::vector<AtlasTile> ShadowAtlas::allocateGroup(uint32_t requestedSize,
												  uint32_t count) {
	for (uint32_t size = roundTileSize(requestedSize); size >= m_minTileSize;
		 size >>= 1) {
		std::vector<AtlasTile> tiles;
		tiles.reserve(count);
		for (uint32_t i = 0; i < count; i++) {
			auto tile = allocate(size);
			if (!tile) break;
			tiles.push_back(*tile);
		}
		if (tiles.size() == count) return tiles;
		for (const AtlasTile& tile : tiles) release(tile);
	}
	return {};
}

void ShadowAtlas::release(const AtlasTile& tile) {
	m_freeTiles[levelOf(tile.size)].push_back(tile);
}

}  // namespace bird
