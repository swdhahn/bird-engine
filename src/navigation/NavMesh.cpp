#include "NavMesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../platform/Log.h"
#include "../platform/VFS.h"
#include "../renders/DebugDraw.h"
#include "../renders/generic/Mesh.h"
#include "../scene/Scene.h"
#include "../scene/WorldObject.h"
#include "../scene/components/MeshComponent.h"
#include "NavAgent.h"
#include "NavInternal.h"

namespace bird {

namespace {

void validateBuildSettings(const NavMeshBuildSettings& s) {
	if (!(s.cellSize > 0.0f) || !(s.cellHeight > 0.0f)) {
		throw std::invalid_argument(
			"NavMesh::build: cellSize and cellHeight must be positive");
	}
	if (s.agentRadius < 0.0f) {
		throw std::invalid_argument("NavMesh::build: agentRadius must be non-negative");
	}
	if (!(s.agentHeight > 0.0f)) {
		throw std::invalid_argument("NavMesh::build: agentHeight must be positive");
	}
	if (s.agentMaxClimb < 0.0f) {
		throw std::invalid_argument("NavMesh::build: agentMaxClimb must be non-negative");
	}
	if (!(s.agentMaxSlope > 0.0f && s.agentMaxSlope < 90.0f)) {
		throw std::invalid_argument(
			"NavMesh::build: agentMaxSlope must be in (0, 90) degrees");
	}
	if (s.vertsPerPoly < 3 || s.vertsPerPoly > DT_VERTS_PER_POLYGON) {
		throw std::invalid_argument(
			"NavMesh::build: vertsPerPoly must be in [3, 6]");
	}
}

void appendMeshTransformed(const std::shared_ptr<Mesh>& mesh, const Matrix4& transform,
						   std::vector<Vector3>& outVerts,
						   std::vector<uint32_t>& outIndices, const char* what) {
	if (!mesh) {
		throw std::invalid_argument(std::string("NavMesh::") + what + ": null mesh");
	}
	Buffer<float>* vb = mesh->getVertexBuffer();
	if (vb == nullptr || vb->m_pData == nullptr) {
		throw std::invalid_argument(std::string("NavMesh::") + what +
									": mesh has no CPU-side vertex data");
	}
	Buffer<uint32_t>* ib = mesh->getIndexBuffer();
	if (ib == nullptr || ib->m_pData == nullptr) {
		throw std::invalid_argument(std::string("NavMesh::") + what +
									": mesh has no CPU-side index data");
	}

	const uint32_t baseVertex = static_cast<uint32_t>(outVerts.size());
	const size_t floatCount = vb->getBufferSize();
	const float* vdata = vb->m_pData.get();
	for (size_t i = 0; i + 2 < floatCount; i += 3) {
		const Vector4 worldPos =
			transform * Vector4(vdata[i], vdata[i + 1], vdata[i + 2], 1.0f);
		outVerts.emplace_back(worldPos.x, worldPos.y, worldPos.z);
	}

	const uint32_t addedVerts = static_cast<uint32_t>(outVerts.size()) - baseVertex;
	const size_t idxCount = ib->getBufferSize();
	const uint32_t* idata = ib->m_pData.get();
	for (size_t i = 0; i + 2 < idxCount; i += 3) {
		if (idata[i] >= addedVerts || idata[i + 1] >= addedVerts ||
			idata[i + 2] >= addedVerts) {
			throw std::invalid_argument(std::string("NavMesh::") + what +
										": mesh index out of range");
		}
		outIndices.push_back(baseVertex + idata[i]);
		outIndices.push_back(baseVertex + idata[i + 1]);
		outIndices.push_back(baseVertex + idata[i + 2]);
	}
}

bool hasNavAgentComponent(WorldObject* obj) {
	for (const auto& comp : obj->getComponents()) {
		if (dynamic_cast<NavAgent*>(comp.get()) != nullptr) {
			return true;
		}
	}
	return false;
}

void collectSceneGeometry(Scene* scene, std::vector<Vector3>& outVerts,
						  std::vector<uint32_t>& outIndices) {
	for (auto& child : scene->getChildren()) {
		WorldObject* obj = child.get();
		if (obj == nullptr) continue;

		if (obj->getType() == SCENE) {
			collectSceneGeometry(static_cast<Scene*>(obj), outVerts, outIndices);
			continue;
		}

		// Skip dynamic navigation agents so they are not baked as static obstacles.
		if (hasNavAgentComponent(obj)) continue;

		for (auto& comp : obj->getComponents()) {
			auto* meshComp = dynamic_cast<MeshComponent*>(comp.get());
			if (meshComp == nullptr) continue;

			if (obj->needsMatrixUpdate()) {
				obj->updateTransformationMatrix();
			}
			const Matrix4& worldMat = obj->getTransformMatrix();
			for (const auto& mesh : meshComp->getMeshes()) {
				if (mesh && mesh->getVertexBuffer() && mesh->getVertexBuffer()->m_pData &&
					mesh->getIndexBuffer() && mesh->getIndexBuffer()->m_pData) {
					appendMeshTransformed(mesh, worldMat, outVerts, outIndices,
										  "buildFromScene");
				}
			}
		}
	}
}

/** Simple byte writer for binary serialization. */
class ByteWriter {
   public:
	template <typename T>
	void write(const T& value) {
		const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
		m_buf.insert(m_buf.end(), bytes, bytes + sizeof(T));
	}

	void writeBytes(const uint8_t* data, size_t count) {
		m_buf.insert(m_buf.end(), data, data + count);
	}

	std::vector<uint8_t> take() { return std::move(m_buf); }

   private:
	std::vector<uint8_t> m_buf;
};

/** Bounds-checked byte reader for binary deserialization. */
class ByteReader {
   public:
	ByteReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

	template <typename T>
	bool read(T& out) {
		if (m_offset + sizeof(T) > m_size) return false;
		std::memcpy(&out, m_data + m_offset, sizeof(T));
		m_offset += sizeof(T);
		return true;
	}

	bool readBytes(uint8_t* dst, size_t count) {
		if (m_offset + count > m_size) return false;
		std::memcpy(dst, m_data + m_offset, count);
		m_offset += count;
		return true;
	}

   private:
	const uint8_t* m_data;
	size_t m_size;
	size_t m_offset = 0;
};

/** Mixes a 64-bit value into a running FNV-1a style hash for debug cache versioning. */
void hashCombine(uint64_t& seed, uint64_t value) {
	seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

uint64_t floatBits(float f) {
	uint32_t bits = 0;
	std::memcpy(&bits, &f, sizeof(bits));
	return static_cast<uint64_t>(bits);
}

uint64_t debugLayerVersion(uint64_t navVersion, const NavDebugSettings& s) {
	uint64_t h = navVersion;
	hashCombine(h, floatBits(s.heightOffset));
	hashCombine(h, floatBits(s.navMeshColor.x));
	hashCombine(h, floatBits(s.navMeshColor.y));
	hashCombine(h, floatBits(s.navMeshColor.z));
	hashCombine(h, floatBits(s.navMeshColor.w));
	hashCombine(h, floatBits(s.navMeshBoundaryColor.x));
	hashCombine(h, floatBits(s.navMeshBoundaryColor.y));
	hashCombine(h, floatBits(s.navMeshBoundaryColor.z));
	hashCombine(h, floatBits(s.navMeshBoundaryColor.w));
	return h == 0 ? 1 : h;
}

}  // namespace

NavMesh::NavMesh() : m_impl(std::make_unique<NavMeshImpl>()) {}

NavMesh::~NavMesh() = default;

_ref<NavMesh> NavMesh::build(const std::vector<Vector3>& vertices,
							 const std::vector<uint32_t>& indices,
							 const NavMeshBuildSettings& settings) {
	validateBuildSettings(settings);

	if (vertices.empty() || indices.empty()) {
		throw std::invalid_argument("NavMesh::build: input geometry is empty");
	}
	if (indices.size() % 3 != 0) {
		throw std::invalid_argument(
			"NavMesh::build: index count must be a multiple of 3");
	}

	const int nverts = static_cast<int>(vertices.size());
	const int ntris = static_cast<int>(indices.size() / 3);

	std::vector<float> flatVerts(vertices.size() * 3);
	for (size_t i = 0; i < vertices.size(); ++i) {
		flatVerts[i * 3 + 0] = vertices[i].x;
		flatVerts[i * 3 + 1] = vertices[i].y;
		flatVerts[i * 3 + 2] = vertices[i].z;
	}

	std::vector<int> flatTris(indices.size());
	for (size_t i = 0; i < indices.size(); ++i) {
		if (indices[i] >= vertices.size()) {
			throw std::invalid_argument("NavMesh::build: triangle index out of range");
		}
		flatTris[i] = static_cast<int>(indices[i]);
	}

	// Step 1: Configure Recast parameters.
	rcConfig cfg{};
	cfg.cs = settings.cellSize;
	cfg.ch = settings.cellHeight;
	cfg.walkableSlopeAngle = settings.agentMaxSlope;
	cfg.walkableHeight = static_cast<int>(std::ceil(settings.agentHeight / cfg.ch));
	cfg.walkableClimb = static_cast<int>(std::floor(settings.agentMaxClimb / cfg.ch));
	cfg.walkableRadius = static_cast<int>(std::ceil(settings.agentRadius / cfg.cs));
	cfg.maxEdgeLen = static_cast<int>(settings.edgeMaxLen / cfg.cs);
	cfg.maxSimplificationError = settings.edgeMaxError;
	cfg.minRegionArea =
		static_cast<int>(rcSqr(settings.regionMinSize));
	cfg.mergeRegionArea =
		static_cast<int>(rcSqr(settings.regionMergeSize));
	cfg.maxVertsPerPoly = settings.vertsPerPoly;
	cfg.detailSampleDist =
		settings.detailSampleDist < 0.9f ? 0.0f : cfg.cs * settings.detailSampleDist;
	cfg.detailSampleMaxError = cfg.ch * settings.detailSampleMaxError;

	rcCalcBounds(flatVerts.data(), nverts, cfg.bmin, cfg.bmax);
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

	rcContext ctx(false);

	// RAII wrappers for temporary Recast allocations.
	struct RcHeightfieldGuard {
		rcHeightfield* p = nullptr;
		~RcHeightfieldGuard() { rcFreeHeightField(p); }
	} solid{rcAllocHeightfield()};

	if (!solid.p || !rcCreateHeightfield(&ctx, *solid.p, cfg.width, cfg.height,
										 cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) {
		throw std::runtime_error("NavMesh::build: failed to create Recast heightfield");
	}

	// Step 2: Rasterize input polygon soup.
	std::vector<unsigned char> triAreas(static_cast<size_t>(ntris), 0);
	rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, flatVerts.data(), nverts,
							flatTris.data(), ntris, triAreas.data());
	if (!rcRasterizeTriangles(&ctx, flatVerts.data(), nverts, flatTris.data(),
							  triAreas.data(), ntris, *solid.p, cfg.walkableClimb)) {
		throw std::runtime_error("NavMesh::build: failed to rasterize triangles");
	}

	// Step 3: Filter walkable surfaces.
	if (settings.filterLowHangingWalkableObstacles) {
		rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid.p);
	}
	if (settings.filterLedgeSpans) {
		rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid.p);
	}
	if (settings.filterWalkableLowHeightSpans) {
		rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid.p);
	}

	// Step 4: Compact heightfield and erode walkable area by agent radius.
	struct RcCompactGuard {
		rcCompactHeightfield* p = nullptr;
		~RcCompactGuard() { rcFreeCompactHeightfield(p); }
	} chf{rcAllocCompactHeightfield()};

	if (!chf.p || !rcBuildCompactHeightfield(&ctx, cfg.walkableHeight,
											 cfg.walkableClimb, *solid.p, *chf.p)) {
		throw std::runtime_error(
			"NavMesh::build: failed to build compact heightfield");
	}
	if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf.p)) {
		throw std::runtime_error("NavMesh::build: failed to erode walkable area");
	}

	// Step 5: Build distance field and watershed regions.
	if (!rcBuildDistanceField(&ctx, *chf.p)) {
		throw std::runtime_error("NavMesh::build: failed to build distance field");
	}
	if (!rcBuildRegions(&ctx, *chf.p, 0, cfg.minRegionArea, cfg.mergeRegionArea)) {
		throw std::runtime_error("NavMesh::build: failed to build watershed regions");
	}

	// Step 6: Trace and simplify contours.
	struct RcContourGuard {
		rcContourSet* p = nullptr;
		~RcContourGuard() { rcFreeContourSet(p); }
	} cset{rcAllocContourSet()};

	if (!cset.p || !rcBuildContours(&ctx, *chf.p, cfg.maxSimplificationError,
									cfg.maxEdgeLen, *cset.p)) {
		throw std::runtime_error("NavMesh::build: failed to build contours");
	}

	// Step 7: Build polygon mesh and detail mesh.
	struct RcPolyMeshGuard {
		rcPolyMesh* p = nullptr;
		~RcPolyMeshGuard() { rcFreePolyMesh(p); }
	} pmesh{rcAllocPolyMesh()};

	if (!pmesh.p || !rcBuildPolyMesh(&ctx, *cset.p, cfg.maxVertsPerPoly, *pmesh.p)) {
		throw std::runtime_error("NavMesh::build: failed to build polygon mesh");
	}
	if (pmesh.p->npolys == 0) {
		throw std::runtime_error(
			"NavMesh::build: no walkable polygons generated (check geometry size and build settings)");
	}

	// Mark every walkable polygon with the walk flag so Detour filters include it.
	for (int i = 0; i < pmesh.p->npolys; ++i) {
		if (pmesh.p->areas[i] == RC_WALKABLE_AREA) {
			pmesh.p->flags[i] = kNavPolyFlagWalk;
		}
	}

	struct RcDetailGuard {
		rcPolyMeshDetail* p = nullptr;
		~RcDetailGuard() { rcFreePolyMeshDetail(p); }
	} dmesh{rcAllocPolyMeshDetail()};

	if (!dmesh.p ||
		!rcBuildPolyMeshDetail(&ctx, *pmesh.p, *chf.p, cfg.detailSampleDist,
							   cfg.detailSampleMaxError, *dmesh.p)) {
		throw std::runtime_error("NavMesh::build: failed to build detail mesh");
	}

	// Step 8: Package into Detour NavMesh data.
	dtNavMeshCreateParams params{};
	params.verts = pmesh.p->verts;
	params.vertCount = pmesh.p->nverts;
	params.polys = pmesh.p->polys;
	params.polyAreas = pmesh.p->areas;
	params.polyFlags = pmesh.p->flags;
	params.polyCount = pmesh.p->npolys;
	params.nvp = pmesh.p->nvp;
	params.detailMeshes = dmesh.p->meshes;
	params.detailVerts = dmesh.p->verts;
	params.detailVertsCount = dmesh.p->nverts;
	params.detailTris = dmesh.p->tris;
	params.detailTriCount = dmesh.p->ntris;
	params.walkableHeight = settings.agentHeight;
	params.walkableRadius = settings.agentRadius;
	params.walkableClimb = settings.agentMaxClimb;
	rcVcopy(params.bmin, pmesh.p->bmin);
	rcVcopy(params.bmax, pmesh.p->bmax);
	params.cs = cfg.cs;
	params.ch = cfg.ch;
	params.buildBvTree = true;

	unsigned char* navData = nullptr;
	int navDataSize = 0;
	if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
		throw std::runtime_error(
			"NavMesh::build: failed to create Detour navmesh data");
	}

	auto result = _ref_mk<NavMesh>();
	result->m_impl->navMesh = dtAllocNavMesh();
	if (!result->m_impl->navMesh) {
		dtFree(navData);
		throw std::runtime_error("NavMesh::build: failed to allocate dtNavMesh");
	}

	dtStatus status =
		result->m_impl->navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
	if (dtStatusFailed(status)) {
		dtFree(navData);
		throw std::runtime_error("NavMesh::build: failed to initialize dtNavMesh");
	}

	result->m_impl->navQuery = dtAllocNavMeshQuery();
	if (!result->m_impl->navQuery ||
		dtStatusFailed(
			result->m_impl->navQuery->init(result->m_impl->navMesh, kNavMaxQueryNodes))) {
		throw std::runtime_error(
			"NavMesh::build: failed to initialize dtNavMeshQuery");
	}

	result->m_impl->buildSettings = settings;
	result->m_impl->version = DebugDrawList::newVersion();
	return result;
}

_ref<NavMesh> NavMesh::buildFromMeshes(const std::vector<_ref<Mesh>>& meshes,
									   const NavMeshBuildSettings& settings) {
	if (meshes.empty()) {
		throw std::invalid_argument("NavMesh::buildFromMeshes: meshes list is empty");
	}
	std::vector<Vector3> verts;
	std::vector<uint32_t> indices;
	const Matrix4 identity(1.0f);
	for (const auto& mesh : meshes) {
		appendMeshTransformed(mesh, identity, verts, indices, "buildFromMeshes");
	}
	return build(verts, indices, settings);
}

_ref<NavMesh> NavMesh::buildFromScene(Scene* scene,
									  const NavMeshBuildSettings& settings) {
	if (scene == nullptr) {
		throw std::invalid_argument("NavMesh::buildFromScene: scene is null");
	}
	std::vector<Vector3> verts;
	std::vector<uint32_t> indices;
	collectSceneGeometry(scene, verts, indices);
	if (verts.empty() || indices.empty()) {
		throw std::invalid_argument(
			"NavMesh::buildFromScene: scene contains no bakeable MeshComponent geometry");
	}
	return build(verts, indices, settings);
}

std::vector<uint8_t> NavMesh::serialize() const {
	if (!isValid()) return {};

	const dtNavMesh* mesh = m_impl->navMesh;
	ByteWriter writer;

	// Header
	writer.write(kNavBinaryMagic);
	writer.write(kNavBinaryVersion);

	// Build settings (written field-by-field for layout independence)
	const NavMeshBuildSettings& s = m_impl->buildSettings;
	writer.write(s.cellSize);
	writer.write(s.cellHeight);
	writer.write(s.agentRadius);
	writer.write(s.agentHeight);
	writer.write(s.agentMaxClimb);
	writer.write(s.agentMaxSlope);
	writer.write(s.regionMinSize);
	writer.write(s.regionMergeSize);
	writer.write(s.edgeMaxLen);
	writer.write(s.edgeMaxError);
	writer.write(static_cast<int32_t>(s.vertsPerPoly));
	writer.write(s.detailSampleDist);
	writer.write(s.detailSampleMaxError);
	writer.write(static_cast<uint8_t>(s.filterLowHangingWalkableObstacles ? 1 : 0));
	writer.write(static_cast<uint8_t>(s.filterLedgeSpans ? 1 : 0));
	writer.write(static_cast<uint8_t>(s.filterWalkableLowHeightSpans ? 1 : 0));

	// Detour navmesh params
	const dtNavMeshParams* params = mesh->getParams();
	writer.write(*params);

	// Count populated tiles
	int32_t numTiles = 0;
	for (int i = 0; i < mesh->getMaxTiles(); ++i) {
		const dtMeshTile* tile = mesh->getTile(i);
		if (tile && tile->header && tile->dataSize > 0) {
			++numTiles;
		}
	}
	writer.write(numTiles);

	// Write each tile
	for (int i = 0; i < mesh->getMaxTiles(); ++i) {
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header || tile->dataSize <= 0) continue;

		const uint32_t tileRef = static_cast<uint32_t>(mesh->getTileRef(tile));
		const int32_t dataSize = static_cast<int32_t>(tile->dataSize);
		writer.write(tileRef);
		writer.write(dataSize);
		writer.writeBytes(tile->data, static_cast<size_t>(tile->dataSize));
	}

	return writer.take();
}

_ref<NavMesh> NavMesh::deserialize(const uint8_t* data, size_t size) {
	if (data == nullptr || size == 0) return nullptr;

	ByteReader reader(data, size);

	uint32_t magic = 0;
	uint32_t version = 0;
	if (!reader.read(magic) || !reader.read(version)) return nullptr;
	if (magic != kNavBinaryMagic || version != kNavBinaryVersion) return nullptr;

	NavMeshBuildSettings s{};
	int32_t vertsPerPoly = 6;
	uint8_t fLowHanging = 1;
	uint8_t fLedge = 1;
	uint8_t fLowHeight = 1;
	if (!reader.read(s.cellSize) || !reader.read(s.cellHeight) ||
		!reader.read(s.agentRadius) || !reader.read(s.agentHeight) ||
		!reader.read(s.agentMaxClimb) || !reader.read(s.agentMaxSlope) ||
		!reader.read(s.regionMinSize) || !reader.read(s.regionMergeSize) ||
		!reader.read(s.edgeMaxLen) || !reader.read(s.edgeMaxError) ||
		!reader.read(vertsPerPoly) || !reader.read(s.detailSampleDist) ||
		!reader.read(s.detailSampleMaxError) || !reader.read(fLowHanging) ||
		!reader.read(fLedge) || !reader.read(fLowHeight)) {
		return nullptr;
	}
	s.vertsPerPoly = static_cast<int>(vertsPerPoly);
	s.filterLowHangingWalkableObstacles = fLowHanging != 0;
	s.filterLedgeSpans = fLedge != 0;
	s.filterWalkableLowHeightSpans = fLowHeight != 0;

	dtNavMeshParams params{};
	int32_t numTiles = 0;
	if (!reader.read(params) || !reader.read(numTiles) || numTiles <= 0) {
		return nullptr;
	}

	auto result = _ref_mk<NavMesh>();
	result->m_impl->navMesh = dtAllocNavMesh();
	if (!result->m_impl->navMesh) return nullptr;

	if (dtStatusFailed(result->m_impl->navMesh->init(&params))) {
		return nullptr;
	}

	for (int32_t i = 0; i < numTiles; ++i) {
		uint32_t tileRef = 0;
		int32_t dataSize = 0;
		if (!reader.read(tileRef) || !reader.read(dataSize) || dataSize <= 0) {
			return nullptr;
		}
		auto* tileData = static_cast<unsigned char*>(
			dtAlloc(static_cast<size_t>(dataSize), DT_ALLOC_PERM));
		if (!tileData) return nullptr;

		if (!reader.readBytes(tileData, static_cast<size_t>(dataSize))) {
			dtFree(tileData);
			return nullptr;
		}
		dtStatus status = result->m_impl->navMesh->addTile(
			tileData, dataSize, DT_TILE_FREE_DATA, static_cast<dtTileRef>(tileRef),
			nullptr);
		if (dtStatusFailed(status)) {
			dtFree(tileData);
			return nullptr;
		}
	}

	result->m_impl->navQuery = dtAllocNavMeshQuery();
	if (!result->m_impl->navQuery ||
		dtStatusFailed(
			result->m_impl->navQuery->init(result->m_impl->navMesh, kNavMaxQueryNodes))) {
		return nullptr;
	}

	result->m_impl->buildSettings = s;
	result->m_impl->version = DebugDrawList::newVersion();
	return result;
}

bool NavMesh::saveToFile(const std::string& path) const {
	std::vector<uint8_t> bytes = serialize();
	if (bytes.empty()) return false;

	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) return false;
	out.write(reinterpret_cast<const char*>(bytes.data()),
			  static_cast<std::streamsize>(bytes.size()));
	return out.good();
}

_ref<NavMesh> NavMesh::loadFromFile(const std::string& path) {
	if (auto vfsBytes = platform::VFS::get().readFile(path)) {
		return deserialize(vfsBytes->data(), vfsBytes->size());
	}
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	if (!in.is_open()) return nullptr;
	const std::streamsize sz = in.tellg();
	if (sz <= 0) return nullptr;
	in.seekg(0, std::ios::beg);
	std::vector<uint8_t> bytes(static_cast<size_t>(sz));
	if (!in.read(reinterpret_cast<char*>(bytes.data()), sz)) return nullptr;
	return deserialize(bytes.data(), bytes.size());
}

NavPath NavMesh::findPath(const Vector3& start, const Vector3& end,
						  const Vector3& searchExtents) const {
	if (!isValid()) return NavPath();

	dtQueryFilter filter;
	filter.setIncludeFlags(kNavPolyFlagWalk);
	filter.setExcludeFlags(0);

	float startPos[3], endPos[3], extents[3];
	toRecast(start, startPos);
	toRecast(end, endPos);
	toRecast(searchExtents, extents);

	dtPolyRef startRef = 0;
	dtPolyRef endRef = 0;
	float nearestStart[3] = {};
	float nearestEnd[3] = {};

	if (dtStatusFailed(m_impl->navQuery->findNearestPoly(
			startPos, extents, &filter, &startRef, nearestStart)) ||
		startRef == 0) {
		return NavPath();
	}
	if (dtStatusFailed(m_impl->navQuery->findNearestPoly(
			endPos, extents, &filter, &endRef, nearestEnd)) ||
		endRef == 0) {
		return NavPath();
	}

	dtPolyRef polyPath[kNavMaxPathPolys];
	int polyCount = 0;
	dtStatus pathStatus = m_impl->navQuery->findPath(
		startRef, endRef, nearestStart, nearestEnd, &filter, polyPath, &polyCount,
		kNavMaxPathPolys);
	if (dtStatusFailed(pathStatus) || polyCount == 0) {
		return NavPath();
	}

	const bool partial =
		dtStatusDetail(pathStatus, DT_PARTIAL_RESULT) || (polyPath[polyCount - 1] != endRef);

	// If the path is partial, clamp the final position to the last reached polygon.
	float actualEnd[3] = {nearestEnd[0], nearestEnd[1], nearestEnd[2]};
	if (partial) {
		m_impl->navQuery->closestPointOnPoly(polyPath[polyCount - 1], nearestEnd,
											 actualEnd, nullptr);
	}

	float straightPath[kNavMaxPathPolys * 3];
	unsigned char straightFlags[kNavMaxPathPolys];
	dtPolyRef straightPolys[kNavMaxPathPolys];
	int straightCount = 0;

	dtStatus straightStatus = m_impl->navQuery->findStraightPath(
		nearestStart, actualEnd, polyPath, polyCount, straightPath, straightFlags,
		straightPolys, &straightCount, kNavMaxPathPolys);
	if (dtStatusFailed(straightStatus) || straightCount == 0) {
		return NavPath();
	}

	std::vector<Vector3> points;
	points.reserve(static_cast<size_t>(straightCount));
	for (int i = 0; i < straightCount; ++i) {
		points.emplace_back(straightPath[i * 3 + 0], straightPath[i * 3 + 1],
							straightPath[i * 3 + 2]);
	}

	return NavPath(std::move(points), partial);
}

std::optional<NavPoint> NavMesh::findNearestPoint(const Vector3& position,
												  const Vector3& searchExtents) const {
	if (!isValid()) return std::nullopt;

	dtQueryFilter filter;
	filter.setIncludeFlags(kNavPolyFlagWalk);
	filter.setExcludeFlags(0);

	float pos[3], extents[3], nearest[3];
	toRecast(position, pos);
	toRecast(searchExtents, extents);

	dtPolyRef ref = 0;
	if (dtStatusFailed(
			m_impl->navQuery->findNearestPoly(pos, extents, &filter, &ref, nearest)) ||
		ref == 0) {
		return std::nullopt;
	}

	NavPoint pt;
	pt.position = fromRecast(nearest);
	pt.polyRef = static_cast<uint32_t>(ref);
	return pt;
}

bool NavMesh::isValid() const {
	return m_impl && m_impl->navMesh != nullptr && m_impl->navQuery != nullptr;
}

int NavMesh::getTileCount() const {
	if (!isValid()) return 0;
	int count = 0;
	const dtNavMesh* mesh = m_impl->navMesh;
	for (int i = 0; i < mesh->getMaxTiles(); ++i) {
		const dtMeshTile* tile = mesh->getTile(i);
		if (tile && tile->header) ++count;
	}
	return count;
}

int NavMesh::getPolyCount() const {
	if (!isValid()) return 0;
	int count = 0;
	const dtNavMesh* mesh = m_impl->navMesh;
	for (int i = 0; i < mesh->getMaxTiles(); ++i) {
		const dtMeshTile* tile = mesh->getTile(i);
		if (tile && tile->header) {
			count += tile->header->polyCount;
		}
	}
	return count;
}

const NavMeshBuildSettings& NavMesh::getBuildSettings() const {
	return m_impl->buildSettings;
}

uint64_t NavMesh::getVersion() const {
	return m_impl->version;
}

void NavMesh::debugDraw(DebugDrawList& out, const NavDebugSettings& settings) const {
	if (!isValid() || !settings.drawNavMesh) return;

	DebugDrawLayer& layer = out.cachedLayer(this);
	const uint64_t targetVersion = debugLayerVersion(m_impl->version, settings);
	if (layer.version == targetVersion) return;

	layer.vertices.clear();
	const dtNavMesh* mesh = m_impl->navMesh;
	const float lift = settings.heightOffset;

	for (int t = 0; t < mesh->getMaxTiles(); ++t) {
		const dtMeshTile* tile = mesh->getTile(t);
		if (!tile || !tile->header) continue;

		for (int p = 0; p < tile->header->polyCount; ++p) {
			const dtPoly* poly = &tile->polys[p];
			if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;

			for (unsigned int j = 0; j < poly->vertCount; ++j) {
				const unsigned int next = (j + 1) % poly->vertCount;
				const float* v0 = &tile->verts[poly->verts[j] * 3];
				const float* v1 = &tile->verts[poly->verts[next] * 3];

				const bool isBoundary = (poly->neis[j] == 0);
				const Vector4& color =
					isBoundary ? settings.navMeshBoundaryColor : settings.navMeshColor;

				layer.vertices.push_back(
					{Vector3(v0[0], v0[1] + lift, v0[2]), color});
				layer.vertices.push_back(
					{Vector3(v1[0], v1[1] + lift, v1[2]), color});
			}
		}
	}

	layer.version = targetVersion;
}

NavMeshImpl& NavMesh::_impl() {
	return *m_impl;
}

const NavMeshImpl& NavMesh::_impl() const {
	return *m_impl;
}

}  // namespace bird
