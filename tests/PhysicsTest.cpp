#include <gtest/gtest.h>

#include <stdexcept>
#include <thread>

#include "physics/CollisionShape.h"
#include "physics/FixedStepClock.h"
#include "physics/PhysicsServer.h"
#include "physics/PhysicsWorld.h"
#include "physics/RigidBodyComponent.h"
#include "scene/Scene.h"
#include "scene/components/Script.h"

using namespace bird;

namespace {

constexpr float kStep = 1.0f / 60.0f;

/** A headless physics setup: server first, so the scene dies before it. */
class PhysicsTest : public ::testing::Test {
   protected:
	PhysicsServer server{PhysicsServerSettings{8 * 1024 * 1024, 2}};
	_own<Scene> root = _own_mk<Scene>();

	struct Body {
		WorldObject* object;
		RigidBodyComponent* body;
	};

	Body add(Scene& scene, CollisionShape shape, const Vector3& position,
			 RigidBodySettings settings = {}) {
		auto object = _own_mk<WorldObject>();
		auto component = _own_mk<RigidBodyComponent>(std::move(shape), settings);
		RigidBodyComponent* body = component.get();
		object->addComponent(std::move(component));
		object->setWorldPosition(position);
		WorldObject* raw = object.get();
		scene.addChild(std::move(object));
		return {raw, body};
	}

	Body add(CollisionShape shape, const Vector3& position, RigidBodySettings settings = {}) {
		return add(*root, std::move(shape), position, settings);
	}

	static RigidBodySettings motion(MotionType type) {
		RigidBodySettings settings;
		settings.motionType = type;
		return settings;
	}

	/** Static 100x2x100 floor whose top face is at y = 1. */
	Body addFloor(Scene& scene) {
		return add(scene, CollisionShape::box({50, 1, 50}), {0, 0, 0},
				   motion(MotionType::Static));
	}
	Body addFloor() { return addFloor(*root); }

	PhysicsWorld& world() { return *root->getPhysicsWorld(); }

	void run(int steps) {
		for (int i = 0; i < steps; i++) server.step(kStep);
	}
};

}  // namespace

// ------------------------------------------------------------- simulation

TEST_F(PhysicsTest, DynamicBodyFallsUnderGravity) {
	Body ball = add(CollisionShape::sphere(0.5f), {0, 10, 0});
	run(30);  // 0.5 s: about 1/2 g t^2 = 1.23 m (symplectic Euler: 1.27 m)
	const float drop = 10.0f - ball.object->getWorldPosition().y;
	EXPECT_GT(drop, 1.15f);
	EXPECT_LT(drop, 1.32f);
	EXPECT_LT(ball.body->getLinearVelocity().y, -4.5f);
}

TEST_F(PhysicsTest, BoxComesToRestOnStaticFloorAndSleeps) {
	Body floor = addFloor();
	Body box = add(CollisionShape::box({0.5f, 0.5f, 0.5f}), {0, 5, 0});
	run(300);
	EXPECT_NEAR(box.object->getWorldPosition().y, 1.5f, 0.05f);
	EXPECT_EQ(floor.object->getWorldPosition(), Vector3(0.0f));
	EXPECT_TRUE(box.body->isSleeping());
	EXPECT_EQ(world().getActiveBodyCount(), 0u);

	// Sleeping bodies are skipped by post-sync: no transform writes.
	const uint64_t revision = box.object->getTransformRevision();
	run(10);
	EXPECT_EQ(box.object->getTransformRevision(), revision);
}

TEST_F(PhysicsTest, KinematicBodyFollowsItsObject) {
	Body kinematic = add(CollisionShape::box({0.5f, 0.5f, 0.5f}), {0, 0, 0},
						 motion(MotionType::Kinematic));
	EXPECT_FALSE(world().raycast({2, 5, 0}, {0, -1, 0}, 10).has_value());

	kinematic.object->setWorldPosition({2, 0, 0});
	run(1);
	auto hit = world().raycast({2, 5, 0}, {0, -1, 0}, 10);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->body, kinematic.body);
	EXPECT_EQ(kinematic.object->getWorldPosition(), Vector3(2, 0, 0));	// not written back
	run(5);	 // target reached: the body must not drift on
	EXPECT_NEAR(kinematic.body->getLinearVelocity().x, 0.0f, 1e-5f);
	EXPECT_TRUE(world().raycast({2, 5, 0}, {0, -1, 0}, 10).has_value());
}

TEST_F(PhysicsTest, MovingTheObjectTeleportsTheBodyOnce) {
	addFloor();
	Body box = add(CollisionShape::box({0.5f, 0.5f, 0.5f}), {0, 1.5f, 0});
	run(120);
	ASSERT_TRUE(box.body->isSleeping());

	box.object->setWorldPosition({0, 10, 0});  // a game-side edit
	run(1);
	const float afterOne = box.object->getWorldPosition().y;
	EXPECT_LT(afterOne, 10.0f);
	EXPECT_GT(afterOne, 9.9f);
	run(1);	 // the write-back is not echoed as a new teleport
	EXPECT_LT(box.object->getWorldPosition().y, afterOne);
}

TEST_F(PhysicsTest, TeleportIsImmediatelyVisibleToQueries) {
	Body box = add(CollisionShape::box({0.5f, 0.5f, 0.5f}), {0, 0, 0},
				   motion(MotionType::Static));
	box.body->teleport({5, 0, 0}, Quaternion(1, 0, 0, 0));
	EXPECT_TRUE(world().raycast({5, 5, 0}, {0, -1, 0}, 10).has_value());
	EXPECT_EQ(box.object->getWorldPosition(), Vector3(5, 0, 0));
}

TEST_F(PhysicsTest, ObjectScaleScalesTheShape) {
	auto object = _own_mk<WorldObject>();
	object->addComponent(_own_mk<RigidBodyComponent>(CollisionShape::box({0.5f, 0.5f, 0.5f}),
													 motion(MotionType::Static)));
	object->setScale({2, 2, 2});
	WorldObject* raw = object.get();
	root->addChild(std::move(object));
	auto hit = world().raycast({0, 5, 0}, {0, -1, 0}, 10);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->point.y, 1.0f, 1e-3f);

	raw->setScale({4, 4, 4});  // applied at the next pre-sync
	run(1);
	EXPECT_NEAR(world().raycast({0, 5, 0}, {0, -1, 0}, 10)->point.y, 2.0f, 1e-3f);
}

TEST_F(PhysicsTest, ImpulseAndForcesMoveDynamicBodies) {
	RigidBodySettings settings;
	settings.gravityFactor = 0.0f;
	settings.mass = 2.0f;
	Body ball = add(CollisionShape::sphere(0.5f), {0, 0, 0}, settings);
	EXPECT_NEAR(ball.body->getMass(), 2.0f, 1e-4f);
	ball.body->applyImpulse({4, 0, 0});	 // dv = 4 / 2
	EXPECT_NEAR(ball.body->getLinearVelocity().x, 2.0f, 1e-3f);
	run(30);
	EXPECT_NEAR(ball.object->getWorldPosition().x, 1.0f, 0.05f);  // damping 0.05/s
}

// --------------------------------------------------------------- lifetime

TEST_F(PhysicsTest, RemovingComponentsAndObjectsDestroysBodies) {
	Body a = add(CollisionShape::sphere(0.5f), {0, 0, 0});
	add(CollisionShape::sphere(0.5f), {3, 0, 0});
	Body c = add(CollisionShape::sphere(0.5f), {6, 0, 0});
	EXPECT_EQ(world().getBodyCount(), 3u);

	root->removeChild(a.object);
	EXPECT_EQ(world().getBodyCount(), 2u);
	c.object->removeComponent(c.body);
	EXPECT_EQ(world().getBodyCount(), 1u);
	EXPECT_EQ(c.object->getComponents().size(), 0u);
	root->deinit();
	EXPECT_EQ(world().getBodyCount(), 0u);
}

TEST_F(PhysicsTest, BodyNotInWorldFailsFast) {
	RigidBodyComponent loose(CollisionShape::sphere(1));
	EXPECT_FALSE(loose.isInWorld());
	EXPECT_THROW(loose.applyImpulse({1, 0, 0}), std::logic_error);
	EXPECT_THROW(loose.getLinearVelocity(), std::logic_error);
	loose.setFriction(0.9f);  // settings may be changed before joining
	EXPECT_FLOAT_EQ(loose.getSettings().friction, 0.9f);
}

TEST_F(PhysicsTest, InvalidShapesAndSettingsAreRejected) {
	EXPECT_THROW(CollisionShape::box({1, 0, 1}), std::invalid_argument);
	EXPECT_THROW(CollisionShape::sphere(-1), std::invalid_argument);
	EXPECT_THROW(CollisionShape::convexHull({{0, 0, 0}, {1, 0, 0}}), std::invalid_argument);
	EXPECT_THROW(CollisionShape::triangleMesh({{0, 0, 0}}, {0, 0}), std::invalid_argument);
	EXPECT_THROW(CollisionShape::triangleMesh({{0, 0, 0}, {1, 0, 0}, {0, 0, 1}}, {0, 1, 7}),
				 std::invalid_argument);
	EXPECT_THROW(RigidBodyComponent{CollisionShape()}, std::invalid_argument);

	const CollisionShape quad = CollisionShape::triangleMesh(
		{{-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1}}, {0, 2, 1, 0, 3, 2});
	EXPECT_TRUE(quad.containsTriangleMesh());
	EXPECT_THROW(RigidBodyComponent{quad}, std::invalid_argument);	// dynamic mesh
	EXPECT_NO_THROW(RigidBodyComponent(quad, motion(MotionType::Static)));
	EXPECT_THROW(
		RigidBodyComponent(CollisionShape::compound({{quad}, {CollisionShape::sphere(1)}})),
		std::invalid_argument);

	RigidBodySettings negative;
	negative.friction = -1;
	EXPECT_THROW(RigidBodyComponent(CollisionShape::sphere(1), negative), std::invalid_argument);
}

TEST_F(PhysicsTest, OnlyOneServerAtATime) {
	EXPECT_EQ(PhysicsServer::get(), &server);
	EXPECT_THROW(PhysicsServer second, std::logic_error);
}

// ------------------------------------------------------- world resolution

TEST_F(PhysicsTest, ChildScenesShareTheNearestWorld) {
	auto childOwned = _own_mk<Scene>();
	Scene* child = childOwned.get();
	root->addChild(std::move(childOwned));
	EXPECT_EQ(child->getPhysicsWorld(), root->getPhysicsWorld());
	EXPECT_FALSE(child->ownsPhysicsWorld());

	addFloor();
	Body ball = add(*child, CollisionShape::sphere(0.5f), {0, 3, 0});
	run(120);
	EXPECT_NEAR(ball.object->getWorldPosition().y, 1.5f, 0.05f);  // lands on root's floor
}

TEST_F(PhysicsTest, IsolatedChildWorldsDoNotInteract) {
	auto isolatedOwned = _own_mk<Scene>();
	Scene* isolated = isolatedOwned.get();
	PhysicsWorld* own = isolated->createPhysicsWorld();
	root->addChild(std::move(isolatedOwned));
	EXPECT_NE(own, root->getPhysicsWorld());
	EXPECT_THROW(isolated->createPhysicsWorld(), std::logic_error);
	EXPECT_EQ(server.getWorlds().size(), 2u);

	addFloor();
	Body ball = add(*isolated, CollisionShape::sphere(0.5f), {0, 3, 0});
	run(60);
	EXPECT_LT(ball.object->getWorldPosition().y, 0.0f);	 // fell through: no floor there

	// Moving the object rebuilds its body in the destination world.
	const size_t rootBodies = root->getPhysicsWorld()->getBodyCount();
	_own<WorldObject> moved = isolated->detachChild(ball.object);
	EXPECT_EQ(own->getBodyCount(), 0u);
	moved->setWorldPosition({0, 3, 0});
	root->addChild(std::move(moved));
	EXPECT_EQ(root->getPhysicsWorld()->getBodyCount(), rootBodies + 1);
	run(120);
	EXPECT_NEAR(ball.object->getWorldPosition().y, 1.5f, 0.05f);
}

TEST_F(PhysicsTest, DetachingAStrangerThrows) {
	WorldObject stranger;
	EXPECT_THROW(root->detachChild(&stranger), std::invalid_argument);
}

// --------------------------------------------------------- contacts/triggers

TEST_F(PhysicsTest, ContactBeginAndEndFireOncePerPairOnTheMainThread) {
	Body floor = addFloor();
	Body ball = add(CollisionShape::sphere(0.5f), {0, 3, 0});
	int ballBegins = 0, floorBegins = 0, ballEnds = 0;
	Vector3 normal(0.0f);
	bool mainThread = true;
	const auto mainId = std::this_thread::get_id();
	ball.body->setOnContactBegin([&](const ContactEvent& e) {
		ballBegins++;
		normal = e.normal;
		EXPECT_EQ(e.self, ball.body);
		EXPECT_EQ(e.other, floor.body);
		mainThread = mainThread && std::this_thread::get_id() == mainId;
	});
	ball.body->setOnContactEnd([&](const ContactEvent&) { ballEnds++; });
	floor.body->setOnContactBegin([&](const ContactEvent& e) {
		floorBegins++;
		EXPECT_EQ(e.other, ball.body);
	});

	run(180);  // lands, bounces a little, rests
	EXPECT_EQ(ballBegins, 1);
	EXPECT_EQ(floorBegins, 1);
	EXPECT_EQ(ballEnds, 0);
	EXPECT_TRUE(mainThread);
	EXPECT_NEAR(normal.y, -1.0f, 0.01f);  // from the ball towards the floor

	// Falling asleep and waking up while still touching is not a new contact.
	ASSERT_TRUE(ball.body->isSleeping());
	ball.body->wake();
	run(5);
	EXPECT_EQ(ballBegins, 1);
	EXPECT_EQ(ballEnds, 0);

	ball.body->teleport({0, 10, 0}, Quaternion(1, 0, 0, 0));
	run(2);
	EXPECT_EQ(ballEnds, 1);
}

namespace {
struct TriggerScript : ScriptComponent {
	int enters = 0;
	int exits = 0;
	void onTriggerEnter(const ContactEvent&) override { enters++; }
	void onTriggerExit(const ContactEvent&) override { exits++; }
};
}  // namespace

TEST_F(PhysicsTest, SensorsReportTriggersToCallbacksAndScripts) {
	RigidBodySettings sensor = motion(MotionType::Static);
	sensor.isSensor = true;
	Body zone = add(CollisionShape::box({2, 0.5f, 2}), {0, 0, 0}, sensor);
	int enters = 0, exits = 0, contacts = 0;
	zone.body->setOnTriggerEnter([&](const ContactEvent&) { enters++; });
	zone.body->setOnTriggerExit([&](const ContactEvent&) { exits++; });
	zone.body->setOnContactBegin([&](const ContactEvent&) { contacts++; });

	auto ballObject = _own_mk<WorldObject>();
	auto script = _own_mk<TriggerScript>();
	TriggerScript* scriptRaw = script.get();
	ballObject->addComponent(std::move(script));
	ballObject->addComponent(_own_mk<RigidBodyComponent>(CollisionShape::sphere(0.25f)));
	ballObject->setWorldPosition({0, 2, 0});
	WorldObject* ball = ballObject.get();
	root->addChild(std::move(ballObject));

	run(90);  // falls straight through the sensor
	EXPECT_LT(ball->getWorldPosition().y, -1.0f);
	EXPECT_EQ(enters, 1);
	EXPECT_EQ(exits, 1);
	EXPECT_EQ(contacts, 0);
	EXPECT_EQ(scriptRaw->enters, 1);
	EXPECT_EQ(scriptRaw->exits, 1);
}

TEST_F(PhysicsTest, DestroyingABodyInsideItsCallbackIsSafe) {
	RigidBodySettings sensor = motion(MotionType::Static);
	sensor.isSensor = true;
	Body zone = add(CollisionShape::box({2, 0.5f, 2}), {0, 0, 0}, sensor);
	Body coin = add(CollisionShape::sphere(0.25f), {0, 2, 0});
	int exits = 0;
	zone.body->setOnTriggerExit([&](const ContactEvent& e) {
		exits++;
		EXPECT_EQ(e.other, coin.body);	// still valid during the callback
	});
	coin.body->setOnTriggerEnter([&](const ContactEvent&) { root->removeChild(coin.object); });

	run(60);
	EXPECT_EQ(exits, 1);  // removal ends the pair
	EXPECT_EQ(world().getBodyCount(), 1u);
}

// ------------------------------------------------------------------ queries

TEST_F(PhysicsTest, RaycastHitsMissesAndFiltersByLayer) {
	Body floor = addFloor();
	auto hit = world().raycast({0, 10, 0}, {0, -2, 0}, 100);  // direction normalized
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->body, floor.body);
	EXPECT_NEAR(hit->distance, 9.0f, 1e-3f);
	EXPECT_NEAR(hit->point.y, 1.0f, 1e-3f);
	EXPECT_NEAR(hit->normal.y, 1.0f, 1e-3f);

	EXPECT_FALSE(world().raycast({0, 10, 0}, {0, 1, 0}, 100).has_value());
	EXPECT_FALSE(world().raycast({0, 10, 0}, {0, -1, 0}, 5).has_value());	// too short
	EXPECT_FALSE(world().raycast({0, 10, 0}, {0, -1, 0}, 100, physicsLayer(3)).has_value());
	EXPECT_FALSE(world().raycast({0, 10, 0}, {0, -1, 0}, 100, kAllPhysicsLayers, floor.body));
	EXPECT_THROW(world().raycast({0, 0, 0}, {0, 0, 0}, 1), std::invalid_argument);
}

TEST_F(PhysicsTest, RaycastAllReturnsEachBodyNearestFirst) {
	RigidBodySettings still = motion(MotionType::Static);
	Body low = add(CollisionShape::box({1, 0.5f, 1}), {0, 0, 0}, still);
	Body high = add(CollisionShape::box({1, 0.5f, 1}), {0, 3, 0}, still);
	auto hits = world().raycastAll({0, 10, 0}, {0, -1, 0}, 20);
	ASSERT_EQ(hits.size(), 2u);
	EXPECT_EQ(hits[0].body, high.body);
	EXPECT_EQ(hits[1].body, low.body);
	EXPECT_LT(hits[0].distance, hits[1].distance);
}

TEST_F(PhysicsTest, OverlapAndShapeCast) {
	RigidBodySettings still = motion(MotionType::Static);
	Body a = add(CollisionShape::sphere(0.5f), {0.8f, 0, 0}, still);
	Body b = add(CollisionShape::sphere(0.5f), {-0.8f, 0, 0}, still);
	add(CollisionShape::sphere(0.5f), {5, 0, 0}, still);
	auto found = world().overlap(CollisionShape::sphere(1), {0, 0, 0}, Quaternion(1, 0, 0, 0));
	ASSERT_EQ(found.size(), 2u);
	EXPECT_TRUE((found[0] == a.body && found[1] == b.body) ||
				(found[0] == b.body && found[1] == a.body));

	Body floor = addFloor();
	auto cast = world().shapeCast(CollisionShape::sphere(1), {20, 11, 0},
								  Quaternion(1, 0, 0, 0), {20, -9, 0});
	ASSERT_TRUE(cast.has_value());
	EXPECT_EQ(cast->body, floor.body);
	EXPECT_NEAR(cast->fraction, 9.0f / 20.0f, 0.01f);  // stops 1 m above y = 1
	EXPECT_NEAR(cast->normal.y, 1.0f, 0.01f);
}

TEST_F(PhysicsTest, BallRestsOnStaticTriangleMesh) {
	add(CollisionShape::triangleMesh({{-5, 0, -5}, {5, 0, -5}, {5, 0, 5}, {-5, 0, 5}},
									 {0, 2, 1, 0, 3, 2}),
		{0, 0, 0}, motion(MotionType::Static));
	Body ball = add(CollisionShape::sphere(0.5f), {0, 3, 0});
	run(180);
	EXPECT_NEAR(ball.object->getWorldPosition().y, 0.5f, 0.05f);
}

TEST_F(PhysicsTest, LayerMasksDecideWhoCollides) {
	RigidBodySettings ghost;
	ghost.layers = physicsLayer(1);
	ghost.mask = physicsLayer(1);  // ignores the floor on layer 0
	addFloor();
	Body ball = add(CollisionShape::sphere(0.5f), {0, 3, 0}, ghost);
	run(90);
	EXPECT_LT(ball.object->getWorldPosition().y, 0.0f);
	EXPECT_THROW(physicsLayer(7), std::out_of_range);
}

// ---------------------------------------------------------------- the clock

TEST(FixedStepClock, StepsAccumulateAndClamp) {
	FixedStepClock clock(1.0f / 60.0f, 8, 0.25f);
	int total = 0;
	for (int i = 0; i < 60; i++) total += clock.advance(1.0 / 60.0);
	EXPECT_EQ(total, 60);  // exact frames never round down to 0 steps

	FixedStepClock odd(0.01f);
	total = 0;
	for (int i = 0; i < 100; i++) total += odd.advance(0.007);
	EXPECT_EQ(total, 70);
	EXPECT_NEAR(odd.getAccumulator(), 0.0, 1e-6);

	FixedStepClock slow(1.0f / 60.0f, 8, 0.25f);
	EXPECT_EQ(slow.advance(10.0), 8);  // 0.25 s = 15 steps, clamped to 8
	EXPECT_DOUBLE_EQ(slow.getAccumulator(), 0.0);
	EXPECT_EQ(slow.advance(-1.0), 0);
	EXPECT_THROW(FixedStepClock(0.0f), std::invalid_argument);
}

// ------------------------------------------------------------- characters

#include "physics/CharacterControllerComponent.h"

namespace {

class CharacterTest : public PhysicsTest {
   protected:
	struct Walker {
		WorldObject* object;
		CharacterControllerComponent* character;
	};

	Walker addCharacter(const Vector3& feet, CharacterSettings settings = {}) {
		auto object = _own_mk<WorldObject>();
		auto component = _own_mk<CharacterControllerComponent>(settings);
		CharacterControllerComponent* character = component.get();
		object->addComponent(std::move(component));
		object->setWorldPosition(feet);
		WorldObject* raw = object.get();
		root->addChild(std::move(object));
		return {raw, character};
	}

	Body addStaticBox(const Vector3& halfExtents, const Vector3& center,
					  const Quaternion& rotation = Quaternion(1, 0, 0, 0)) {
		Body box = add(CollisionShape::box(halfExtents), center, motion(MotionType::Static));
		box.body->teleport(center, rotation);
		return box;
	}
};

}  // namespace

TEST_F(CharacterTest, FallsAndLandsOnItsFeet) {
	addFloor();	 // top at y = 1
	Walker w = addCharacter({0, 4, 0});
	EXPECT_EQ(world().getCharacterCount(), 1u);
	run(1);
	EXPECT_EQ(w.character->getGroundState(), GroundState::InAir);
	run(120);
	EXPECT_EQ(w.character->getGroundState(), GroundState::OnGround);
	EXPECT_NEAR(w.object->getWorldPosition().y, 1.0f, 0.05f);	// feet on the floor
	EXPECT_NEAR(w.character->getGroundNormal().y, 1.0f, 1e-3f);
}

TEST_F(CharacterTest, WalksAtTheMoveVelocityAndStopsAtWalls) {
	addFloor();
	Walker w = addCharacter({0, 1, 0});
	run(10);
	w.character->setMoveVelocity({3, 5, 0});  // vertical part ignored
	run(60);
	EXPECT_NEAR(w.object->getWorldPosition().x, 3.0f, 0.15f);
	EXPECT_NEAR(w.object->getWorldPosition().y, 1.0f, 0.05f);

	addStaticBox({0.5f, 2, 2}, {6, 3, 0});	// wall face at x = 5.5
	run(120);
	EXPECT_NEAR(w.object->getWorldPosition().x, 5.5f - 0.3f, 0.1f);	 // radius 0.3
}

TEST_F(CharacterTest, JumpsOnlyFromTheGround) {
	addFloor();
	Walker w = addCharacter({0, 1, 0});
	run(10);
	ASSERT_TRUE(w.character->jump(5));
	float peak = 0;
	for (int i = 0; i < 60; i++) {
		run(1);
		peak = std::max(peak, w.object->getWorldPosition().y);
		if (i == 5) EXPECT_FALSE(w.character->jump(5));	// airborne: refused
	}
	EXPECT_NEAR(peak - 1.0f, 5.0f * 5.0f / (2 * 9.81f), 0.15f);	 // v^2 / 2g
	run(60);
	EXPECT_TRUE(w.character->isOnGround());
}

TEST_F(CharacterTest, ClimbsLowStepsButNotHighOnes) {
	addFloor();
	addStaticBox({1, 0.15f, 3}, {3, 1.15f, 0});	 // 0.3 m step at x = 2
	addStaticBox({1, 0.4f, 3}, {3, 1.4f, 5});	 // 0.8 m ledge at x = 2
	Walker low = addCharacter({0, 1, 0});
	Walker high = addCharacter({0, 1, 5});
	run(10);
	low.character->setMoveVelocity({2, 0, 0});
	high.character->setMoveVelocity({2, 0, 0});
	run(90);
	EXPECT_GT(low.object->getWorldPosition().x, 2.5f);
	EXPECT_NEAR(low.object->getWorldPosition().y, 1.3f, 0.05f);
	EXPECT_LT(high.object->getWorldPosition().x, 2.0f);
}

TEST_F(CharacterTest, SteepSlopesAreNotWalkable) {
	const float angle = glm::radians(60.0f);
	addStaticBox({5, 0.5f, 5}, {0, 0, 0}, glm::angleAxis(angle, Vector3(0, 0, 1)));
	Walker w = addCharacter({0, 4, 0});
	bool steep = false, walkable = false;
	for (int i = 0; i < 120; i++) {
		run(1);
		steep = steep || w.character->getGroundState() == GroundState::OnSteepGround;
		walkable = walkable || w.character->isOnGround();
	}
	EXPECT_TRUE(steep);
	EXPECT_FALSE(walkable);
}

TEST_F(CharacterTest, PushesDynamicBodiesAndReportsThem) {
	addFloor();
	RigidBodySettings light;
	light.mass = 5.0f;	// the default density would make it 216 kg: too heavy at 100 N
	Body crate = add(CollisionShape::box({0.3f, 0.3f, 0.3f}), {1.5f, 1.3f, 0}, light);
	Walker w = addCharacter({0, 1, 0});
	run(30);
	w.character->setMoveVelocity({2, 0, 0});
	bool touched = false;
	for (int i = 0; i < 60; i++) {
		run(1);
		for (RigidBodyComponent* body : w.character->getTouchingBodies()) {
			touched = touched || body == crate.body;
		}
	}
	EXPECT_TRUE(touched);
	EXPECT_GT(crate.object->getWorldPosition().x, 2.0f);
}

TEST_F(CharacterTest, FallingBodiesAndRaycastsHitTheCharacter) {
	addFloor();
	Walker w = addCharacter({0, 1, 0});
	run(5);
	auto hit = world().raycast({0, 10, 0}, {0, -1, 0}, 20);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->object, w.object);
	EXPECT_EQ(hit->body, nullptr);
	EXPECT_NEAR(hit->point.y, 2.8f, 0.05f);	 // head at 1 + 1.8
	auto past = world().raycast({0, 10, 0}, {0, -1, 0}, 20, kAllPhysicsLayers, w.character);
	ASSERT_TRUE(past.has_value());
	EXPECT_NE(past->object, w.object);

	Body ball = add(CollisionShape::sphere(0.2f), {0, 5, 0});
	run(90);
	EXPECT_GT(ball.object->getWorldPosition().y, 2.5f);	 // resting on the head
}

TEST_F(CharacterTest, MovingTheObjectTeleportsAndRemovalCleansUp) {
	addFloor();
	Walker w = addCharacter({0, 1, 0});
	run(10);
	w.object->setWorldPosition({10, 1, 0});
	run(1);
	EXPECT_NEAR(w.object->getWorldPosition().x, 10.0f, 1e-3f);
	w.character->teleport({-10, 1, 0});
	EXPECT_TRUE(world().raycast({-10, 10, 0}, {0, -1, 0}, 20)->object == w.object);

	root->removeChild(w.object);
	EXPECT_EQ(world().getCharacterCount(), 0u);
	EXPECT_EQ(world().raycast({-10, 10, 0}, {0, -1, 0}, 20)->object,
			  world().raycast({0, 10, 0}, {0, -1, 0}, 20)->object);  // only the floor
}

TEST_F(CharacterTest, WorldDestroyedBeforeTheComponent) {
	auto isolatedOwned = _own_mk<Scene>();
	Scene* isolated = isolatedOwned.get();
	isolated->createPhysicsWorld();
	root->addChild(std::move(isolatedOwned));
	auto object = _own_mk<WorldObject>();
	auto component = _own_mk<CharacterControllerComponent>();
	CharacterControllerComponent* character = component.get();
	object->addComponent(std::move(component));
	isolated->addChild(std::move(object));
	ASSERT_TRUE(character->isInWorld());
	root.reset();  // scene, world and character all go; must not crash
	SUCCEED();
}

TEST_F(CharacterTest, InvalidSettingsAreRejected) {
	CharacterSettings tooShort;
	tooShort.height = 0.5f;	 // < 2 * radius
	EXPECT_THROW(CharacterControllerComponent{tooShort}, std::invalid_argument);
	CharacterSettings slope;
	slope.maxSlopeDegrees = 95;
	EXPECT_THROW(CharacterControllerComponent{slope}, std::invalid_argument);
	CharacterControllerComponent loose;
	EXPECT_THROW(loose.getGroundState(), std::logic_error);
}

TEST_F(PhysicsTest, SinglePartCompoundOffsetsItsShape) {
	add(CollisionShape::compound({{CollisionShape::box({1, 1, 1}), {0, 5, 0}}}), {0, 0, 0},
		motion(MotionType::Static));
	EXPECT_NEAR(world().raycast({0, 10, 0}, {0, -1, 0}, 20)->point.y, 6.0f, 1e-3f);
}
