#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "c_wrapper.h"
#include "dart/DartRuntime.h"
#include "scene/Scene.h"
#include "scene/WorldObject.h"
#include "scene/components/ProcessScript.h"

namespace fs = std::filesystem;

static std::string readFileText(const fs::path& p) {
	std::ifstream in(p);
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

TEST(DartIntegrationTest, GeneratedCWrapperRoundTrip) {
	void* scenePtr = bird_Scene_new();
	ASSERT_NE(scenePtr, nullptr);

	BVec3 delta{1.5f, -2.0f, 3.25f};
	bird_WorldObject_translate(scenePtr, delta);

	BVec3 pos = bird_WorldObject_getWorldPosition(scenePtr);
	EXPECT_NEAR(pos.x, 1.5f, 1e-4f);
	EXPECT_NEAR(pos.y, -2.0f, 1e-4f);
	EXPECT_NEAR(pos.z, 3.25f, 1e-4f);

	void* childPtr = bird_WorldObject_new();
	ASSERT_NE(childPtr, nullptr);
	bird_Scene_addChild(scenePtr, childPtr);

	BPtrList children = bird_Scene_getChildren(scenePtr);
	EXPECT_EQ(children.length, 1);
	EXPECT_EQ(children.data[0], childPtr);
	bird_ptr_list_free(children);

	// Test std::string marshalling + free helper
	void* actionPtr = bird_Action_new("jump");
	ASSERT_NE(actionPtr, nullptr);
	char* name = bird_Action_getName(actionPtr);
	ASSERT_NE(name, nullptr);
	EXPECT_STREQ(name, "jump");
	bird_string_free(name);
	bird_Action_delete(actionPtr);

	bird_Scene_delete(scenePtr);
}

TEST(DartIntegrationTest, GeneratedDartContainsCppDocumentation) {
	fs::path genDir = fs::path(BIRD_SOURCE_DIR) / "dart" / "bird_engine" /
					  "lib" / "src" / "generated";

	std::string worldObjDart = readFileText(genDir / "world_object.dart");
	EXPECT_NE(worldObjDart.find("/// 3D spatial entity in the scene graph."),
			  std::string::npos);
	EXPECT_NE(
		worldObjDart.find(
			"/// Translates the object along world axes and marks the transform dirty."),
		std::string::npos);
	EXPECT_NE(worldObjDart.find("/// - [delta]:"), std::string::npos);

	std::string sceneDart = readFileText(genDir / "scene.dart");
	EXPECT_NE(sceneDart.find("class Scene extends WorldObject"),
			  std::string::npos);
	EXPECT_NE(
		sceneDart.find(
			"/// Takes ownership of a child entity and associates it with this Scene."),
		std::string::npos);

	std::string cameraDart = readFileText(genDir / "camera.dart");
	EXPECT_NE(cameraDart.find("class Camera extends WorldObject"),
			  std::string::npos);
	EXPECT_NE(cameraDart.find("/// 3D perspective camera entity."),
			  std::string::npos);
}

TEST(DartIntegrationTest, EmbeddedDartVmExecutesScriptEndToEnd) {
	fs::path scriptPath =
		fs::path(BIRD_SOURCE_DIR) / "tests" / "dart_integration_script.dart";
	fs::path pkgConfigPath = fs::path(BIRD_SOURCE_DIR) / "dart" /
							 "bird_engine" / ".dart_tool" /
							 "package_config.json";

	ASSERT_TRUE(fs::exists(scriptPath)) << "Missing " << scriptPath;
	ASSERT_TRUE(fs::exists(pkgConfigPath))
		<< "Missing " << pkgConfigPath << " (run dart pub get)";

	bird::DartRuntime runtime;
	ASSERT_TRUE(runtime.init(/*startServiceIsolate=*/false))
		<< runtime.lastError();
	ASSERT_TRUE(runtime.loadScript(scriptPath.string(), pkgConfigPath.string()))
		<< runtime.lastError();

	bird::Scene externalScene;
	externalScene.init(nullptr);

	ASSERT_TRUE(runtime.invokeTopLevelPtr("runIntegrationChecks",
										  static_cast<void*>(&externalScene)))
		<< runtime.lastError();

	// Verify the child entity created in Dart and transferred into externalScene
	ASSERT_EQ(externalScene.getChildren().size(), 1u);
	auto* host = externalScene.getChildren()[0].get();
	EXPECT_NEAR(host->getWorldPosition().x, 10.0f, 1e-4f);

	// Trigger Dart NativeCallable process(delta) directly from C++!
	ASSERT_EQ(host->getComponents().size(), 1u);
	auto* procComp = dynamic_cast<bird::ProcessScriptComponent*>(
		host->getComponents()[0].get());
	ASSERT_NE(procComp, nullptr);
	procComp->process(0.5f);
	procComp->process(0.25f);
	EXPECT_NEAR(host->getWorldPosition().y, 0.75f, 1e-4f);

	// Run Pass 2 UI widgets + Dart closure callbacks end-to-end
	ASSERT_TRUE(runtime.invokeTopLevelVoid("runUiIntegrationChecks"))
		<< runtime.lastError();

	// Run Pass 3 Jolt Physics + raycast + contact callbacks from Dart
	ASSERT_TRUE(runtime.invokeTopLevelVoid("runPhysicsIntegrationChecks"))
		<< runtime.lastError();

	// Deinit triggers Dart deinit() and closes NativeCallables cleanly before VM shutdown
	externalScene.deinit();
	runtime.shutdown();
}
