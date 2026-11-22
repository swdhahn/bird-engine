#ifndef BIRD_NAVINTERNAL_H
#define BIRD_NAVINTERNAL_H

#include <DetourCommon.h>
#include <DetourCrowd.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <Recast.h>

#include <cstdint>
#include <vector>

#include "../util/Math.h"
#include "NavTypes.h"

namespace bird {

/** Maximum polygons returned by a single Detour findPath() query. */
constexpr int kNavMaxPathPolys = 256;
/** Maximum nodes in a dtNavMeshQuery search pool. */
constexpr int kNavMaxQueryNodes = 2048;
/** Default maximum agents managed simultaneously by a single dtCrowd. */
constexpr int kNavMaxCrowdAgents = 128;

/** Binary serialization header magic ("BNAV" in little-endian). */
constexpr uint32_t kNavBinaryMagic = 0x56414E42u;
/** Current binary NavMesh serialization format version. */
constexpr uint32_t kNavBinaryVersion = 1u;

/** Poly area and flag constants used during Recast baking and Detour filtering. */
constexpr unsigned char kNavAreaWalkable = RC_WALKABLE_AREA;
constexpr unsigned short kNavPolyFlagWalk = 0x01;

inline void toRecast(const Vector3& v, float out[3]) {
	out[0] = v.x;
	out[1] = v.y;
	out[2] = v.z;
}

inline Vector3 fromRecast(const float v[3]) {
	return Vector3(v[0], v[1], v[2]);
}

/** Private implementation of NavMesh owning Detour navmesh and query handles. */
struct NavMeshImpl {
	dtNavMesh* navMesh = nullptr;
	dtNavMeshQuery* navQuery = nullptr;
	NavMeshBuildSettings buildSettings{};
	uint64_t version = 0;

	~NavMeshImpl() {
		if (navQuery) {
			dtFreeNavMeshQuery(navQuery);
			navQuery = nullptr;
		}
		if (navMesh) {
			dtFreeNavMesh(navMesh);
			navMesh = nullptr;
		}
	}
};

/** Private implementation of NavWorld owning Detour Crowd state. */
struct NavWorldImpl {
	dtCrowd* crowd = nullptr;
	int maxAgents = kNavMaxCrowdAgents;
	float maxAgentRadius = 2.0f;

	~NavWorldImpl() {
		if (crowd) {
			dtFreeCrowd(crowd);
			crowd = nullptr;
		}
	}
};

}  // namespace bird

#endif	// !BIRD_NAVINTERNAL_H
