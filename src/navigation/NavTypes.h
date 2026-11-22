#ifndef BIRD_NAVTYPES_H
#define BIRD_NAVTYPES_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "../util/Constants.h"
#include "../util/Math.h"

namespace bird {

/**
 * Configuration parameters controlling Recast NavMesh voxelization and
 * polygonization.
 *
 * All linear measurements are in world-space metres; angles are in degrees.
 * Defaults are tuned for a human-sized character (0.6 m radius, 2.0 m tall,
 * 0.5 m step height, 45 degree max slope).
 */
struct NavMeshBuildSettings {
	/** Horizontal (XZ) voxel size in metres. Smaller values give finer accuracy at higher bake cost. */
	float cellSize = 0.3f;
	/** Vertical (Y) voxel size in metres. Should be smaller than agentMaxClimb. */
	float cellHeight = 0.2f;

	/** Agent collision radius in metres used to erode walkable areas away from walls. */
	float agentRadius = 0.6f;
	/** Minimum ceiling clearance in metres required for an agent to stand and walk. */
	float agentHeight = 2.0f;
	/** Maximum ledge/stair step height in metres an agent can climb. */
	float agentMaxClimb = 0.5f;
	/** Steepest walkable surface slope in degrees (range (0, 90)). */
	float agentMaxSlope = 45.0f;

	/** Minimum number of cells allowed to form isolated island regions. */
	float regionMinSize = 8.0f;
	/** Any regions with a span count smaller than this are merged into larger neighbors if possible. */
	float regionMergeSize = 20.0f;

	/** Maximum allowed length for contour edges along the border of the mesh, in metres. */
	float edgeMaxLen = 12.0f;
	/** Maximum distance a simplified contour's border edges may deviate from the raw contour, in voxels. */
	float edgeMaxError = 1.3f;
	/** Maximum number of vertices per polygon in the generated NavMesh (3..6). */
	int vertsPerPoly = 6;

	/** Detail mesh sampling distance in metres (0 disables detail height sampling). */
	float detailSampleDist = 6.0f;
	/** Maximum distance the detail mesh surface should deviate from heightfield data, in metres. */
	float detailSampleMaxError = 1.0f;

	/** Marks non-walkable spans as walkable if their maximum is within agentMaxClimb of a walkable neighbor. */
	bool filterLowHangingWalkableObstacles = true;
	/** Marks spans that are ledges (drop-offs steeper than agentMaxClimb) as non-walkable. */
	bool filterLedgeSpans = true;
	/** Marks walkable spans as non-walkable if the clearance above them is less than agentHeight. */
	bool filterWalkableLowHeightSpans = true;
};

/**
 * A point projected onto the surface of a NavMesh.
 *
 * Returned by NavMesh::findNearestPoint() and NavWorld::findNearestPoint().
 */
struct NavPoint {
	/** Closest world-space position lying on a walkable NavMesh polygon. */
	Vector3 position{0.0f};
	/** Opaque Detour polygon reference (0 if invalid). */
	uint32_t polyRef = 0;
};

/**
 * Result of a Detour point-to-point pathfinding query.
 *
 * Contains the funnel-smoothed 3D corner waypoints from start to goal, plus
 * validity and partial-path status flags. In C++, use getPoints() or indexed
 * access; in Dart scripts, iterate from 0 to getPointCount() - 1 with getPoint().
 */
class NavPath {
   public:
	NavPath() = default;

	/**
	 * Constructs a NavPath from a sequence of world-space waypoints.
	 * @param points Ordered 3D corner waypoints from start to end.
	 * @param partial True if the destination was unreachable and the path ends at the closest reachable polygon.
	 */
	BIRD_NO_EXPORT NavPath(std::vector<Vector3> points, bool partial = false)
		: m_points(std::move(points)), m_partial(partial) {}

	/** @return True when the path contains at least one waypoint. */
	bool isValid() const { return !m_points.empty(); }

	/** @return True when the path has no waypoints. */
	bool isEmpty() const { return m_points.empty(); }

	/** @return True when the goal could not be reached and the path terminates at the closest reachable point. */
	bool isPartial() const { return m_partial; }

	/** @return Total number of waypoints along the path. */
	size_t getPointCount() const { return m_points.size(); }

	/**
	 * Returns the world-space waypoint at the given index.
	 * @param index Zero-based waypoint index in [0, getPointCount()).
	 * @return World-space position of the waypoint.
	 * @throws std::out_of_range when index >= getPointCount().
	 */
	Vector3 getPoint(size_t index) const {
		if (index >= m_points.size()) {
			throw std::out_of_range("NavPath::getPoint: index out of range");
		}
		return m_points[index];
	}

	/**
	 * Computes the total Euclidean length of the polyline path in metres.
	 * @return Sum of segment lengths, or 0.0f if fewer than two points exist.
	 */
	float getLength() const {
		float total = 0.0f;
		for (size_t i = 1; i < m_points.size(); ++i) {
			total += glm::length(m_points[i] - m_points[i - 1]);
		}
		return total;
	}

	/** Direct read-only access to the underlying waypoint vector (C++ convenience). */
	BIRD_NO_EXPORT const std::vector<Vector3>& getPoints() const { return m_points; }

   private:
	std::vector<Vector3> m_points;
	bool m_partial = false;
};

/**
 * Configuration for an individual NavAgent moving on a NavMesh via Detour Crowd.
 */
struct NavAgentSettings {
	/** Agent collision radius in metres. Must be positive. */
	float radius = 0.6f;
	/** Agent height in metres. Must be positive. */
	float height = 2.0f;
	/** Maximum movement speed in metres per second. Must be positive. */
	float maxSpeed = 3.5f;
	/** Maximum acceleration in metres per second squared. Must be positive. */
	float maxAcceleration = 8.0f;
	/** Distance threshold in metres at which the agent considers its destination reached. */
	float stoppingDistance = 0.25f;
	/** Weight for crowd agent-to-agent separation steering (higher = stronger spacing). */
	float separationWeight = 2.0f;
	/** When true, smoothly rotates the host WorldObject's yaw to face the movement direction. */
	bool updateRotation = true;
	/** Angular turn speed in radians per second when updateRotation is enabled. */
	float rotationSpeed = 10.0f;
	/** Enables Detour Crowd velocity-obstacle local avoidance between agents. */
	bool obstacleAvoidance = true;
};

/**
 * Settings controlling what NavMesh::debugDraw() and NavWorld::debugDraw() render
 * into the engine's 3D DebugDrawList.
 */
struct NavDebugSettings {
	/** Draws walkable NavMesh polygon edges and outer boundary edges (cached GPU layer). */
	bool drawNavMesh = true;
	/** Draws active planned paths and destination markers for every NavAgent. */
	bool drawAgentPaths = true;
	/** Draws each NavAgent's cylindrical bounds and velocity vector. */
	bool drawAgentBounds = true;
	/** Small vertical lift (metres) applied to NavMesh lines to prevent z-fighting with floor meshes. */
	float heightOffset = 0.05f;
	/** RGBA colour for interior NavMesh polygon edges. */
	Vector4 navMeshColor{0.1f, 0.75f, 0.95f, 0.85f};
	/** RGBA colour for outer/unconnected NavMesh boundary edges. */
	Vector4 navMeshBoundaryColor{0.0f, 0.3f, 0.65f, 1.0f};
	/** RGBA colour for active NavAgent path polylines. */
	Vector4 agentPathColor{1.0f, 0.85f, 0.1f, 1.0f};
	/** RGBA colour for NavAgent cylinder wireframes. */
	Vector4 agentBoundsColor{0.2f, 0.95f, 0.35f, 1.0f};
};

}  // namespace bird

#endif	// !BIRD_NAVTYPES_H
