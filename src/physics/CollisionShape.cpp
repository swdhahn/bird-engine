#include "CollisionShape.h"

#include <algorithm>
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

CollisionShape CollisionShape::compound(const std::vector<Part>& parts) {
	if (parts.empty()) {
		throw std::invalid_argument("CollisionShape::compound: needs at least one part");
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
