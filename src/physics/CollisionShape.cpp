#include "CollisionShape.h"

#include <algorithm>
#include <future>
#include <limits>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "../renders/generic/Mesh.h"
#include "PhysicsInternal.h"

namespace bird {

namespace {

CollisionShape wrap(const JPH::ShapeSettings& settings, CollisionShape::Kind kind,
					bool containsMesh, const char* what) {
	JPH::ShapeSettings::ShapeResult result = settings.Create();
	if (result.HasError()) {
		throw std::invalid_argument(std::string("CollisionShape::") + what + ": " +
									result.GetError().c_str());
	}
	auto native = std::make_shared<CollisionShape::Native>();
	native->shape = result.Get();
	return CollisionShape(native, kind, containsMesh);
}

/** Jolt rounds convex shapes by this radius; it must fit inside the shape. */
float convexRadiusFor(float smallestExtent) {
	return std::min(JPH::cDefaultConvexRadius, smallestExtent * 0.5f);
}

void requirePositive(float value, const char* what) {
	if (!(value > 0.0f)) {
		throw std::invalid_argument(std::string("CollisionShape::") + what +
									": dimensions must be positive");
	}
}

/** Appends a mesh's CPU-side positions and (optionally) triangle indices. */
void appendMesh(const std::shared_ptr<Mesh>& mesh, JPH::VertexList& vertices,
				JPH::IndexedTriangleList* triangles, const char* what) {
	if (!mesh) {
		throw std::invalid_argument(std::string("CollisionShape::") + what + ": null mesh");
	}
	Buffer<float>* vertexBuffer = mesh->getVertexBuffer();
	if (vertexBuffer == nullptr || vertexBuffer->m_pData == nullptr) {
		throw std::invalid_argument(std::string("CollisionShape::") + what +
									": mesh has no CPU-side vertex data");
	}
	const uint32_t base = uint32_t(vertices.size());
	const size_t floats = vertexBuffer->getBufferSize();
	const float* data = vertexBuffer->m_pData.get();
	for (size_t i = 0; i + 2 < floats; i += 3) {
		vertices.push_back(JPH::Float3(data[i], data[i + 1], data[i + 2]));
	}
	if (triangles == nullptr) return;

	Buffer<uint32_t>* indexBuffer = mesh->getIndexBuffer();
	if (indexBuffer == nullptr || indexBuffer->m_pData == nullptr) {
		throw std::invalid_argument(std::string("CollisionShape::") + what +
									": mesh has no CPU-side index data");
	}
	const size_t count = indexBuffer->getBufferSize();
	const uint32_t* indices = indexBuffer->m_pData.get();
	const uint32_t vertexCount = uint32_t(vertices.size()) - base;
	for (size_t i = 0; i + 2 < count; i += 3) {
		if (indices[i] >= vertexCount || indices[i + 1] >= vertexCount ||
			indices[i + 2] >= vertexCount) {
			throw std::invalid_argument(std::string("CollisionShape::") + what +
										": mesh index out of range");
		}
		triangles->push_back(JPH::IndexedTriangle(base + indices[i], base + indices[i + 1],
												  base + indices[i + 2]));
	}
}

}  // namespace

CollisionShape::CollisionShape() = default;

CollisionShape::CollisionShape(std::shared_ptr<const Native> native, Kind kind,
							   bool containsMesh)
	: m_native(std::move(native)), m_kind(kind), m_containsMesh(containsMesh) {}

const void* CollisionShape::_nativeShape() const {
	return m_native ? m_native->shape.GetPtr() : nullptr;
}

CollisionShape CollisionShape::box(const Vector3& halfExtents) {
	requirePositive(halfExtents.x, "box");
	requirePositive(halfExtents.y, "box");
	requirePositive(halfExtents.z, "box");
	const float smallest = std::min({halfExtents.x, halfExtents.y, halfExtents.z});
	return wrap(JPH::BoxShapeSettings(toJolt(halfExtents), convexRadiusFor(smallest)),
				Kind::Box, false, "box");
}

CollisionShape CollisionShape::sphere(float radius) {
	requirePositive(radius, "sphere");
	return wrap(JPH::SphereShapeSettings(radius), Kind::Sphere, false, "sphere");
}

CollisionShape CollisionShape::capsule(float halfHeight, float radius) {
	requirePositive(halfHeight, "capsule");
	requirePositive(radius, "capsule");
	return wrap(JPH::CapsuleShapeSettings(halfHeight, radius), Kind::Capsule, false,
				"capsule");
}

CollisionShape CollisionShape::cylinder(float halfHeight, float radius) {
	requirePositive(halfHeight, "cylinder");
	requirePositive(radius, "cylinder");
	return wrap(JPH::CylinderShapeSettings(halfHeight, radius,
										   convexRadiusFor(std::min(halfHeight, radius))),
				Kind::Cylinder, false, "cylinder");
}

CollisionShape CollisionShape::convexHull(const std::vector<Vector3>& points) {
	if (points.size() < 4) {
		throw std::invalid_argument("CollisionShape::convexHull: needs at least 4 points");
	}
	JPH::Array<JPH::Vec3> joltPoints;
	joltPoints.reserve(points.size());
	for (const Vector3& p : points) joltPoints.push_back(toJolt(p));
	return wrap(JPH::ConvexHullShapeSettings(joltPoints), Kind::ConvexHull, false,
				"convexHull");
}

CollisionShape CollisionShape::convexHull(const std::vector<std::shared_ptr<Mesh>>& meshes) {
	JPH::VertexList vertices;
	for (const auto& mesh : meshes) appendMesh(mesh, vertices, nullptr, "convexHull");
	std::vector<Vector3> points;
	points.reserve(vertices.size());
	for (const JPH::Float3& v : vertices) points.emplace_back(v.x, v.y, v.z);
	return convexHull(points);
}

CollisionShape CollisionShape::triangleMesh(const std::vector<Vector3>& vertices,
											const std::vector<uint32_t>& indices) {
	if (vertices.empty() || indices.empty() || indices.size() % 3 != 0) {
		throw std::invalid_argument(
			"CollisionShape::triangleMesh: needs vertices and a multiple of 3 indices");
	}
	JPH::VertexList joltVertices;
	joltVertices.reserve(vertices.size());
	for (const Vector3& v : vertices) joltVertices.push_back(JPH::Float3(v.x, v.y, v.z));
	JPH::IndexedTriangleList triangles;
	triangles.reserve(indices.size() / 3);
	for (size_t i = 0; i < indices.size(); i += 3) {
		if (indices[i] >= vertices.size() || indices[i + 1] >= vertices.size() ||
			indices[i + 2] >= vertices.size()) {
			throw std::invalid_argument("CollisionShape::triangleMesh: index out of range");
		}
		triangles.push_back(JPH::IndexedTriangle(indices[i], indices[i + 1], indices[i + 2]));
	}
	return wrap(JPH::MeshShapeSettings(joltVertices, triangles), Kind::TriangleMesh, true,
				"triangleMesh");
}

CollisionShape CollisionShape::triangleMesh(const std::vector<std::shared_ptr<Mesh>>& meshes) {
	JPH::VertexList vertices;
	JPH::IndexedTriangleList triangles;
	for (const auto& mesh : meshes) appendMesh(mesh, vertices, &triangles, "triangleMesh");
	if (triangles.empty()) {
		throw std::invalid_argument("CollisionShape::triangleMesh: meshes have no triangles");
	}
	return wrap(JPH::MeshShapeSettings(vertices, triangles), Kind::TriangleMesh, true,
				"triangleMesh");
}

std::vector<CollisionShape> CollisionShape::triangleMeshChunks(
	const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices, int cellsX,
	int cellsZ) {
	if (cellsX < 1 || cellsZ < 1) {
		throw std::invalid_argument("CollisionShape::triangleMeshChunks: cells must be >= 1");
	}
	if (indices.size() % 3 != 0) {
		throw std::invalid_argument(
			"CollisionShape::triangleMeshChunks: index count must be a multiple of 3");
	}

	// Keep non-degenerate triangles and find the centroid extent: O(n).
	std::vector<uint32_t> kept;
	kept.reserve(indices.size() / 3);
	Vector2 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
	for (size_t t = 0; t < indices.size(); t += 3) {
		if (indices[t] >= vertices.size() || indices[t + 1] >= vertices.size() ||
			indices[t + 2] >= vertices.size()) {
			throw std::invalid_argument("CollisionShape::triangleMeshChunks: index out of range");
		}
		const Vector3& a = vertices[indices[t]];
		const Vector3& b = vertices[indices[t + 1]];
		const Vector3& c = vertices[indices[t + 2]];
		if (glm::length(glm::cross(b - a, c - a)) < 1e-10f) continue;
		const Vector3 centroid = (a + b + c) / 3.0f;
		lo = glm::min(lo, Vector2(centroid.x, centroid.z));
		hi = glm::max(hi, Vector2(centroid.x, centroid.z));
		kept.push_back(uint32_t(t));
	}
	if (kept.empty()) {
		throw std::invalid_argument("CollisionShape::triangleMeshChunks: no usable triangles");
	}

	// Bucket triangles by cell.
	const Vector2 extent = glm::max(hi - lo, Vector2(1e-6f));
	std::vector<std::vector<uint32_t>> cells(size_t(cellsX) * cellsZ);
	for (uint32_t t : kept) {
		const Vector3 centroid =
			(vertices[indices[t]] + vertices[indices[t + 1]] + vertices[indices[t + 2]]) / 3.0f;
		const int x = std::min(cellsX - 1, int((centroid.x - lo.x) / extent.x * cellsX));
		const int z = std::min(cellsZ - 1, int((centroid.z - lo.y) / extent.y * cellsZ));
		cells[size_t(z) * cellsX + x].push_back(t);
	}

	// Build each non-empty cell's mesh (only the vertices it uses) in parallel.
	std::vector<std::future<CollisionShape>> builds;
	for (const auto& cell : cells) {
		if (cell.empty()) continue;
		builds.push_back(std::async(std::launch::async, [&vertices, &indices, &cell] {
			JPH::VertexList cellVertices;
			JPH::IndexedTriangleList triangles;
			triangles.reserve(cell.size());
			std::unordered_map<uint32_t, uint32_t> remap;
			auto local = [&](uint32_t index) {
				auto [it, added] = remap.emplace(index, uint32_t(cellVertices.size()));
				if (added) {
					const Vector3& v = vertices[index];
					cellVertices.push_back(JPH::Float3(v.x, v.y, v.z));
				}
				return it->second;
			};
			for (uint32_t t : cell) {
				triangles.push_back(JPH::IndexedTriangle(local(indices[t]), local(indices[t + 1]),
														 local(indices[t + 2])));
			}
			return wrap(JPH::MeshShapeSettings(cellVertices, triangles), Kind::TriangleMesh,
						true, "triangleMeshChunks");
		}));
	}
	std::vector<CollisionShape> chunks;
	chunks.reserve(builds.size());
	for (auto& build : builds) chunks.push_back(build.get());	 // rethrows build errors
	return chunks;
}

std::vector<CollisionShape> CollisionShape::triangleMeshChunks(
	const std::vector<std::shared_ptr<Mesh>>& meshes, int cellsX, int cellsZ) {
	JPH::VertexList joltVertices;
	JPH::IndexedTriangleList triangles;
	for (const auto& mesh : meshes) appendMesh(mesh, joltVertices, &triangles, "triangleMeshChunks");
	std::vector<Vector3> vertices;
	vertices.reserve(joltVertices.size());
	for (const JPH::Float3& v : joltVertices) vertices.emplace_back(v.x, v.y, v.z);
	std::vector<uint32_t> indices;
	indices.reserve(triangles.size() * 3);
	for (const JPH::IndexedTriangle& t : triangles) {
		indices.insert(indices.end(), {t.mIdx[0], t.mIdx[1], t.mIdx[2]});
	}
	return triangleMeshChunks(vertices, indices, cellsX, cellsZ);
}

CollisionShape CollisionShape::compound(const std::vector<Part>& parts) {
	if (parts.empty()) {
		throw std::invalid_argument("CollisionShape::compound: needs at least one part");
	}
	if (parts.size() == 1) {
		const Part& part = parts.front();
		if (part.shape.isEmpty()) {
			throw std::invalid_argument("CollisionShape::compound: empty part");
		}
		return wrap(JPH::RotatedTranslatedShapeSettings(toJolt(part.position),
														toJolt(part.rotation),
														nativeShape(part.shape)),
					Kind::Compound, part.shape.containsTriangleMesh(), "compound");
	}
	JPH::StaticCompoundShapeSettings settings;
	bool containsMesh = false;
	for (const Part& part : parts) {
		if (part.shape.isEmpty()) {
			throw std::invalid_argument("CollisionShape::compound: empty part");
		}
		settings.AddShape(toJolt(part.position), toJolt(part.rotation),
						  nativeShape(part.shape));
		containsMesh = containsMesh || part.shape.containsTriangleMesh();
	}
	return wrap(settings, Kind::Compound, containsMesh, "compound");
}

}  // namespace bird
