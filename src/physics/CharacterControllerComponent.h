#ifndef BIRD_CHARACTERCONTROLLERCOMPONENT_H
#define BIRD_CHARACTERCONTROLLERCOMPONENT_H

#include <cstdint>
#include <memory>
#include <vector>

#include "../scene/components/Component.h"
#include "CollisionShape.h"
#include "PhysicsLayers.h"

namespace bird {

class PhysicsWorld;
class RigidBodyComponent;

/** Where the character stands after the last step. */
enum class GroundState : uint8_t {
	/** Standing on walkable ground. */
	OnGround,
	/** Touching ground too steep to climb (steeper than maxSlopeDegrees). */
	OnSteepGround,
	/** Touching something that does not support it (it will fall). */
	NotSupported,
	/** Touching nothing. */
	InAir
};

struct CharacterSettings {
	/** Total capsule height, feet to head, in metres. */
	float height = 1.8f;
	float radius = 0.3f;
	/**
	 * Optional custom shape whose origin is the object's position (the feet
	 * for the default capsule). When set, height and radius are ignored.
	 */
	CollisionShape shape;
	/** Steepest walkable slope, in degrees. */
	float maxSlopeDegrees = 50.0f;
	/** Tallest step (stair) walked up without jumping, in metres; 0 disables. */
	float stepHeight = 0.4f;
	/** How far the character snaps down to stay on descending ground; 0 disables. */
	float stickToFloorDistance = 0.5f;
	/** Mass in kg, used when dynamic bodies push the character. */
	float mass = 70.0f;
	/** Largest force (N) the character applies when pushing dynamic bodies. */
	float pushStrength = 100.0f;
	float gravityFactor = 1.0f;
	PhysicsLayerMask layers = 0x01;	 // layer 0
	PhysicsLayerMask mask = kAllPhysicsLayers;
};

/**
 * A player or NPC controller: a capsule swept through the world rather than
 * simulated, so it walks up stairs and slopes, sticks to descending ground,
 * stops at walls and steep slopes, and never tips over. Built on Jolt's
 * CharacterVirtual.
 *
 * The object's position is the character's feet; the capsule stays upright
 * whatever the object's rotation. Each physics step the controller applies
 * gravity, the move velocity and any requested jump, moves, and writes the
 * new position back to the object. Moving the object yourself teleports it.
 *
 * The character pushes dynamic bodies (up to pushStrength) and an inner
 * kinematic body lets falling bodies, other bodies and raycasts hit it
 * (RaycastHit::object is the character's object).
 *
 * Call the movement methods from ProcessScript::physicsProcess or process;
 * the move velocity persists until changed.
 */
class CharacterControllerComponent : public Component {
   public:
	/**
	 * @throws std::invalid_argument for a non-positive height or radius
	 *         (height must exceed 2 * radius), a triangle-mesh shape, or a
	 *         negative mass, strength, step height or slope.
	 */
	explicit CharacterControllerComponent(const CharacterSettings& settings = {});
	~CharacterControllerComponent() override;

	void init() override {}
	void deinit() override {}
	void _init() override;
	void _deinit() override;

	bool isInWorld() const { return m_world != nullptr; }
	PhysicsWorld* getWorld() const { return m_world; }
	const CharacterSettings& getSettings() const { return m_settings; }

	/**
	 * Desired horizontal velocity in world space (m/s). The vertical part
	 * is ignored: gravity and jumps own it. Kept until changed.
	 */
	void setMoveVelocity(const Vector3& velocity);
	const Vector3& getMoveVelocity() const { return m_moveVelocity; }

	/**
	 * Requests a jump at the next step.
	 * @param speed Upward launch speed in m/s.
	 * @return false (and no jump) unless the character is on walkable ground.
	 */
	bool jump(float speed);

	/** Actual velocity after the last step. @throws std::logic_error outside a world. */
	Vector3 getVelocity() const;
	/** Overrides the full velocity for the next step (knockback, launch pads). */
	void setVelocity(const Vector3& velocity);

	/** @throws std::logic_error outside a world (for all ground queries). */
	GroundState getGroundState() const;
	bool isOnGround() const;
	/** Normal of the ground touched (zero in the air). */
	Vector3 getGroundNormal() const;
	/** The rigid body stood on, or nullptr (static geometry counts). */
	RigidBodyComponent* getGroundBody() const;
	/** Rigid bodies actually collided with during the last step, each once. */
	std::vector<RigidBodyComponent*> getTouchingBodies() const;

	/** Moves the object and the character immediately (no sweep). */
	void teleport(const Vector3& position);

	// ---- engine-internal (PhysicsWorld) ----
	/** Moves the character for one step and writes the result back. */
	void _step(float fixedDelta);
	/** The world is being destroyed: release Jolt state while it still exists. */
	void _forgetWorld();
	uint32_t _innerBodyId() const;
	size_t _index() const { return m_index; }
	void _setIndex(size_t index) { m_index = index; }

	struct Native;

   private:
	void requireWorld(const char* what) const;
	void destroyCharacter();

	CharacterSettings m_settings;
	std::unique_ptr<Native> m_native;
	PhysicsWorld* m_world = nullptr;
	size_t m_index = 0;
	Vector3 m_moveVelocity{0.0f};
	float m_jumpSpeed = 0.0f;
	bool m_jumpRequested = false;
	bool m_hasVelocityOverride = false;
	Vector3 m_velocityOverride{0.0f};
	uint64_t m_syncedRevision = 0;
};

}  // namespace bird

#endif	// !BIRD_CHARACTERCONTROLLERCOMPONENT_H
