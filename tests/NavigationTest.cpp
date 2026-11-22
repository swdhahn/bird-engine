#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "navigation/NavAgent.h"
#include "navigation/NavMesh.h"
#include "navigation/NavServer.h"
#include "navigation/NavTypes.h"
#include "navigation/NavWorld.h"
#include "renders/DebugDraw.h"
#include "scene/Scene.h"
#include "scene/WorldObject.h"

using namespace bird;

namespace {

constexpr float kStep = 1.0f / 60.0f;

/** Appends an axis-aligned box (12 triangles, outward CCW winding) to a triangle soup. */
void appendBox(std::vector<Vector3>& verts, std::vector<uint32_t>& indices,
			   const Vector3& minPt, const Vector3& maxPt) {
	const uint32_t b = static_cast<uint32_t>(verts.size());
	verts.push_back({minPt.x, minPt.y, minPt.z});  // 0
	verts.push_back({maxPt.x, minPt.y, minPt.z});  // 1
	verts.push_back({maxPt.x, maxPt.y, minPt.z});  // 2
	verts.push_back({minPt.x, maxPt.y, minPt.z});  // 3
	verts.push_back({minPt.x, minPt.y, maxPt.z});  // 4
	verts.push_back({maxPt.x, minPt.y, maxPt.z});  // 5
	verts.push_back({maxPt.x, maxPt.y, maxPt.z});  // 6
	verts.push_back({minPt.x, maxPt.y, maxPt.z});  // 7

	const uint32_t boxTris[] = {
		// Top (+Y)
		3, 7, 6, 3, 6, 2,
		// Bottom (-Y)
		0, 1, 5, 0, 5, 4,
		// Front (+Z)
		4, 5, 6, 4, 6, 7,
		// Back (-Z)
		1, 0, 3, 1, 3, 2,
		// Right (+X)
		5, 1, 2, 5, 2, 6,
		// Left (-X)
		0, 4, 7, 0, 7, 3,
	};
	for (uint32_t idx : boxTris) {
		indices.push_back(b + idx);
	}
}

/** Builds a simple 40x40 m flat floor slab with top face at y = 0. */
_ref<NavMesh> buildFlatFloorNavMesh(const NavMeshBuildSettings& settings = {}) {
	std::vector<Vector3> verts;
	std::vector<uint32_t> indices;
	appendBox(verts, indices, {-20.0f, -1.0f, -20.0f}, {20.0f, 0.0f, 20.0f});
	return NavMesh::build(verts, indices, settings);
}

/**
 * Builds a 40x40 m floor with a tall central wall at x in [-1, 1], z in [-12, 12],
 * forcing agents travelling from (-10, 0, 0) to (10, 0, 0) to route around z = +/-12.
 */
_ref<NavMesh> buildWalledFloorNavMesh(const NavMeshBuildSettings& settings = {}) {
	std::vector<Vector3> verts;
	std::vector<uint32_t> indices;
	appendBox(verts, indices, {-20.0f, -1.0f, -20.0f}, {20.0f, 0.0f, 20.0f});
	appendBox(verts, indices, {-1.0f, 0.0f, -12.0f}, {1.0f, 4.0f, 12.0f});
	return NavMesh::build(verts, indices, settings);
}

/** Headless navigation fixture: NavServer first so Scene/NavWorld die before it. */
class NavigationTest : public ::testing::Test {
   protected:
	NavServer server;
	_own<Scene> root = _own_mk<Scene>();

	struct AgentEntity {
		WorldObject* object;
		NavAgent* agent;
	};

	AgentEntity addAgent(Scene& scene, const Vector3& position,
						 const NavAgentSettings& settings = {}) {
		auto obj = _own_mk<WorldObject>();
		auto comp = _own_mk<NavAgent>(settings);
		NavAgent* rawAgent = comp.get();
		obj->setWorldPosition(position);
		obj->addComponent(std::move(comp));
		WorldObject* rawObj = obj.get();
		scene.addChild(std::move(obj));
		return {rawObj, rawAgent};
	}

	AgentEntity addAgent(const Vector3& position,
						 const NavAgentSettings& settings = {}) {
		return addAgent(*root, position, settings);
	}

	void stepFrames(int count, float dt = kStep) {
		for (int i = 0; i < count; ++i) {
			server.step(dt);
		}
	}
};

}  // namespace

// ------------------------------------------------------------- Recast baking

TEST(NavMeshBuildTest, BakesFlatFloorAndValidatesGeometry) {
	auto navMesh = buildFlatFloorNavMesh();
	ASSERT_NE(navMesh, nullptr);
	EXPECT_TRUE(navMesh->isValid());
	EXPECT_GE(navMesh->getTileCount(), 1);
	EXPECT_GE(navMesh->getPolyCount(), 1);
}

TEST(NavMeshBuildTest, RejectsInvalidInputsAndSettings) {
	std::vector<Vector3> verts;
	std::vector<uint32_t> indices;
	EXPECT_THROW(NavMesh::build(verts, indices), std::invalid_argument);

	appendBox(verts, indices, {-5, -1, -5}, {5, 0, 5});

	NavMeshBuildSettings badCell;
	badCell.cellSize = 0.0f;
	EXPECT_THROW(NavMesh::build(verts, indices, badCell), std::invalid_argument);

	NavMeshBuildSettings badSlope;
	badSlope.agentMaxSlope = 95.0f;
	EXPECT_THROW(NavMesh::build(verts, indices, badSlope), std::invalid_argument);

	NavMeshBuildSettings badVerts;
	badVerts.vertsPerPoly = 7;
	EXPECT_THROW(NavMesh::build(verts, indices, badVerts), std::invalid_argument);

	// Bad index count
	indices.pop_back();
	EXPECT_THROW(NavMesh::build(verts, indices), std::invalid_argument);
}

TEST(NavMeshBuildTest, HonorsAgentMaxClimbOnStairs) {
	// Two platforms separated by a 0.4 m vertical step.
	std::vector<Vector3> verts;
	std::vector<uint32_t> indices;
	appendBox(verts, indices, {-10.0f, -1.0f, -5.0f}, {0.0f, 0.0f, 5.0f});
	appendBox(verts, indices, {0.0f, -1.0f, -5.0f}, {10.0f, 0.4f, 5.0f});

	// Climbable with agentMaxClimb = 0.6 m
	NavMeshBuildSettings climbable;
	climbable.cellSize = 0.2f;
	climbable.cellHeight = 0.1f;
	climbable.agentMaxClimb = 0.6f;
	auto meshClimb = NavMesh::build(verts, indices, climbable);
	NavPath okPath = meshClimb->findPath({-5.0f, 0.0f, 0.0f}, {5.0f, 0.4f, 0.0f});
	EXPECT_TRUE(okPath.isValid());
	EXPECT_FALSE(okPath.isPartial());

	// Unclimbable with agentMaxClimb = 0.15 m
	NavMeshBuildSettings tooHigh = climbable;
	tooHigh.agentMaxClimb = 0.15f;
	auto meshBlocked = NavMesh::build(verts, indices, tooHigh);
	NavPath blockedPath =
		meshBlocked->findPath({-5.0f, 0.0f, 0.0f}, {5.0f, 0.4f, 0.0f});
	EXPECT_TRUE(!blockedPath.isValid() || blockedPath.isPartial());
}

// ------------------------------------------------------------- Serialization

TEST(NavMeshSerializationTest, RoundTripBinaryBufferAndFile) {
	NavMeshBuildSettings settings;
	settings.agentRadius = 0.5f;
	settings.agentHeight = 1.9f;
	settings.agentMaxClimb = 0.45f;
	settings.agentMaxSlope = 40.0f;

	auto original = buildWalledFloorNavMesh(settings);
	ASSERT_TRUE(original->isValid());

	// In-memory serialize / deserialize
	std::vector<uint8_t> bytes = original->serialize();
	ASSERT_GT(bytes.size(), 64u);

	auto restored = NavMesh::deserialize(bytes.data(), bytes.size());
	ASSERT_NE(restored, nullptr);
	EXPECT_TRUE(restored->isValid());
	EXPECT_EQ(restored->getTileCount(), original->getTileCount());
	EXPECT_EQ(restored->getPolyCount(), original->getPolyCount());
	EXPECT_FLOAT_EQ(restored->getBuildSettings().agentRadius, 0.5f);
	EXPECT_FLOAT_EQ(restored->getBuildSettings().agentHeight, 1.9f);
	EXPECT_FLOAT_EQ(restored->getBuildSettings().agentMaxClimb, 0.45f);
	EXPECT_FLOAT_EQ(restored->getBuildSettings().agentMaxSlope, 40.0f);

	// File saveToFile / loadFromFile
	const auto tmpPath =
		std::filesystem::temp_directory_path() / "bird_navmesh_roundtrip_test.bnav";
	ASSERT_TRUE(original->saveToFile(tmpPath.string()));
	auto fromFile = NavMesh::loadFromFile(tmpPath.string());
	std::filesystem::remove(tmpPath);

	ASSERT_NE(fromFile, nullptr);
	EXPECT_TRUE(fromFile->isValid());
	EXPECT_EQ(fromFile->getPolyCount(), original->getPolyCount());

	// Identical pathfinding query results
	NavPath p1 = original->findPath({-10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	NavPath p2 = fromFile->findPath({-10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	ASSERT_TRUE(p1.isValid());
	ASSERT_TRUE(p2.isValid());
	EXPECT_EQ(p1.getPointCount(), p2.getPointCount());
	EXPECT_NEAR(p1.getLength(), p2.getLength(), 1e-3f);

	// Corrupt / truncated buffers return nullptr safely
	EXPECT_EQ(NavMesh::deserialize(nullptr, 0), nullptr);
	EXPECT_EQ(NavMesh::deserialize(bytes.data(), 12), nullptr);
	bytes[0] ^= 0xFF;  // corrupt magic
	EXPECT_EQ(NavMesh::deserialize(bytes.data(), bytes.size()), nullptr);
}

// ------------------------------------------------------------- Detour queries

TEST(NavMeshQueryTest, FindNearestPointAndFindPathAroundWall) {
	auto navMesh = buildWalledFloorNavMesh();

	// Nearest point snaps a point hovering above the floor onto y = 0.
	auto snapped = navMesh->findNearestPoint({-8.0f, 2.0f, 3.0f});
	ASSERT_TRUE(snapped.has_value());
	EXPECT_NEAR(snapped->position.x, -8.0f, 0.3f);
	EXPECT_NEAR(snapped->position.y, 0.0f, 0.25f);
	EXPECT_NEAR(snapped->position.z, 3.0f, 0.3f);
	EXPECT_NE(snapped->polyRef, 0u);

	// Far outside the floor returns nullopt.
	EXPECT_FALSE(navMesh->findNearestPoint({200.0f, 0.0f, 200.0f}).has_value());

	// Path around the central wall (z in [-12, 12]) must detour past |z| > 11.
	NavPath path = navMesh->findPath({-10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	ASSERT_TRUE(path.isValid());
	EXPECT_FALSE(path.isPartial());
	EXPECT_GT(path.getPointCount(), 2u);
	EXPECT_GT(path.getLength(), 26.0f);  // straight line would be 20 m

	float maxAbsZ = 0.0f;
	for (size_t i = 0; i < path.getPointCount(); ++i) {
		maxAbsZ = std::max(maxAbsZ, std::abs(path.getPoint(i).z));
	}
	EXPECT_GT(maxAbsZ, 11.0f);
}

// ------------------------------------------------------------- NavAgent & Crowd

TEST_F(NavigationTest, AgentNavigatesAroundWallAndReachesDestination) {
	root->getNavWorld()->setNavMesh(buildWalledFloorNavMesh());

	NavAgentSettings settings;
	settings.maxSpeed = 5.0f;
	settings.maxAcceleration = 12.0f;
	settings.stoppingDistance = 0.5f;

	AgentEntity npc = addAgent({-10.0f, 0.0f, 0.0f}, settings);
	EXPECT_EQ(root->getNavWorld()->getAgentCount(), 1u);
	EXPECT_FALSE(npc.agent->hasDestination());
	EXPECT_FALSE(npc.agent->hasReachedDestination());

	const Vector3 goal(10.0f, 0.0f, 0.0f);
	ASSERT_TRUE(npc.agent->setDestination(goal));
	EXPECT_TRUE(npc.agent->hasDestination());
	EXPECT_FALSE(npc.agent->hasReachedDestination());

	// Simulate up to 12 seconds at 60 Hz (path is ~32 m at 5 m/s => ~6.5-8 s)
	for (int i = 0; i < 720 && !npc.agent->hasReachedDestination(); ++i) {
		server.step(kStep);
	}

	EXPECT_TRUE(npc.agent->hasReachedDestination());
	EXPECT_FALSE(npc.agent->hasDestination());
	const Vector3 finalPos = npc.object->getWorldPosition();
	const float distXZ = std::hypot(finalPos.x - goal.x, finalPos.z - goal.z);
	EXPECT_LE(distXZ, settings.stoppingDistance + 0.1f);
}

TEST_F(NavigationTest, StopHaltsAgentImmediately) {
	root->getNavWorld()->setNavMesh(buildFlatFloorNavMesh());

	NavAgentSettings settings;
	settings.maxSpeed = 4.0f;
	settings.maxAcceleration = 10.0f;
	AgentEntity npc = addAgent({-12.0f, 0.0f, 0.0f}, settings);

	ASSERT_TRUE(npc.agent->setDestination({12.0f, 0.0f, 0.0f}));
	stepFrames(60);  // 1 second of movement
	EXPECT_GT(npc.object->getWorldPosition().x, -11.0f);
	EXPECT_TRUE(npc.agent->isMoving());

	npc.agent->stop();
	EXPECT_FALSE(npc.agent->hasDestination());
	EXPECT_FALSE(npc.agent->hasReachedDestination());
	EXPECT_FALSE(npc.agent->isMoving());

	const Vector3 stoppedPos = npc.object->getWorldPosition();
	stepFrames(30);
	EXPECT_NEAR(glm::length(npc.object->getWorldPosition() - stoppedPos), 0.0f, 1e-3f);
}

TEST_F(NavigationTest, ExternalTeleportSyncsCrowdAgent) {
	root->getNavWorld()->setNavMesh(buildFlatFloorNavMesh());

	AgentEntity npc = addAgent({-10.0f, 0.0f, 0.0f});
	stepFrames(5);

	// Teleport the entity manually
	npc.object->setWorldPosition({8.0f, 0.0f, 5.0f});
	stepFrames(5);

	const Vector3 pos = npc.object->getWorldPosition();
	EXPECT_NEAR(pos.x, 8.0f, 0.3f);
	EXPECT_NEAR(pos.z, 5.0f, 0.3f);
}

TEST_F(NavigationTest, PendingDestinationAppliesOnceNavMeshIsAssigned) {
	AgentEntity npc = addAgent({-5.0f, 0.0f, 0.0f});
	ASSERT_TRUE(npc.agent->setDestination({5.0f, 0.0f, 0.0f}));
	stepFrames(10);  // No navmesh yet; should not crash or move
	EXPECT_NEAR(npc.object->getWorldPosition().x, -5.0f, 1e-3f);

	// Assign NavMesh now; queued destination should activate automatically
	root->getNavWorld()->setNavMesh(buildFlatFloorNavMesh());
	for (int i = 0; i < 360 && !npc.agent->hasReachedDestination(); ++i) {
		server.step(kStep);
	}
	EXPECT_TRUE(npc.agent->hasReachedDestination());
	EXPECT_NEAR(npc.object->getWorldPosition().x, 5.0f, 0.5f);
}

TEST_F(NavigationTest, IsolatedSubSceneNavWorld) {
	root->getNavWorld()->setNavMesh(buildFlatFloorNavMesh());

	auto sub = _own_mk<Scene>();
	Scene* rawSub = sub.get();
	root->addChild(std::move(sub));

	NavWorld* subWorld = rawSub->createNavWorld();
	EXPECT_NE(subWorld, root->getNavWorld());
	EXPECT_TRUE(rawSub->ownsNavWorld());
	EXPECT_EQ(server.getWorlds().size(), 2u);

	subWorld->setNavMesh(buildFlatFloorNavMesh());
	AgentEntity a1 = addAgent(*root, {-2.0f, 0.0f, 0.0f});
	AgentEntity a2 = addAgent(*rawSub, {2.0f, 0.0f, 0.0f});
	EXPECT_EQ(root->getNavWorld()->getAgentCount(), 1u);
	EXPECT_EQ(subWorld->getAgentCount(), 1u);
	EXPECT_EQ(a1.agent->getWorld(), root->getNavWorld());
	EXPECT_EQ(a2.agent->getWorld(), subWorld);
}

// ------------------------------------------------------------- Debug Draw

TEST_F(NavigationTest, DebugDrawCachesNavMeshAndDrawsAgentPaths) {
	auto navMesh = buildFlatFloorNavMesh();
	root->getNavWorld()->setNavMesh(navMesh);

	AgentEntity npc = addAgent({-8.0f, 0.0f, 0.0f});
	ASSERT_TRUE(npc.agent->setDestination({8.0f, 0.0f, 0.0f}));
	stepFrames(5);

	DebugDrawList debugDraw;
	NavDebugSettings dbgSettings;
	root->getNavWorld()->debugDraw(debugDraw, dbgSettings);

	// Cached layer for NavMesh + transient lines for agent bounds & path
	EXPECT_FALSE(debugDraw.empty());
	EXPECT_GT(debugDraw.lineCount(), 10u);
	ASSERT_EQ(debugDraw.cachedLayers().size(), 1u);
	const uint64_t cachedVersion = debugDraw.cachedLayers().begin()->second.version;
	EXPECT_NE(cachedVersion, 0u);
	EXPECT_GT(debugDraw.vertices().size(), 0u);

	// Next frame: cached layer stays intact with same version
	debugDraw.clear();
	root->getNavWorld()->debugDraw(debugDraw, dbgSettings);
	ASSERT_EQ(debugDraw.cachedLayers().size(), 1u);
	EXPECT_EQ(debugDraw.cachedLayers().begin()->second.version, cachedVersion);

	// Changing a color setting invalidates and updates the cached layer version
	debugDraw.clear();
	dbgSettings.navMeshColor = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
	root->getNavWorld()->debugDraw(debugDraw, dbgSettings);
	EXPECT_NE(debugDraw.cachedLayers().begin()->second.version, cachedVersion);
}
