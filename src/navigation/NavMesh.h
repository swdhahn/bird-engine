#ifndef BIRD_NAVMESH_H
#define BIRD_NAVMESH_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../util/Constants.h"
#include "../util/Math.h"
#include "../util/Memory.hpp"
#include "NavTypes.h"

namespace bird {

class DebugDrawList;
class Mesh;
class Scene;
struct NavMeshImpl;

/**
 * Baked 3D navigation mesh powered by Recast (generation) and Detour (queries).
 *
 * A NavMesh represents walkable surfaces extracted from static level geometry.
 * It can be built at runtime from raw triangle soup, rendered Mesh resources,
 * or an entire Scene subtree, and can be serialized to / loaded from a compact
 * binary file (`.bnav`) so levels do not need to rebake on every load.
 *
 * Once built or loaded, assign the NavMesh to a scene's NavWorld via
 * `scene->getNavWorld()->setNavMesh(navMesh)` to enable NavAgent crowd movement
 * and scene-level path queries, or query this NavMesh directly via `findPath()`
 * and `findNearestPoint()`.
 *
 * All Recast and Detour types are hidden behind an opaque PIMPL (`NavMeshImpl`),
 * keeping public headers free of third-party navigation includes.
 */
class NavMesh {
   public:
	/** Constructs an empty (invalid) NavMesh. Use the static build/load factories to create a populated one. */
	NavMesh();
	~NavMesh();

	NavMesh(const NavMesh&) = delete;
	NavMesh& operator=(const NavMesh&) = delete;

	// ---- Recast Baking ----

	/**
	 * Bakes a NavMesh from raw world-space triangle soup using Recast.
	 *
	 * @param vertices World-space vertex positions.
	 * @param indices Triangle indices (3 per triangle, referencing `vertices`).
	 * @param settings Voxelization and agent clearance parameters.
	 * @return Shared reference to the baked NavMesh.
	 * @throws std::invalid_argument on invalid settings, empty/malformed geometry, or out-of-range indices.
	 * @throws std::runtime_error if Recast fails to produce any walkable polygons.
	 */
	BIRD_NO_EXPORT static _ref<NavMesh> build(const std::vector<Vector3>& vertices,
											  const std::vector<uint32_t>& indices,
											  const NavMeshBuildSettings& settings = {});

	/**
	 * Bakes a NavMesh by merging the CPU-side vertex and index buffers of one or
	 * more rendered Mesh resources (assumed to be in world space).
	 *
	 * @param meshes Collection of shared Mesh resources with CPU-side geometry data.
	 * @param settings Voxelization and agent clearance parameters.
	 * @return Shared reference to the baked NavMesh.
	 * @throws std::invalid_argument if `meshes` is empty or any mesh lacks CPU-side buffers.
	 * @throws std::runtime_error if Recast fails to produce any walkable polygons.
	 */
	static _ref<NavMesh> buildFromMeshes(const std::vector<_ref<Mesh>>& meshes,
										 const NavMeshBuildSettings& settings = {});

	/**
	 * Bakes a NavMesh from all static MeshComponents in a Scene hierarchy.
	 *
	 * Recursively traverses `scene` and all nested sub-scenes. For every
	 * WorldObject that owns a MeshComponent (skipping objects with a NavAgent
	 * component), updates its world transform matrix if dirty and transforms
	 * its mesh vertices into world space before running Recast.
	 *
	 * @param scene Root or sub-scene containing static level geometry.
	 * @param settings Voxelization and agent clearance parameters.
	 * @return Shared reference to the baked NavMesh.
	 * @throws std::invalid_argument if `scene` is null or contains no bakeable mesh geometry.
	 * @throws std::runtime_error if Recast fails to produce any walkable polygons.
	 */
	static _ref<NavMesh> buildFromScene(Scene* scene,
										const NavMeshBuildSettings& settings = {});

	// ---- Binary Serialization & Loading ----

	/**
	 * Serializes this baked NavMesh (including build settings and Detour tile
	 * data) to a compact binary file on disk.
	 *
	 * @param path Destination file path (e.g. `"assets/nav/level01.bnav"`).
	 * @return True on success, false if this NavMesh is invalid or the file cannot be written.
	 */
	bool saveToFile(const std::string& path) const;

	/**
	 * Loads a previously baked NavMesh binary file via the platform VFS
	 * (falling back to the native filesystem).
	 *
	 * @param path File path to the binary NavMesh file.
	 * @return Shared reference to the loaded NavMesh, or nullptr if the file
	 *         could not be read or contained invalid/incompatible data.
	 */
	static _ref<NavMesh> loadFromFile(const std::string& path);

	/**
	 * Serializes this NavMesh into an in-memory byte buffer.
	 *
	 * @return Binary payload suitable for storage or deserialization, or empty
	 *         if this NavMesh is invalid.
	 */
	BIRD_NO_EXPORT std::vector<uint8_t> serialize() const;

	/**
	 * Deserializes a NavMesh from an in-memory byte buffer produced by serialize().
	 *
	 * @param data Pointer to the serialized bytes.
	 * @param size Length of `data` in bytes.
	 * @return Shared reference to the reconstructed NavMesh, or nullptr on
	 *         corrupt/incompatible data.
	 */
	BIRD_NO_EXPORT static _ref<NavMesh> deserialize(const uint8_t* data, size_t size);

	// ---- Detour Pathfinding Queries ----

	/**
	 * Computes a smooth, funnelled 3D path between two world-space positions.
	 *
	 * Projects `start` and `end` onto the nearest walkable polygons within
	 * `searchExtents`, runs Detour A* polygon search, and funnels the polygon
	 * corridor into straight-line corner waypoints.
	 *
	 * @param start World-space starting position.
	 * @param end World-space goal position.
	 * @param searchExtents Half-extents (X, Y, Z in metres) of the search box
	 *                      used to snap `start` and `end` onto the NavMesh.
	 * @return A NavPath containing the waypoints (check `isValid()` and `isPartial()`).
	 */
	NavPath findPath(const Vector3& start, const Vector3& end,
					 const Vector3& searchExtents = Vector3(2.0f, 4.0f, 2.0f)) const;

	/**
	 * Projects a world-space position onto the nearest point on the NavMesh surface.
	 *
	 * @param position World-space query position.
	 * @param searchExtents Half-extents (X, Y, Z in metres) of the search box.
	 * @return Nearest NavPoint on the NavMesh, or std::nullopt if no polygon lies
	 *         within `searchExtents` or this NavMesh is invalid.
	 */
	std::optional<NavPoint> findNearestPoint(
		const Vector3& position,
		const Vector3& searchExtents = Vector3(2.0f, 4.0f, 2.0f)) const;

	// ---- Introspection & Debug Rendering ----

	/** @return True when this NavMesh holds valid Detour navigation data. */
	bool isValid() const;

	/** @return Number of populated tiles in the Detour NavMesh. */
	int getTileCount() const;

	/** @return Total number of walkable polygons across all tiles. */
	int getPolyCount() const;

	/** @return The build settings used to generate (or loaded alongside) this NavMesh. */
	const NavMeshBuildSettings& getBuildSettings() const;

	/** @return Monotonic version identifier used for cached debug wireframe layers. */
	uint64_t getVersion() const;

	/**
	 * Draws this NavMesh's polygon edges into a 3D DebugDrawList using a cached
	 * layer so static wireframes are uploaded to the GPU only when the NavMesh
	 * or debug settings change.
	 *
	 * @param out Target DebugDrawList (typically GraphicsPipeline::getDebugDraw()).
	 * @param settings Visualisation options (colours and height offset).
	 */
	void debugDraw(DebugDrawList& out, const NavDebugSettings& settings = {}) const;

	/** Engine-internal access to the underlying Detour state. */
	BIRD_NO_EXPORT NavMeshImpl& _impl();
	BIRD_NO_EXPORT const NavMeshImpl& _impl() const;

   private:
	std::unique_ptr<NavMeshImpl> m_impl;
};

}  // namespace bird

#endif	// !BIRD_NAVMESH_H
