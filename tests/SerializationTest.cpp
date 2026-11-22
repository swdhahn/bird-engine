#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>

#include "scene/Camera.h"
#include "scene/Scene.h"
#include "scene/WorldObject.h"
#include "scene/components/LightComponent.h"
#include "scene/components/MeshComponent.h"
#include "serialization/BuiltinComponents.h"
#include "serialization/ComponentRegistry.h"
#include "serialization/DartScriptRecord.h"
#include "serialization/Json.h"
#include "serialization/JsonArchive.h"
#include "serialization/Prefab.h"
#include "serialization/PrefabInstanceComponent.h"
#include "serialization/PrefabManager.h"
#include "serialization/PropertyOverride.h"
#include "serialization/SaveGame.h"
#include "serialization/SceneSerializer.h"
#include "serialization/UUID.h"

using namespace bird;

namespace {

constexpr float kEps = 1e-4f;

// ------------------------------------------------ Test Components & Fixtures

enum class AIState : int32_t { Idle = 0, Patrol = 1, Combat = 2 };

/** Custom gameplay component exercising reflected scalars, math, enum, and EntityRef. */
class TestAgentComponent : public Component {
   public:
	int32_t health = 100;
	float speed = 3.5f;
	bool active = true;
	std::string tag = "grunt";
	Vector3 homeOffset{1.0f, 2.0f, 3.0f};
	AIState state = AIState::Idle;
	EntityRef targetEntity;
	EntityRef externalAnchor;

	// Lifecycle telemetry verified by prefab/override tests
	int initCount = 0;
	int deinitCount = 0;
	int32_t healthSeenAtInit = -1;
	Vector3 worldPosSeenAtInit{0.0f};
	UUID targetUuidSeenAtInit = UUID::nil();

	void init() override {
		++initCount;
		healthSeenAtInit = health;
		targetUuidSeenAtInit = targetEntity.uuid;
		if (world_object()) {
			worldPosSeenAtInit = world_object()->getWorldPosition();
		}
	}

	void deinit() override { ++deinitCount; }
};

void ensureTestComponentsRegistered() {
	auto& reg = ComponentRegistry::instance();
	if (!reg.isRegistered("TestAgentComponent")) {
		reg.registerComponent<TestAgentComponent>(
			"TestAgentComponent",
			[](ComponentMetaBuilder<TestAgentComponent>& b) {
				b.field("health", &TestAgentComponent::health);
				b.field("speed", &TestAgentComponent::speed);
				b.field("active", &TestAgentComponent::active);
				b.field("tag", &TestAgentComponent::tag);
				b.field("homeOffset", &TestAgentComponent::homeOffset);
				b.enumField<AIState>(
					"state",
					[](const TestAgentComponent& c) { return c.state; },
					[](TestAgentComponent& c, AIState s) { c.state = s; });
				b.field("targetEntity", &TestAgentComponent::targetEntity);
				b.field("externalAnchor", &TestAgentComponent::externalAnchor);
			});
	}
}

template <typename T>
T* findComponent(WorldObject* obj) {
	if (!obj) return nullptr;
	for (auto& c : obj->getComponents()) {
		if (auto* cast = dynamic_cast<T*>(c.get())) return cast;
	}
	return nullptr;
}

class SerializationTest : public ::testing::Test {
   protected:
	void SetUp() override {
		registerBuiltinComponents();
		ensureTestComponentsRegistered();
		PrefabManager::get()->clear();
		DartScriptRecord::setScriptFactory(nullptr);
	}

	void TearDown() override {
		PrefabManager::get()->clear();
		DartScriptRecord::setScriptFactory(nullptr);
	}
};

}  // namespace

// ===================================================================== UUID ==

TEST_F(SerializationTest, UuidGenerationFormatAndUniqueness) {
	std::set<std::string> seen;
	for (int i = 0; i < 500; ++i) {
		UUID id = UUID::generate();
		EXPECT_FALSE(id.isNil());
		std::string s = id.toString();
		ASSERT_EQ(s.size(), 36u);
		EXPECT_EQ(s[8], '-');
		EXPECT_EQ(s[13], '-');
		EXPECT_EQ(s[14], '4');  // RFC 4122 version 4
		EXPECT_EQ(s[18], '-');
		EXPECT_TRUE(s[19] == '8' || s[19] == '9' || s[19] == 'a' ||
					s[19] == 'b');  // RFC 4122 variant 1
		EXPECT_EQ(s[23], '-');

		UUID parsed = UUID::fromString(s);
		EXPECT_EQ(parsed, id);
		EXPECT_TRUE(seen.insert(s).second);
	}
}

TEST_F(SerializationTest, UuidParseValidationAndHash) {
	const std::string upper = "550E8400-E29B-41D4-A716-446655440000";
	auto parsed = UUID::tryParse(upper);
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ(parsed->toString(), "550e8400-e29b-41d4-a716-446655440000");

	EXPECT_FALSE(UUID::tryParse("").has_value());
	EXPECT_FALSE(UUID::tryParse("not-a-uuid").has_value());
	EXPECT_FALSE(UUID::tryParse("550e8400e29b41d4a716446655440000").has_value());
	EXPECT_FALSE(
		UUID::tryParse("550e8400-e29b-41d4-a716-44665544000g").has_value());
	EXPECT_THROW(UUID::fromString("bad"), std::invalid_argument);

	std::unordered_map<UUID, int> map;
	map[*parsed] = 42;
	EXPECT_EQ(map[*parsed], 42);
}

// ===================================================================== JSON ==

TEST_F(SerializationTest, JsonRoundTripEscapesAndErrors) {
	JsonValue root = JsonValue::object();
	root.set("nullVal", nullptr);
	root.set("boolVal", true);
	root.set("intVal", int64_t{-9876543210LL});
	root.set("doubleVal", 3.1415926535);
	root.set("escaped", std::string("line1\nline2\t\"quoted\"\\slash"));
	JsonValue arr = JsonValue::array();
	arr.pushBack(1);
	arr.pushBack(2.5);
	arr.pushBack("three");
	root.set("arr", std::move(arr));

	const std::string pretty = root.dump(2);
	std::string err;
	auto parsed = JsonValue::parse(pretty, &err);
	ASSERT_TRUE(parsed.has_value()) << err;
	EXPECT_EQ(*parsed, root);

	const std::string compact = root.dump(-1);
	auto parsedCompact = JsonValue::parse(compact, &err);
	ASSERT_TRUE(parsedCompact.has_value()) << err;
	EXPECT_EQ(*parsedCompact, root);

	// Unicode surrogate pair (\uD83D\uDE80 = rocket emoji UTF-8)
	auto emoji = JsonValue::parse("\"\\uD83D\\uDE80\"", &err);
	ASSERT_TRUE(emoji.has_value()) << err;
	EXPECT_EQ(emoji->asString(), "\xF0\x9F\x9A\x80");

	// Syntax error reporting
	std::string parseErr;
	EXPECT_FALSE(JsonValue::parse("{\"a\": [1, 2,]}", &parseErr).has_value());
	EXPECT_FALSE(parseErr.empty());
}

// ====================================================== Scene Round-Trip =====

TEST_F(SerializationTest, RoundTripSceneSaveAndLoad) {
	auto root = _own_mk<Scene>();
	root->setName("MainLevel");
	const UUID rootUuid = root->getUUID();

	// 1. Camera child with custom WorldEnvironment
	auto cam = _own_mk<Camera>(1280, 720);
	cam->setName("MainCamera");
	cam->setWorldPosition(Vector3(0.0f, 5.0f, 12.0f));
	cam->setWorldRotation(fromAngleAxis(0.5f, Vector3(0.0f, 1.0f, 0.0f)));
	WorldEnvironment env;
	env.clearColor = Vector3(0.1f, 0.2f, 0.35f);
	env.ambientColor = Vector3(0.8f, 0.7f, 0.6f);
	env.ambientIntensity = 0.42f;
	env.exposure = 1.25f;
	env.debugView = 2;
	cam->setWorldEnvironment(env);
	const UUID camUuid = cam->getUUID();
	root->addChild(_mv(cam));

	// 2. Nested sub-scene with two children referencing each other
	auto subScene = _own_mk<Scene>();
	subScene->setName("EnemySquad");
	subScene->setWorldPosition(Vector3(20.0f, 0.0f, -10.0f));
	subScene->setScale(Vector3(2.0f, 2.0f, 2.0f));
	const UUID subSceneUuid = subScene->getUUID();

	auto leader = _own_mk<WorldObject>();
	leader->setName("SquadLeader");
	leader->setLocalPosition(Vector3(1.0f, 0.0f, 0.0f));
	const UUID leaderUuid = leader->getUUID();

	auto follower = _own_mk<WorldObject>();
	follower->setName("SquadFollower");
	follower->setWorldPosition(Vector3(-3.0f, 1.5f, 4.0f));
	follower->setLocalPosition(Vector3(0.5f, 0.25f, -0.5f));
	follower->setWorldRotation(fromAngleAxis(1.2f, Vector3(0.0f, 1.0f, 0.0f)));
	follower->setLocalRotation(fromAngleAxis(0.3f, Vector3(1.0f, 0.0f, 0.0f)));
	follower->setScale(Vector3(1.5f, 0.75f, 1.25f));
	const UUID followerUuid = follower->getUUID();

	auto agent = _own_mk<TestAgentComponent>();
	agent->health = 175;
	agent->speed = 6.25f;
	agent->active = false;
	agent->tag = "scout";
	agent->homeOffset = Vector3(-4.0f, 5.0f, 6.0f);
	agent->state = AIState::Combat;
	agent->targetEntity = EntityRef(leaderUuid);
	follower->addComponent(_mv(agent));

	// Attach a DartScriptRecord with exposed properties including EntityRef
	auto dartScript = _own_mk<DartScriptRecord>("FollowLeaderScript",
												"scripts/follow_leader.dart");
	dartScript->setProperty("followDistance", SerializedValue(4.5));
	dartScript->setProperty("maxRetries", SerializedValue(int64_t{3}));
	dartScript->setProperty("sprintEnabled", SerializedValue(true));
	dartScript->setProperty("callsign", SerializedValue("Alpha-2"));
	dartScript->setProperty("offset", SerializedValue(Vector3(0.0f, 2.0f, -1.0f)));
	dartScript->setProperty("leaderRef", SerializedValue(EntityRef(leaderUuid)));
	follower->addComponent(_mv(dartScript));

	// Attach a LightComponent and MeshComponent to verify built-in component round-trip
	auto light = _own_mk<LightComponent>(LightType::Spot);
	light->setColor(Vector3(1.0f, 0.8f, 0.4f));
	light->setIntensity(3.5f);
	light->setRange(25.0f);
	light->setConeAngles(0.25f, 0.6f);
	light->setCastsShadows(true);
	light->setShadowDepthBias(1.25f);
	light->setShadowNormalBias(2.0f);
	leader->addComponent(_mv(light));

	auto meshComp = _own_mk<MeshComponent>(std::vector<_ref<Mesh>>{});
	meshComp->setAssetPath("assets/models/leader.glb");
	leader->addComponent(_mv(meshComp));

	subScene->addChild(_mv(leader));
	subScene->addChild(_mv(follower));
	root->addChild(_mv(subScene));

	// Serialize to JSON and save/load via temporary file as well
	const std::string json = SceneSerializer::serializeSceneToJson(root.get());
	ASSERT_FALSE(json.empty());

	std::string err;
	auto loaded = SceneSerializer::deserializeSceneFromJson(json, &err);
	ASSERT_NE(loaded, nullptr) << err;

	// Verify root & hierarchy
	EXPECT_EQ(loaded->getUUID(), rootUuid);
	EXPECT_EQ(loaded->getName(), "MainLevel");
	ASSERT_EQ(loaded->getChildren().size(), 2u);

	// Verify Camera
	WorldObject* loadedCamObj = SceneSerializer::findByUUID(*loaded, camUuid);
	ASSERT_NE(loadedCamObj, nullptr);
	ASSERT_EQ(loadedCamObj->getType(), CAMERA);
	auto* loadedCam = static_cast<Camera*>(loadedCamObj);
	EXPECT_EQ(loadedCam->getName(), "MainCamera");
	EXPECT_NEAR(loadedCam->getWorldPosition().z, 12.0f, kEps);
	EXPECT_NEAR(loadedCam->getWorldEnvironment().clearColor.z, 0.35f, kEps);
	EXPECT_NEAR(loadedCam->getWorldEnvironment().ambientIntensity, 0.42f, kEps);
	EXPECT_NEAR(loadedCam->getWorldEnvironment().exposure, 1.25f, kEps);
	EXPECT_EQ(loadedCam->getWorldEnvironment().debugView, 2);

	// Verify nested sub-scene & parent-child links
	WorldObject* loadedSubObj =
		SceneSerializer::findByUUID(*loaded, subSceneUuid);
	ASSERT_NE(loadedSubObj, nullptr);
	ASSERT_EQ(loadedSubObj->getType(), SCENE);
	EXPECT_EQ(loadedSubObj->getParent(), loaded.get());
	EXPECT_NEAR(loadedSubObj->getScale().x, 2.0f, kEps);

	WorldObject* loadedLeader =
		SceneSerializer::findByUUID(*loaded, leaderUuid);
	ASSERT_NE(loadedLeader, nullptr);
	auto* loadedLight = findComponent<LightComponent>(loadedLeader);
	ASSERT_NE(loadedLight, nullptr);
	EXPECT_EQ(loadedLight->getType(), LightType::Spot);
	EXPECT_NEAR(loadedLight->getColor().y, 0.8f, kEps);
	EXPECT_FLOAT_EQ(loadedLight->getIntensity(), 3.5f);
	EXPECT_FLOAT_EQ(loadedLight->getRange(), 25.0f);
	EXPECT_NEAR(loadedLight->getInnerConeAngle(), 0.25f, kEps);
	EXPECT_NEAR(loadedLight->getOuterConeAngle(), 0.6f, kEps);
	EXPECT_TRUE(loadedLight->getCastsShadows());
	EXPECT_FLOAT_EQ(loadedLight->getShadowDepthBias(), 1.25f);
	EXPECT_FLOAT_EQ(loadedLight->getShadowNormalBias(), 2.0f);

	auto* loadedMesh = findComponent<MeshComponent>(loadedLeader);
	ASSERT_NE(loadedMesh, nullptr);
	EXPECT_EQ(loadedMesh->getAssetPath(), "assets/models/leader.glb");

	WorldObject* loadedFollower =
		SceneSerializer::findByUUID(*loaded, followerUuid);
	ASSERT_NE(loadedFollower, nullptr);
	EXPECT_EQ(loadedFollower->getParent(), static_cast<Scene*>(loadedSubObj));
	EXPECT_EQ(loadedFollower->getName(), "SquadFollower");
	EXPECT_NEAR(loadedFollower->getWorldPosition().x, -3.0f, kEps);
	EXPECT_NEAR(loadedFollower->getLocalPosition().y, 0.25f, kEps);
	EXPECT_NEAR(loadedFollower->getScale().z, 1.25f, kEps);

	// Verify reflected C++ component & EntityRef
	auto* loadedAgent = findComponent<TestAgentComponent>(loadedFollower);
	ASSERT_NE(loadedAgent, nullptr);
	EXPECT_EQ(loadedAgent->health, 175);
	EXPECT_FLOAT_EQ(loadedAgent->speed, 6.25f);
	EXPECT_FALSE(loadedAgent->active);
	EXPECT_EQ(loadedAgent->tag, "scout");
	EXPECT_NEAR(loadedAgent->homeOffset.x, -4.0f, kEps);
	EXPECT_EQ(loadedAgent->state, AIState::Combat);
	EXPECT_EQ(loadedAgent->targetEntity.uuid, leaderUuid);
	EXPECT_NE(SceneSerializer::findByUUID(*loaded, loadedAgent->targetEntity.uuid),
			  nullptr);
	EXPECT_EQ(loadedAgent->initCount, 1);

	// Verify DartScript component reference & exposed properties
	auto* loadedScript = findComponent<DartScriptRecord>(loadedFollower);
	ASSERT_NE(loadedScript, nullptr);
	EXPECT_EQ(loadedScript->getScriptClass(), "FollowLeaderScript");
	EXPECT_EQ(loadedScript->getScriptPath(), "scripts/follow_leader.dart");
	ASSERT_TRUE(loadedScript->hasProperty("followDistance"));
	EXPECT_NEAR(loadedScript->getProperty("followDistance")->asDouble(), 4.5,
				1e-6);
	EXPECT_EQ(loadedScript->getProperty("maxRetries")->asInt(), 3);
	EXPECT_TRUE(loadedScript->getProperty("sprintEnabled")->asBool());
	EXPECT_EQ(loadedScript->getProperty("callsign")->asString(), "Alpha-2");
	EXPECT_NEAR(loadedScript->getProperty("offset")->asVec3().y, 2.0f, kEps);
	EXPECT_EQ(loadedScript->getProperty("leaderRef")->asEntityRef().uuid,
			  leaderUuid);
	EXPECT_EQ(loadedScript->getInitCount(), 1);
}

// ==================================== Prefab Instantiation & Remapping =======

TEST_F(SerializationTest, PrefabInstantiationRemapsInternalRefsAndCallsInit) {
	// Author a multi-entity turret prefab:
	//   TurretRoot (Scene)
	//     |- TurretBase (WorldObject) -> TestAgentComponent(targetEntity -> TurretBarrel, externalAnchor -> externalUuid)
	//     |                           -> DartScriptRecord("TurretAI", barrelRef -> TurretBarrel)
	//     |- TurretBarrel (WorldObject)
	const UUID externalUuid = UUID::generate();

	auto turretTemplate = _own_mk<Scene>();
	turretTemplate->setName("TurretRoot");
	const UUID localRootUuid = turretTemplate->getUUID();

	auto barrel = _own_mk<WorldObject>();
	barrel->setName("TurretBarrel");
	barrel->setLocalPosition(Vector3(0.0f, 1.5f, 0.0f));
	const UUID localBarrelUuid = barrel->getUUID();

	auto base = _own_mk<WorldObject>();
	base->setName("TurretBase");
	const UUID localBaseUuid = base->getUUID();

	auto agent = _own_mk<TestAgentComponent>();
	agent->health = 300;
	agent->targetEntity = EntityRef(localBarrelUuid);
	agent->externalAnchor = EntityRef(externalUuid);
	base->addComponent(_mv(agent));

	auto script = _own_mk<DartScriptRecord>("TurretAI", "scripts/turret.dart");
	script->setProperty("barrelRef", SerializedValue(EntityRef(localBarrelUuid)));
	script->setProperty("fireRate", SerializedValue(2.0));
	base->addComponent(_mv(script));

	turretTemplate->addChild(_mv(barrel));
	turretTemplate->addChild(_mv(base));

	ASSERT_TRUE(PrefabManager::get()->registerPrefabFromEntity(
		"prefabs/turret", turretTemplate.get()));

	// Instantiate the prefab TWICE into a runtime scene via InstantiatePrefab()
	Scene worldScene;
	WorldObject* inst1 = InstantiatePrefab("prefabs/turret", &worldScene);
	WorldObject* inst2 = InstantiatePrefab("prefabs/turret", &worldScene);
	ASSERT_NE(inst1, nullptr);
	ASSERT_NE(inst2, nullptr);
	ASSERT_EQ(inst1->getType(), SCENE);
	ASSERT_EQ(inst2->getType(), SCENE);

	// 1. Every spawned entity gets a fresh UUID distinct from the prefab's local
	//    UUIDs and distinct between instances.
	EXPECT_NE(inst1->getUUID(), localRootUuid);
	EXPECT_NE(inst2->getUUID(), localRootUuid);
	EXPECT_NE(inst1->getUUID(), inst2->getUUID());

	auto* scene1 = static_cast<Scene*>(inst1);
	auto* scene2 = static_cast<Scene*>(inst2);
	ASSERT_EQ(scene1->getChildren().size(), 2u);
	ASSERT_EQ(scene2->getChildren().size(), 2u);

	WorldObject* barrel1 = scene1->getChildren()[0].get();
	WorldObject* base1 = scene1->getChildren()[1].get();
	WorldObject* barrel2 = scene2->getChildren()[0].get();
	WorldObject* base2 = scene2->getChildren()[1].get();

	// 2. Parent pointers match each newly spawned sub-scene
	EXPECT_EQ(inst1->getParent(), &worldScene);
	EXPECT_EQ(barrel1->getParent(), scene1);
	EXPECT_EQ(base1->getParent(), scene1);
	EXPECT_EQ(barrel2->getParent(), scene2);
	EXPECT_EQ(base2->getParent(), scene2);

	// 3. Internal UUIDs remapped uniquely per instance
	EXPECT_NE(barrel1->getUUID(), localBarrelUuid);
	EXPECT_NE(base1->getUUID(), localBaseUuid);
	EXPECT_NE(barrel1->getUUID(), barrel2->getUUID());
	EXPECT_NE(base1->getUUID(), base2->getUUID());

	// 4. Internal EntityRefs point to EACH instance's own barrel, while
	//    external EntityRef stays untouched!
	auto* agent1 = findComponent<TestAgentComponent>(base1);
	auto* agent2 = findComponent<TestAgentComponent>(base2);
	ASSERT_NE(agent1, nullptr);
	ASSERT_NE(agent2, nullptr);
	EXPECT_EQ(agent1->targetEntity.uuid, barrel1->getUUID());
	EXPECT_EQ(agent2->targetEntity.uuid, barrel2->getUUID());
	EXPECT_EQ(agent1->externalAnchor.uuid, externalUuid);
	EXPECT_EQ(agent2->externalAnchor.uuid, externalUuid);

	// 5. DartScript exposed EntityRef property is ALSO remapped!
	auto* script1 = findComponent<DartScriptRecord>(base1);
	auto* script2 = findComponent<DartScriptRecord>(base2);
	ASSERT_NE(script1, nullptr);
	ASSERT_NE(script2, nullptr);
	EXPECT_EQ(script1->getProperty("barrelRef")->asEntityRef().uuid,
			  barrel1->getUUID());
	EXPECT_EQ(script2->getProperty("barrelRef")->asEntityRef().uuid,
			  barrel2->getUUID());

	// 6. Component & script init() were triggered AND saw the remapped UUID!
	EXPECT_EQ(agent1->initCount, 1);
	EXPECT_EQ(agent1->targetUuidSeenAtInit, barrel1->getUUID());
	EXPECT_EQ(agent2->initCount, 1);
	EXPECT_EQ(agent2->targetUuidSeenAtInit, barrel2->getUUID());
	EXPECT_EQ(script1->getInitCount(), 1);
	EXPECT_EQ(script2->getInitCount(), 1);
}

// =========================================== Prefab Instance Overrides =======

TEST_F(SerializationTest, PrefabOverridesStoredAsDeltasAndAppliedBeforeInit) {
	// Author a base enemy prefab:
	//   EnemyRoot (Scene) with TestAgentComponent + DartScriptRecord
	//     |- WeaponMount (WorldObject)
	auto enemyTemplate = _own_mk<Scene>();
	enemyTemplate->setName("Goblin");
	enemyTemplate->setWorldPosition(Vector3(0.0f, 0.0f, 0.0f));
	enemyTemplate->setScale(Vector3(1.0f, 1.0f, 1.0f));

	auto rootAgent = _own_mk<TestAgentComponent>();
	rootAgent->health = 100;
	rootAgent->speed = 3.0f;
	rootAgent->tag = "goblin";
	enemyTemplate->addComponent(_mv(rootAgent));

	auto rootScript = _own_mk<DartScriptRecord>("GoblinAI", "scripts/goblin.dart");
	rootScript->setProperty("aggroRange", SerializedValue(15.0));
	rootScript->setProperty("lootTable", SerializedValue("common"));
	enemyTemplate->addComponent(_mv(rootScript));

	auto weapon = _own_mk<WorldObject>();
	weapon->setName("WeaponMount");
	weapon->setLocalPosition(Vector3(0.5f, 1.0f, 0.0f));
	enemyTemplate->addChild(_mv(weapon));

	ASSERT_TRUE(PrefabManager::get()->registerPrefabFromEntity(
		"prefabs/goblin", enemyTemplate.get()));

	// Place an instance in a scene and edit only a subset of properties!
	auto level = _own_mk<Scene>();
	level->setName("Dungeon1");
	WorldObject* boss = InstantiatePrefab("prefabs/goblin", level.get());
	ASSERT_NE(boss, nullptr);
	const UUID bossSceneUuid = boss->getUUID();

	boss->setName("GoblinBoss");
	boss->setWorldPosition(Vector3(12.0f, 0.0f, -8.0f));
	boss->setScale(Vector3(2.0f, 2.0f, 2.0f));

	auto* bossAgent = findComponent<TestAgentComponent>(boss);
	ASSERT_NE(bossAgent, nullptr);
	bossAgent->health = 500;  // overridden! (speed=3.0 and tag="goblin" stay base)

	auto* bossScript = findComponent<DartScriptRecord>(boss);
	ASSERT_NE(bossScript, nullptr);
	bossScript->setProperty("aggroRange", SerializedValue(35.0));  // overridden!

	// Also override a nested child's localPosition inside the prefab!
	auto* bossScene = static_cast<Scene*>(boss);
	ASSERT_EQ(bossScene->getChildren().size(), 1u);
	bossScene->getChildren()[0]->setLocalPosition(Vector3(1.25f, 2.5f, 0.0f));

	// Save the scene to JSON
	const std::string sceneJson = SceneSerializer::serializeSceneToJson(level.get());
	ASSERT_FALSE(sceneJson.empty());

	// Inspect the JSON directly to prove ONLY prefab_asset_id + overrides were stored!
	std::string parseErr;
	auto doc = JsonValue::parse(sceneJson, &parseErr);
	ASSERT_TRUE(doc.has_value()) << parseErr;
	const JsonValue* children = doc->find("scene")->find("children");
	ASSERT_NE(children, nullptr);
	ASSERT_EQ(children->size(), 1u);
	const JsonValue& bossNode = children->asArray()[0];
	EXPECT_EQ(bossNode.find("prefab_asset_id")->asString(), "prefabs/goblin");
	EXPECT_EQ(bossNode.find("uuid")->asString(), bossSceneUuid.toString());
	EXPECT_EQ(bossNode.find("components"), nullptr);  // NOT duplicated!
	EXPECT_EQ(bossNode.find("children"), nullptr);    // NOT duplicated!
	ASSERT_NE(bossNode.find("overrides"), nullptr);
	EXPECT_EQ(bossNode.find("overrides")->size(), 6u);

	// Now update a NON-overridden field on the base prefab (speed: 3.0 -> 4.25)
	// to prove reloaded instances inherit base prefab updates while keeping overrides!
	auto updatedTemplate = PrefabManager::get()->instantiatePrefabDetached("prefabs/goblin");
	ASSERT_NE(updatedTemplate, nullptr);
	// Re-author base prefab with same local UUIDs by tweaking the template directly:
	auto* tmplAgent = findComponent<TestAgentComponent>(enemyTemplate.get());
	tmplAgent->speed = 4.25f;
	ASSERT_TRUE(PrefabManager::get()->registerPrefabFromEntity(
		"prefabs/goblin", enemyTemplate.get()));

	// Reload the scene!
	auto reloaded = SceneSerializer::deserializeSceneFromJson(sceneJson, &parseErr);
	ASSERT_NE(reloaded, nullptr) << parseErr;
	ASSERT_EQ(reloaded->getChildren().size(), 1u);

	WorldObject* reloadedBoss = reloaded->getChildren()[0].get();
	EXPECT_EQ(reloadedBoss->getUUID(), bossSceneUuid);
	EXPECT_EQ(reloadedBoss->getName(), "GoblinBoss");
	EXPECT_NEAR(reloadedBoss->getWorldPosition().x, 12.0f, kEps);
	EXPECT_NEAR(reloadedBoss->getWorldPosition().z, -8.0f, kEps);
	EXPECT_NEAR(reloadedBoss->getScale().y, 2.0f, kEps);

	// Verify overrides AND inherited base updates AND pre-init application!
	auto* reloadedAgent = findComponent<TestAgentComponent>(reloadedBoss);
	ASSERT_NE(reloadedAgent, nullptr);
	EXPECT_EQ(reloadedAgent->health, 500);          // overridden
	EXPECT_FLOAT_EQ(reloadedAgent->speed, 4.25f);   // inherited updated base!
	EXPECT_EQ(reloadedAgent->tag, "goblin");        // inherited base
	EXPECT_EQ(reloadedAgent->initCount, 1);
	EXPECT_EQ(reloadedAgent->healthSeenAtInit, 500);
	EXPECT_NEAR(reloadedAgent->worldPosSeenAtInit.x, 12.0f, kEps);

	auto* reloadedScript = findComponent<DartScriptRecord>(reloadedBoss);
	ASSERT_NE(reloadedScript, nullptr);
	EXPECT_NEAR(reloadedScript->getProperty("aggroRange")->asDouble(), 35.0,
				1e-6);
	EXPECT_EQ(reloadedScript->getProperty("lootTable")->asString(), "common");
	EXPECT_EQ(reloadedScript->getInitCount(), 1);

	auto* reloadedBossScene = static_cast<Scene*>(reloadedBoss);
	ASSERT_EQ(reloadedBossScene->getChildren().size(), 1u);
	EXPECT_NEAR(reloadedBossScene->getChildren()[0]->getLocalPosition().x,
				1.25f, kEps);
	EXPECT_NEAR(reloadedBossScene->getChildren()[0]->getLocalPosition().y,
				2.5f, kEps);
}

// ============================================== Dart Script Factory Hook =====

TEST_F(SerializationTest, DartScriptFactoryHookRunsOnSceneLoad) {
	bool factoryCalled = false;
	std::string receivedClass;
	double receivedSpeed = 0.0;

	DartScriptRecord::setScriptFactory(
		[&](const std::string& scriptClass, const std::string& /*scriptPath*/,
			const std::map<std::string, SerializedValue>& props)
			-> _own<Component> {
			factoryCalled = true;
			receivedClass = scriptClass;
			if (auto it = props.find("moveSpeed"); it != props.end()) {
				receivedSpeed = it->second.asDouble();
			}
			// Return a DartScriptRecord carrying the properties (simulating a
			// live DartScriptComponent returned by the Dart VM bridge).
			auto rec = _own_mk<DartScriptRecord>(scriptClass);
			for (const auto& kv : props) rec->setProperty(kv.first, kv.second);
			return rec;
		});

	Scene scene;
	auto player = _own_mk<WorldObject>();
	auto script = _own_mk<DartScriptRecord>("PlayerMovement", "player.dart");
	script->setProperty("moveSpeed", SerializedValue(7.5));
	player->addComponent(_mv(script));
	scene.addChild(_mv(player));

	const std::string json = SceneSerializer::serializeSceneToJson(&scene);
	auto loaded = SceneSerializer::deserializeSceneFromJson(json);
	ASSERT_NE(loaded, nullptr);
	EXPECT_TRUE(factoryCalled);
	EXPECT_EQ(receivedClass, "PlayerMovement");
	EXPECT_DOUBLE_EQ(receivedSpeed, 7.5);
}

// =================================================== SaveGame Utility ========

TEST_F(SerializationTest, SaveGameDirectoryResolutionAndAtomicIO) {
	// Redirect XDG_DATA_HOME / XDG_CONFIG_HOME to a temp dir for clean testing.
	namespace fs = std::filesystem;
	const fs::path tempBase =
		fs::temp_directory_path() /
		("bird_savegame_test_" + UUID::generate().toString());
	const fs::path tempData = tempBase / "data";
	const fs::path tempConfig = tempBase / "config";

	const char* prevXdgData = std::getenv("XDG_DATA_HOME");
	const char* prevXdgConfig = std::getenv("XDG_CONFIG_HOME");
	const std::string savedData = prevXdgData ? prevXdgData : "";
	const std::string savedConfig = prevXdgConfig ? prevXdgConfig : "";

#if !defined(_WIN32)
	setenv("XDG_DATA_HOME", tempData.c_str(), 1);
	setenv("XDG_CONFIG_HOME", tempConfig.c_str(), 1);
#endif

	const std::string appName = "BirdTestGame";
	const std::string dataDir = SaveGame::getUserDataDirectory(appName);
	const std::string cfgDir = SaveGame::getConfigDirectory(appName);
	EXPECT_FALSE(dataDir.empty());
	EXPECT_FALSE(cfgDir.empty());
#if !defined(_WIN32) && !defined(__APPLE__)
	EXPECT_EQ(dataDir, (tempData / appName).string());
	EXPECT_EQ(cfgDir, (tempConfig / appName).string());
#endif

	// Validation & path traversal guards
	EXPECT_THROW(SaveGame::getUserDataDirectory(""), std::invalid_argument);
	EXPECT_THROW(SaveGame::getUserDataDirectory("../escape"),
				 std::invalid_argument);
	EXPECT_THROW(SaveGame::getSaveFilePath(appName, "../escape.json"),
				 std::invalid_argument);
	EXPECT_THROW(SaveGame::getSaveFilePath(appName, "/etc/passwd"),
				 std::invalid_argument);

	// Atomic text save / read / overwrite / delete round-trip
	const std::string payload = "{\"slot\":1,\"gold\":999}";
	EXPECT_FALSE(SaveGame::fileExists(appName, "slots/slot1.json"));
	ASSERT_TRUE(SaveGame::writeTextFile(appName, "slots/slot1.json", payload));
	EXPECT_TRUE(SaveGame::fileExists(appName, "slots/slot1.json"));
	EXPECT_EQ(SaveGame::readTextFile(appName, "slots/slot1.json"), payload);

	// Binary round-trip
	const std::vector<uint8_t> binData = {0x42, 0x49, 0x52, 0x44, 0x00, 0xFF};
	ASSERT_TRUE(SaveGame::writeBinaryFile(appName, "progress.bin", binData));
	auto readBin = SaveGame::readBinaryFile(appName, "progress.bin");
	ASSERT_TRUE(readBin.has_value());
	EXPECT_EQ(*readBin, binData);

	EXPECT_TRUE(SaveGame::deleteFile(appName, "slots/slot1.json"));
	EXPECT_FALSE(SaveGame::fileExists(appName, "slots/slot1.json"));

	// Cleanup & restore env
	std::error_code ec;
	fs::remove_all(tempBase, ec);
#if !defined(_WIN32)
	if (prevXdgData)
		setenv("XDG_DATA_HOME", savedData.c_str(), 1);
	else
		unsetenv("XDG_DATA_HOME");
	if (prevXdgConfig)
		setenv("XDG_CONFIG_HOME", savedConfig.c_str(), 1);
	else
		unsetenv("XDG_CONFIG_HOME");
#endif
}
