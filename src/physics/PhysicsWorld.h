#ifndef BIRD_PHYSICSWORLD_H
#define BIRD_PHYSICSWORLD_H

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "../util/Math.h"
#include "CollisionShape.h"
#include "PhysicsLayers.h"

namespace bird {

class Component;
class DebugDrawList;
class PhysicsServer;
class CharacterControllerComponent;
class RigidBodyComponent;
class WorldObject;
struct PhysicsWorldImpl;

struct PhysicsWorldSettings {
	uint32_t maxBodies = 65536;
	uint32_t maxBodyPairs = 65536;
	uint32_t maxContactConstraints = 10240;
	Vector3 gravity{0.0f, -9.81f, 0.0f};
};

struct RaycastHit {
	/** The rigid body hit, or nullptr when the hit is a character. */
	RigidBodyComponent* body = nullptr;
	/** The object hit (a rigid body's or a character controller's). */
	WorldObject* object = nullptr;
	Vector3 point{0.0f};
	/** Surface normal at the hit, facing the ray origin. */
	Vector3 normal{0.0f};
	float distance = 0.0f;
};

struct ShapeCastHit {
	/** The rigid body hit, or nullptr when the hit is a character. */
	RigidBodyComponent* body = nullptr;
	/** The object hit (a rigid body's or a character controller's). */
	WorldObject* object = nullptr;
	/** Contact point on the hit body. */
	Vector3 point{0.0f};
	/** Normal pointing from the hit body towards the cast shape. */
	Vector3 normal{0.0f};
	/** Fraction of the sweep travelled before contact, in [0, 1]. */
	float fraction = 0.0f;
};

/** What PhysicsWorld::debugDraw() draws. */
struct PhysicsDebugSettings {
	/** Wireframe of every body's shape (characters included). */
	bool shapes = true;
	/** Colour shapes by awake (random per island) / asleep instead of by
	 * motion type (static grey, kinematic green, dynamic per body). */
	bool colorBySleep = false;
	bool boundingBoxes = false;
	/** Linear and angular velocity vectors of moving bodies. */
	bool velocities = false;
	bool centersOfMass = false;
	/** Ground contact normal under each supported character (yellow). */
	bool characterGround = true;
};

/**
 * One independent rigid-body simulation (a Jolt PhysicsSystem).
 *
 * Scenes own worlds (see Scene::getPhysicsWorld); RigidBodyComponents join
 * the world of their scene. step() runs one fixed step:
 *   1. pre-sync: objects the game moved are pushed to their bodies
 *      (teleport for static and dynamic bodies, MoveKinematic for kinematic),
 *      then character controllers move and write their position back;
 *   2. the Jolt update, on the server's worker threads;
 *   3. post-sync: active dynamic bodies write their pose back to their
 *      objects (O(active bodies));
 *   4. contact and trigger events are delivered on the calling thread.
 *
 * Every method must be called from the main thread, outside step().
 */
class PhysicsWorld {
   public:
	/** Registers with the server, which then steps this world. */
	explicit PhysicsWorld(PhysicsServer& server, const PhysicsWorldSettings& settings = {});
	/** Destroys remaining bodies' Jolt state; their components detach. */
	~PhysicsWorld();

	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;

	/** @throws std::invalid_argument unless fixedDelta is positive. */
	void step(float fixedDelta);

	void setGravity(const Vector3& gravity);
	Vector3 getGravity() const;

	/** Rigid bodies in this world (characters not included). */
	size_t getBodyCount() const;
	/** Character controllers in this world. */
	size_t getCharacterCount() const;
	/** Bodies currently awake (simulated). */
	size_t getActiveBodyCount() const;

	/**
	 * Closest hit along a ray.
	 * @param direction Ray direction (normalized internally).
	 * @param maxDistance Ray length.
	 * @param mask Layers to test against.
	 * @param ignore Optional RigidBodyComponent or CharacterControllerComponent
	 *        to skip (such as the caster's own).
	 * @throws std::invalid_argument for a zero direction or non-positive length.
	 */
	std::optional<RaycastHit> raycast(const Vector3& origin, const Vector3& direction,
									  float maxDistance,
									  PhysicsLayerMask mask = kAllPhysicsLayers,
									  const Component* ignore = nullptr) const;

	/** Every hit along a ray (one per body), nearest first. */
	std::vector<RaycastHit> raycastAll(const Vector3& origin, const Vector3& direction,
									   float maxDistance,
									   PhysicsLayerMask mask = kAllPhysicsLayers,
									   const Component* ignore = nullptr) const;

	/**
	 * Sweeps a shape from `from` to `to` (keeping `rotation`) and reports the
	 * first body it would touch.
	 * @throws std::invalid_argument for an empty shape.
	 */
	std::optional<ShapeCastHit> shapeCast(const CollisionShape& shape, const Vector3& from,
										  const Quaternion& rotation, const Vector3& to,
										  PhysicsLayerMask mask = kAllPhysicsLayers,
										  const Component* ignore = nullptr) const;

	/**
	 * Rigid bodies overlapping a shape placed at position/rotation (each
	 * once; characters are not reported).
	 * @throws std::invalid_argument for an empty shape.
	 */
	std::vector<RigidBodyComponent*> overlap(const CollisionShape& shape,
											 const Vector3& position,
											 const Quaternion& rotation,
											 PhysicsLayerMask mask = kAllPhysicsLayers,
											 const Component* ignore = nullptr) const;

	/**
	 * Appends wireframes of this world to a debug line list (typically
	 * GraphicsPipeline::getDebugDraw()). Static bodies go into a cached layer
	 * rebuilt only when static geometry or the settings change, so large
	 * levels cost nothing per frame; moving bodies are drawn every call.
	 * Convex shapes use level of detail by distance to cameraPosition. Does
	 * nothing when Jolt was built without its debug renderer.
	 */
	void debugDraw(DebugDrawList& out, const PhysicsDebugSettings& settings = {},
				   const Vector3& cameraPosition = Vector3(0.0f)) const;

	/** Engine-internal. */
	PhysicsWorldImpl& _impl() { return *m_impl; }
	void _addBody(RigidBodyComponent* body);
	/** A static body moved or changed shape: rebuild cached static wireframes. */
	void _staticGeometryChanged();
	/**
	 * Unregisters a body. With notify, partners receive end events first
	 * (explicit removal); destructor cleanup passes false, because partner
	 * objects may be mid-destruction.
	 */
	void _removeBody(RigidBodyComponent* body, bool notify = true);
	/** As _removeBody, for a character controller. */
	void _removeCharacter(CharacterControllerComponent* character, bool notify = true);

   private:
	void dispatchContacts();

	PhysicsServer& m_server;
	std::unique_ptr<PhysicsWorldImpl> m_impl;
	bool m_stepping = false;
};

}  // namespace bird

#endif	// !BIRD_PHYSICSWORLD_H
