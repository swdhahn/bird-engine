#ifndef BIRD_DEBUGDRAW_H
#define BIRD_DEBUGDRAW_H

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../util/Math.h"

namespace bird {

/** One endpoint of a debug line. */
struct DebugVertex {
	Vector3 position;
	Vector4 color;
};

/**
 * Lines that rarely change (static level geometry), kept across frames and
 * uploaded to the GPU only when `version` changes.
 */
struct DebugDrawLayer {
	std::vector<DebugVertex> vertices;
	/** Producer-defined; 0 means "never built". See DebugDrawList::newVersion. */
	uint64_t version = 0;
	/** Requested this frame (only used layers are drawn). */
	bool used = false;
};

/**
 * World-space debug lines, rendered by the graphics pipeline after the 3D
 * scene (depth-tested unless drawOnTop is set).
 *
 * - Transient lines (line(), box()) live for one frame; the Application
 *   clears them every frame.
 * - Cached layers hold lines that rarely change. A producer asks for its
 *   layer each frame with cachedLayer(key) and rebuilds the vertices only
 *   when its own version moved on, so large static wireframes cost one
 *   upload instead of one per frame. A layer not requested for a whole
 *   frame is dropped.
 */
class DebugDrawList {
   public:
	void line(const Vector3& from, const Vector3& to, const Vector4& color) {
		m_vertices.push_back({from, color});
		m_vertices.push_back({to, color});
	}

	/**
	 * Transient world-space line always drawn on top of scene geometry
	 * (depth test disabled), used for editor gizmos and x-ray handles.
	 * @param from World-space start point.
	 * @param to World-space end point.
	 * @param color RGBA line color.
	 */
	void overlayLine(const Vector3& from, const Vector3& to,
					 const Vector4& color) {
		m_overlayVertices.push_back({from, color});
		m_overlayVertices.push_back({to, color});
	}

	/** Twelve edges of an axis-aligned box. */
	void box(const Vector3& min, const Vector3& max, const Vector4& color) {
		const Vector3 c[8] = {{min.x, min.y, min.z}, {max.x, min.y, min.z},
							  {max.x, max.y, min.z}, {min.x, max.y, min.z},
							  {min.x, min.y, max.z}, {max.x, min.y, max.z},
							  {max.x, max.y, max.z}, {min.x, max.y, max.z}};
		for (int i = 0; i < 4; i++) {
			line(c[i], c[(i + 1) % 4], color);
			line(c[i + 4], c[(i + 1) % 4 + 4], color);
			line(c[i], c[i + 4], color);
		}
	}

	/** The cached layer for a producer (created empty), marked used. */
	DebugDrawLayer& cachedLayer(const void* key) {
		DebugDrawLayer& layer = m_layers[key];
		layer.used = true;
		return layer;
	}
	const std::unordered_map<const void*, DebugDrawLayer>& cachedLayers() const {
		return m_layers;
	}

	/**
	 * A process-wide unique, never-zero version number for cached layers,
	 * so a new producer reusing an old key never matches stale lines.
	 */
	static uint64_t newVersion() {
		static std::atomic<uint64_t> counter{0};
		return ++counter;
	}

	/** Starts a frame: drops transient lines and layers unused last frame. */
	void clear() {
		m_vertices.clear();
		m_overlayVertices.clear();
		for (auto it = m_layers.begin(); it != m_layers.end();) {
			if (!it->second.used) {
				it = m_layers.erase(it);
			} else {
				it->second.used = false;
				++it;
			}
		}
	}

	/** True when nothing would be drawn this frame. */
	bool empty() const {
		if (!m_vertices.empty() || !m_overlayVertices.empty()) return false;
		for (const auto& [key, layer] : m_layers) {
			if (layer.used && !layer.vertices.empty()) return false;
		}
		return true;
	}
	/** Transient vertices, two per line. */
	const std::vector<DebugVertex>& vertices() const { return m_vertices; }
	/** Always-on-top transient overlay vertices, two per line. */
	const std::vector<DebugVertex>& overlayVertices() const {
		return m_overlayVertices;
	}
	/** Transient lines plus overlay lines plus lines in used cached layers. */
	size_t lineCount() const {
		size_t count = m_vertices.size() + m_overlayVertices.size();
		for (const auto& [key, layer] : m_layers) {
			if (layer.used) count += layer.vertices.size();
		}
		return count / 2;
	}

	/** Draw over the scene, ignoring depth (x-ray). */
	bool drawOnTop = false;

   private:
	std::vector<DebugVertex> m_vertices;
	std::vector<DebugVertex> m_overlayVertices;
	std::unordered_map<const void*, DebugDrawLayer> m_layers;
};

}  // namespace bird

#endif	// !BIRD_DEBUGDRAW_H
