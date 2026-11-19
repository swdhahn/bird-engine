#ifndef BIRD_JOINT_H
#define BIRD_JOINT_H

#include <cstdint>
#include <memory>

namespace bird {

class RigidBodyComponent;
class PhysicsWorld;

enum class JointType : uint8_t { Fixed, Point, Hinge, Slider, Distance };

/**
 * A constraint between two rigid bodies (or one body and the world), made
 * with PhysicsWorld::create*Joint and owned by the world. Destroying either
 * body (or the world) destroys the joint; the handle is invalid afterwards.
 */
class Joint {
   public:
	~Joint();
	Joint(const Joint&) = delete;
	Joint& operator=(const Joint&) = delete;

	JointType getType() const { return m_type; }
	RigidBodyComponent* getBodyA() const { return m_a; }
	/** nullptr when the joint holds body A to the world. */
	RigidBodyComponent* getBodyB() const { return m_b; }

	/** A disabled joint stops constraining (and wakes its bodies). */
	void setEnabled(bool enabled);
	bool isEnabled() const;

	/**
	 * Drives a hinge at an angular velocity (rad/s), or a slider at a linear
	 * velocity (m/s); limited by maxForce (N or N*m). 0 velocity holds.
	 * @throws std::logic_error for other joint types.
	 */
	void setMotorVelocity(float velocity, float maxForce = 1000.0f);
	/** Turns a motor off (free joint again). */
	void disableMotor();
	/** Hinge angle (rad) or slider position (m) from the initial pose.
	 * @throws std::logic_error for other joint types. */
	float getPosition() const;

	struct Native;
	/** Engine-internal: worlds create joints. */
	Joint(JointType type, RigidBodyComponent* a, RigidBodyComponent* b,
		  std::unique_ptr<Native> native, PhysicsWorld* world);
	Native& _native() { return *m_native; }

   private:
	JointType m_type;
	RigidBodyComponent* m_a;
	RigidBodyComponent* m_b;
	std::unique_ptr<Native> m_native;
	PhysicsWorld* m_world;
};

}  // namespace bird

#endif	// !BIRD_JOINT_H
