#ifndef BIRD_RIGIDBODYCOMPONENT_H
#define BIRD_RIGIDBODYCOMPONENT_H

#include <cstdint>
#include <functional>
#include <vector>

#include "../scene/components/Component.h"
#include "CollisionShape.h"
#include "ContactEvent.h"
#include "PhysicsLayers.h"

namespace bird {

class PhysicsWorld;

enum class MotionType : uint8_t {
	/** Never moves under simulation; the game may still teleport it. */
	Static,
	/** Moved by the game; pushes dynamic bodies but is not pushed back. */
	Kinematic,
	/** Fully simulated: gravity, forces and collisions move it. */
	Dynamic
};

struct RigidBodySettings {
	MotionType motionType = MotionType::Dynamic;
	/** Mass in kg; 0 derives it from the shape volume (density 1000 kg/m^3). */
	float mass = 0.0f;
	float friction = 0.5f;
	float restitution = 0.0f;
	float linearDamping = 0.05f;
	float angularDamping = 0.05f;
	float gravityFactor = 1.0f;
	/** Sensors detect overlaps (trigger events) but never push bodies. */
	bool isSensor = false;
	bool allowSleeping = true;
	PhysicsLayerMask layers = 0x01;	 // layer 0
	PhysicsLayerMask mask = kAllPhysicsLayers;
};

/**
 * Gives its WorldObject a rigid body in the physics world of its scene.
 *
 * The body is created when the object is initialized into a scene and
 * destroyed when it is deinitialized, removed or destroyed. Bodies follow
 * the object's world position and rotation (local offsets stay visual) and
 * use its scale. Move the object as usual: the next step pushes the change
 * to the body. Dynamic bodies write their simulated pose back every step.
 *
 * Contacts reach the callbacks below and every ScriptComponent on the same
 * object (ScriptComponent::onContactBegin and friends).
 *
 * Methods that act on the simulated body throw std::logic_error while the
 * component is not in a world.
 */
class RigidBodyComponent : public Component {
   public:
	using ContactCallback = std::function<void(const ContactEvent&)>;

	/**
	 * @throws std::invalid_argument for an empty shape, a triangle mesh on a
	 *         dynamic body, or a negative mass, friction or damping.
	 */
	explicit RigidBodyComponent(CollisionShape shape, const RigidBodySettings& settings = {});
	~RigidBodyComponent() override;

	void init() override {}
	void deinit() override {}
	void _init() override;
	void _deinit() override;

	bool isInWorld() const { return m_world != nullptr; }
	PhysicsWorld* getWorld() const { return m_world; }
	const CollisionShape& getShape() const { return m_shape; }
	const RigidBodySettings& getSettings() const { return m_settings; }
	MotionType getMotionType() const { return m_settings.motionType; }

	/** Replaces the shape (mass is recomputed for dynamic bodies). */
	void setShape(CollisionShape shape);

	Vector3 getLinearVelocity() const;
	void setLinearVelocity(const Vector3& velocity);
	Vector3 getAngularVelocity() const;
	void setAngularVelocity(const Vector3& velocity);

	/** Continuous force at the centre of mass, for this step (N). */
	void applyForce(const Vector3& force);
	/** Continuous force at a world-space point, for this step. */
	void applyForceAt(const Vector3& force, const Vector3& point);
	void applyTorque(const Vector3& torque);
	/** Instant velocity change at the centre of mass (N*s). */
	void applyImpulse(const Vector3& impulse);
	void applyImpulseAt(const Vector3& impulse, const Vector3& point);

	/**
	 * Moves the object and its body immediately (no sweeping, velocities
	 * kept), so queries in the same frame already see the new pose.
	 */
	void teleport(const Vector3& position, const Quaternion& rotation);

	void wake();
	bool isSleeping() const;
	/** Mass in kg (0 for static and kinematic bodies). */
	float getMass() const;

	void setFriction(float friction);
	void setRestitution(float restitution);
	void setGravityFactor(float factor);
	void setLayers(PhysicsLayerMask layers, PhysicsLayerMask mask);

	RigidBodyComponent& setOnContactBegin(ContactCallback callback);
	RigidBodyComponent& setOnContactEnd(ContactCallback callback);
	RigidBodyComponent& setOnTriggerEnter(ContactCallback callback);
	RigidBodyComponent& setOnTriggerExit(ContactCallback callback);

	// ---- engine-internal (PhysicsWorld) ----
	enum class _EventKind : uint8_t { ContactBegin, ContactEnd, TriggerEnter, TriggerExit };
	uint32_t _bodyId() const { return m_bodyId; }
	size_t _index() const { return m_index; }
	void _setIndex(size_t index) { m_index = index; }
	/** Pushes game-side transform edits to the body before a step. */
	void _preStep(float fixedDelta);
	/** Writes the simulated pose back after a step. */
	void _writeBack(const Vector3& position, const Quaternion& rotation);
	/**
	 * Everything that should hear an event of this kind: the matching
	 * callback and each ScriptComponent on the object (copied, so delivery
	 * survives callbacks that change components).
	 */
	std::vector<ContactCallback> _receivers(_EventKind kind) const;
	/** The world is being destroyed: forget the body without touching it. */
	void _forgetWorld();

   private:
	void requireWorld(const char* what) const;
	void createBody();
	void destroyBody();

	CollisionShape m_shape;
	RigidBodySettings m_settings;
	PhysicsWorld* m_world = nullptr;
	uint32_t m_bodyId = 0xFFFFFFFFu;
	size_t m_index = 0;
	uint64_t m_syncedRevision = 0;
	Vector3 m_syncedScale{1.0f};
	bool m_kinematicMoving = false;
	bool m_removing = false;
	ContactCallback m_onContactBegin;
	ContactCallback m_onContactEnd;
	ContactCallback m_onTriggerEnter;
	ContactCallback m_onTriggerExit;
};

}  // namespace bird

#endif	// !BIRD_RIGIDBODYCOMPONENT_H
