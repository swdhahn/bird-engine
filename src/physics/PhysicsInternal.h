#ifndef BIRD_PHYSICSINTERNAL_H
#define BIRD_PHYSICSINTERNAL_H

// Engine-internal: the only header that includes Jolt. Public physics headers
// stay Jolt-free so game code does not need Jolt's compile definitions.

// Jolt.h must precede every other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../util/Math.h"
#include "CollisionShape.h"
#include "ContactEvent.h"
#include "CharacterControllerComponent.h"
#include "PhysicsLayers.h"

namespace bird {

class RigidBodyComponent;
class CharacterControllerComponent;
class PhysicsWorld;
class WorldObject;
class Component;

/**
 * The world a component on this object belongs to: the nearest scene
 * owning a world, starting at the object (when a Scene) or its parent.
 * @throws std::logic_error when the object is not in a scene.
 */
PhysicsWorld* resolvePhysicsWorld(WorldObject* object, const char* who);

// ------------------------------------------------------------ conversions

inline JPH::Vec3 toJolt(const Vector3& v) {
	return JPH::Vec3(v.x, v.y, v.z);
}
inline JPH::Quat toJolt(const Quaternion& q) {
	return JPH::Quat(q.x, q.y, q.z, q.w).Normalized();
}
inline Vector3 toBird(JPH::Vec3Arg v) {
	return Vector3(v.GetX(), v.GetY(), v.GetZ());
}
inline Quaternion toBird(JPH::QuatArg q) {
	return Quaternion(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

// ----------------------------------------------------------------- layers
//
// A Jolt ObjectLayer (16 bits) packs: bits 0-6 the body's layers, bits 8-14
// its collision mask, bit 15 set for moving (kinematic or dynamic) bodies.
// The moving flag picks the broad-phase bucket, so static-vs-static pairs
// are never tested.

namespace layer_bits {
constexpr uint16_t kMovingFlag = 0x8000;

inline JPH::ObjectLayer pack(PhysicsLayerMask layers, PhysicsLayerMask mask,
							 bool moving) {
	return JPH::ObjectLayer((layers & kAllPhysicsLayers) |
							((mask & kAllPhysicsLayers) << 8) |
							(moving ? kMovingFlag : 0));
}
inline PhysicsLayerMask layers(JPH::ObjectLayer layer) {
	return PhysicsLayerMask(layer & kAllPhysicsLayers);
}
inline PhysicsLayerMask mask(JPH::ObjectLayer layer) {
	return PhysicsLayerMask((layer >> 8) & kAllPhysicsLayers);
}
inline bool isMoving(JPH::ObjectLayer layer) {
	return (layer & kMovingFlag) != 0;
}
}  // namespace layer_bits

namespace broad_phase {
constexpr JPH::BroadPhaseLayer kStatic(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
constexpr uint32_t kCount = 2;
}  // namespace broad_phase

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
   public:
	JPH::uint GetNumBroadPhaseLayers() const override { return broad_phase::kCount; }
	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
		return layer_bits::isMoving(layer) ? broad_phase::kMoving : broad_phase::kStatic;
	}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
		return layer == broad_phase::kMoving ? "MOVING" : "STATIC";
	}
#endif
};

class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
   public:
	bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bucket) const override {
		// Static bodies only need to meet moving ones.
		return layer_bits::isMoving(layer) || bucket == broad_phase::kMoving;
	}
};

class ObjectPairFilter final : public JPH::ObjectLayerPairFilter {
   public:
	bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
		if (!layer_bits::isMoving(a) && !layer_bits::isMoving(b)) return false;
		return (layer_bits::layers(a) & layer_bits::mask(b)) != 0 &&
			   (layer_bits::layers(b) & layer_bits::mask(a)) != 0;
	}
};

/** Query filter: accepts bodies on any layer in the query mask. */
class QueryLayerFilter final : public JPH::ObjectLayerFilter {
   public:
	explicit QueryLayerFilter(PhysicsLayerMask mask) : m_mask(mask) {}
	bool ShouldCollide(JPH::ObjectLayer layer) const override {
		return (layer_bits::layers(layer) & m_mask) != 0;
	}

   private:
	PhysicsLayerMask m_mask;
};

// ------------------------------------------------------------------ shape

struct CollisionShape::Native {
	JPH::RefConst<JPH::Shape> shape;
};

/** The Jolt shape behind a CollisionShape (never null for a non-empty one). */
inline const JPH::Shape* nativeShape(const CollisionShape& shape) {
	return static_cast<const JPH::Shape*>(shape._nativeShape());
}

// ----------------------------------------------------------------- server

struct PhysicsServerImpl {
	// Falls back to malloc when a step outgrows the arena, instead of aborting.
	std::unique_ptr<JPH::TempAllocatorImplWithMallocFallback> tempAllocator;
	std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
	BroadPhaseLayers broadPhaseLayers;
	ObjectVsBroadPhaseFilter objectVsBroadPhase;
	ObjectPairFilter objectPairs;
};

// ------------------------------------------------------------------ world

/** One begin/end notification recorded on a Jolt worker thread. */
struct RawContact {
	bool begin;
	bool sensor;
	JPH::BodyID body1;
	JPH::BodyID body2;
	Vector3 point;
	Vector3 normal;	 // from body1 towards body2
};

/**
 * Records Jolt's contact callbacks (which run on worker threads) into a
 * queue; PhysicsWorld drains the queue on the main thread after each step.
 */
class ContactQueue final : public JPH::ContactListener {
   public:
	void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
						const JPH::ContactManifold& manifold,
						JPH::ContactSettings& settings) override;
	void OnContactRemoved(const JPH::SubShapeIDPair& pair) override;

	std::vector<RawContact> drain();

   private:
	std::mutex m_mutex;
	std::vector<RawContact> m_events;
};

struct PhysicsWorldImpl {
	PhysicsServerImpl* server = nullptr;
	JPH::PhysicsSystem system;
	ContactQueue contacts;

	// All registered bodies, for pre-step sync; components store their index.
	std::vector<RigidBodyComponent*> bodies;
	// BodyID -> component, for contact dispatch and post-step sync.
	std::unordered_map<uint32_t, RigidBodyComponent*> byId;

	struct PairState {
		int touching = 0;  // sub-shape pairs currently in contact
		bool sensor = false;
		// Jolt drops the contacts of sleeping bodies; for the game the pair
		// still touches until a woken body fails to re-detect it.
		bool dormant = false;
	};
	// Key: (lower body ID, higher body ID).
	std::map<std::pair<uint32_t, uint32_t>, PairState> pairs;

	JPH::BodyIDVector activeScratch;

	// Character controllers, stepped before the simulation.
	std::vector<CharacterControllerComponent*> characters;
	// Inner body ID -> character, so queries can report the character.
	std::unordered_map<uint32_t, CharacterControllerComponent*> characterByInnerBody;
	JPH::CharacterVsCharacterCollisionSimple characterVsCharacter;
};

/** Native state of a character controller. */
struct CharacterControllerComponent::Native {
	JPH::Ref<JPH::CharacterVirtual> character;
	JPH::CharacterVirtual::ExtendedUpdateSettings update;
	JPH::ObjectLayer layer = 0;
};

}  // namespace bird

#endif	// !BIRD_PHYSICSINTERNAL_H
