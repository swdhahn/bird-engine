#ifndef BIRD_COLLISIONSHAPE_H
#define BIRD_COLLISIONSHAPE_H

#include <cstdint>
#include <memory>
#include <vector>

#include "../util/Math.h"

namespace bird {

class Mesh;

/**
 * Immutable collision geometry, shared cheaply by copy (reference counted).
 *
 * Shapes are built in object space at unit scale; a body applies its
 * object's scale on top. Construction validates its input and throws on
 * bad dimensions or degenerate geometry, so a body never receives an
 * invalid shape.
 *
 * Triangle meshes have no volume: they are allowed only on static and
 * kinematic bodies. Use convexHull() for moving, dynamic mesh-like bodies.
 */
class CollisionShape {
   public:
	enum class Kind : uint8_t {
		Empty,
		Box,
		Sphere,
		Capsule,
		Cylinder,
		ConvexHull,
		TriangleMesh,
		Compound
	};

	/** One child of a compound shape, placed in the compound's space. */
	struct Part;

	/** An empty shape; a body rejects it. */
	CollisionShape();

	/** @throws std::invalid_argument unless every half extent is positive. */
	static CollisionShape box(const Vector3& halfExtents);
	/** @throws std::invalid_argument unless radius is positive. */
	static CollisionShape sphere(float radius);
	/**
	 * Y-axis capsule: a cylinder of half height halfHeight capped by
	 * hemispheres of the given radius.
	 * @throws std::invalid_argument unless both values are positive.
	 */
	static CollisionShape capsule(float halfHeight, float radius);
	/** Y-axis cylinder. @throws std::invalid_argument unless both are positive. */
	static CollisionShape cylinder(float halfHeight, float radius);

	/**
	 * Convex hull of a point cloud.
	 * @throws std::invalid_argument for fewer than 4 points or a flat cloud.
	 */
	static CollisionShape convexHull(const std::vector<Vector3>& points);
	/**
	 * Convex hull of the vertices of one or more meshes.
	 * @throws std::invalid_argument when a mesh has no CPU-side vertex data.
	 */
	static CollisionShape convexHull(const std::vector<std::shared_ptr<Mesh>>& meshes);

	/**
	 * Static triangle mesh.
	 * @param vertices Vertex positions.
	 * @param indices Three indices per triangle.
	 * @throws std::invalid_argument for an empty mesh, an index count that
	 *         is not a multiple of 3, or an index out of range.
	 */
	static CollisionShape triangleMesh(const std::vector<Vector3>& vertices,
									   const std::vector<uint32_t>& indices);
	/**
	 * Static triangle mesh from rendered meshes (all merged into one shape).
	 * @throws std::invalid_argument when a mesh has no CPU-side data.
	 */
	static CollisionShape triangleMesh(const std::vector<std::shared_ptr<Mesh>>& meshes);

	/**
	 * Several shapes rigidly joined.
	 * @throws std::invalid_argument when parts is empty or a part is empty.
	 */
	static CollisionShape compound(const std::vector<Part>& parts);

	Kind getKind() const { return m_kind; }
	bool isEmpty() const { return m_kind == Kind::Empty; }
	/** True for triangle meshes and compounds containing one. */
	bool containsTriangleMesh() const { return m_containsMesh; }

	/** Opaque engine-internal handle (a const JPH::Shape*). */
	const void* _nativeShape() const;

	/** Engine-internal: wraps an existing native shape. */
	struct Native;
	CollisionShape(std::shared_ptr<const Native> native, Kind kind, bool containsMesh);

   private:
	std::shared_ptr<const Native> m_native;
	Kind m_kind = Kind::Empty;
	bool m_containsMesh = false;
};

struct CollisionShape::Part {
	CollisionShape shape;
	Vector3 position{0.0f};
	Quaternion rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

}  // namespace bird

#endif	// !BIRD_COLLISIONSHAPE_H
