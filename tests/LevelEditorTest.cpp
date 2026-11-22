#include <gtest/gtest.h>

#include "UIHarness.h"
#include "editor/ComponentInspector.h"
#include "editor/EditorCamera.h"
#include "editor/EditorToolbar.h"
#include "editor/HierarchyPanel.h"
#include "editor/InspectorPanel.h"
#include "editor/LevelEditor.h"
#include "editor/TransformGizmo.h"
#include "editor/UndoStack.h"
#include "editor/ViewportPicker.h"
#include "renders/DebugDraw.h"
#include "scene/Camera.h"
#include "scene/Scene.h"
#include "scene/WorldObject.h"
#include "scene/components/LightComponent.h"

namespace bird::editor {
namespace {

// Custom component for testing ComponentInspectorRegistry extensibility.
class CustomHealthComponent : public Component {
   public:
	float maxHealth = 100.0f;
	bool invulnerable = false;

	void init() override {}
	void deinit() override {}
};

TEST(LevelEditorUndoTest, TransformCommandAndUndoRedoCycle) {
	WorldObject entity;
	entity.setName("Hero");
	entity.setWorldPosition(Vector3(1.0f, 2.0f, 3.0f));
	entity.setScale(Vector3(1.0f));

	const TransformSnapshot before = TransformSnapshot::capture(entity);

	entity.setWorldPosition(Vector3(10.0f, 20.0f, 30.0f));
	entity.setScale(Vector3(2.0f, 2.0f, 2.0f));
	const TransformSnapshot after = TransformSnapshot::capture(entity);

	UndoStack stack;
	EXPECT_FALSE(stack.canUndo());
	EXPECT_FALSE(stack.canRedo());

	stack.push(
		std::make_unique<TransformCommand>(&entity, before, after, "Move Hero"),
		false);
	EXPECT_TRUE(stack.canUndo());
	EXPECT_FALSE(stack.canRedo());
	EXPECT_EQ(stack.undoDescription(), "Move Hero");

	EXPECT_TRUE(stack.undo());
	EXPECT_FLOAT_EQ(entity.getWorldPosition().x, 1.0f);
	EXPECT_FLOAT_EQ(entity.getWorldPosition().y, 2.0f);
	EXPECT_FLOAT_EQ(entity.getScale().x, 1.0f);
	EXPECT_FALSE(stack.canUndo());
	EXPECT_TRUE(stack.canRedo());

	EXPECT_TRUE(stack.redo());
	EXPECT_FLOAT_EQ(entity.getWorldPosition().x, 10.0f);
	EXPECT_FLOAT_EQ(entity.getScale().x, 2.0f);
}

TEST(LevelEditorUndoTest, CreateAndDeleteEntityCommands) {
	Scene root;
	UndoStack stack;

	auto child = _own_mk<WorldObject>();
	child->setName("Crate");
	WorldObject* rawPtr = child.get();

	stack.push(std::make_unique<CreateEntityCommand>(&root, std::move(child)),
			   true);
	ASSERT_EQ(root.getChildren().size(), 1u);
	EXPECT_EQ(root.getChildren()[0].get(), rawPtr);

	// Undo creation detaches the entity.
	EXPECT_TRUE(stack.undo());
	EXPECT_TRUE(root.getChildren().empty());

	// Redo creation re-attaches the same entity.
	EXPECT_TRUE(stack.redo());
	ASSERT_EQ(root.getChildren().size(), 1u);
	EXPECT_EQ(root.getChildren()[0].get(), rawPtr);

	// Delete command detaches and can be undone.
	stack.push(std::make_unique<DeleteEntityCommand>(&root, rawPtr), true);
	EXPECT_TRUE(root.getChildren().empty());
	EXPECT_TRUE(stack.undo());
	ASSERT_EQ(root.getChildren().size(), 1u);
	EXPECT_EQ(root.getChildren()[0]->getName(), "Crate");
}

TEST(LevelEditorCameraTest, FreeFlyMovementLookAndScrollSpeed) {
	Camera cam(1280, 720);
	cam.setWorldPosition(Vector3(0.0f, 0.0f, 5.0f));

	EditorCamera controller(&cam);
	EXPECT_FLOAT_EQ(controller.getMoveSpeed(), EditorCamera::kDefaultSpeed);

	// Scroll wheel up increases speed.
	EditorCameraInput scrollUp;
	scrollUp.scrollDelta = 2.0f;
	controller.update(scrollUp, 0.0f);
	EXPECT_GT(controller.getMoveSpeed(), EditorCamera::kDefaultSpeed);

	// Reset speed and move forward (-Z) for 1 second.
	controller.setMoveSpeed(10.0f);
	EditorCameraInput moveFwd;
	moveFwd.forward = 1.0f;
	controller.update(moveFwd, 1.0f);
	EXPECT_NEAR(cam.getWorldPosition().z, -5.0f, 1e-4f);

	// Look right and pitch up.
	EditorCameraInput look;
	look.lookActive = true;
	look.mouseDelta = Vector2(100.0f, -50.0f);
	controller.update(look, 0.016f);
	EXPECT_LT(controller.getYaw(), 0.0f);
	EXPECT_GT(controller.getPitch(), 0.0f);

	// Focus frames target at requested distance along view direction.
	controller.focusOn(Vector3(0.0f, 0.0f, 0.0f), 6.0f);
	EXPECT_NEAR(glm::length(cam.getWorldPosition()), 6.0f, 1e-3f);
}

TEST(LevelEditorPickingTest, RaycastSelectsClosestEntity) {
	Camera cam(800, 600);
	cam.setWorldPosition(Vector3(0.0f, 0.0f, 10.0f));
	cam.updateTransformationMatrix();

	Scene root;
	auto nearObj = _own_mk<WorldObject>();
	nearObj->setName("Near");
	nearObj->setWorldPosition(Vector3(0.0f, 0.0f, 4.0f));
	nearObj->updateTransformationMatrix();
	WorldObject* nearPtr = nearObj.get();
	root.addChild(std::move(nearObj));

	auto farObj = _own_mk<WorldObject>();
	farObj->setName("Far");
	farObj->setWorldPosition(Vector3(0.0f, 0.0f, -2.0f));
	farObj->updateTransformationMatrix();
	root.addChild(std::move(farObj));

	// Ray through the exact center of the 800x600 viewport looks down -Z.
	const Ray centerRay = ViewportPicker::screenToRay(
		Vector2(400.0f, 300.0f), Vector2(800.0f, 600.0f), cam);
	EXPECT_NEAR(centerRay.direction.x, 0.0f, 1e-4f);
	EXPECT_NEAR(centerRay.direction.y, 0.0f, 1e-4f);
	EXPECT_NEAR(centerRay.direction.z, -1.0f, 1e-4f);

	const PickHit hit = ViewportPicker::pick(centerRay, &root);
	ASSERT_TRUE(static_cast<bool>(hit));
	EXPECT_EQ(hit.entity, nearPtr);
}

TEST(LevelEditorGizmoTest, TranslateRotateAndScaleDragProducesUndoCommands) {
	Camera cam(800, 600);
	cam.setWorldPosition(Vector3(0.0f, 0.0f, 8.0f));

	WorldObject target;
	target.setWorldPosition(Vector3(0.0f, 0.0f, 0.0f));

	TransformGizmo gizmo;
	gizmo.setMode(GizmoMode::Translate);

	// Ray aimed at the +X axis shaft of the gizmo at (0.6, 0, 0).
	Ray hitXRay;
	hitXRay.origin = Vector3(0.6f, 0.0f, 8.0f);
	hitXRay.direction = Vector3(0.0f, 0.0f, -1.0f);

	EXPECT_EQ(gizmo.hitTest(hitXRay, target, cam), GizmoAxis::X);
	ASSERT_TRUE(gizmo.beginDrag(hitXRay, &target, cam));
	EXPECT_TRUE(gizmo.isDragging());

	// Drag ray to x = 2.6 (+2.0 units along +X).
	Ray dragXRay;
	dragXRay.origin = Vector3(2.6f, 0.0f, 8.0f);
	dragXRay.direction = Vector3(0.0f, 0.0f, -1.0f);
	gizmo.updateDrag(dragXRay, cam);
	EXPECT_NEAR(target.getWorldPosition().x, 2.0f, 1e-3f);
	EXPECT_NEAR(target.getWorldPosition().y, 0.0f, 1e-4f);

	auto cmd = gizmo.endDrag();
	ASSERT_NE(cmd, nullptr);
	EXPECT_FALSE(gizmo.isDragging());

	// Undoing the gizmo command restores the entity to x = 0.
	cmd->undo();
	EXPECT_NEAR(target.getWorldPosition().x, 0.0f, 1e-4f);
	cmd->execute();
	EXPECT_NEAR(target.getWorldPosition().x, 2.0f, 1e-3f);

	// Emit gizmo overlay lines into DebugDrawList.
	DebugDrawList drawList;
	gizmo.draw(drawList, &target, cam);
	EXPECT_FALSE(drawList.overlayVertices().empty());
}

TEST(LevelEditorPanelsTest, HierarchyInspectorAndModeLifecycle) {
	test::UIHarness harness(Vector2(1280.0f, 720.0f));
	Scene root;
	Camera cam(1280, 720);
	cam.setWorldPosition(Vector3(0.0f, 2.0f, 8.0f));

	auto lightNode = _own_mk<WorldObject>();
	lightNode->setName("Sun");
	lightNode->setWorldPosition(Vector3(1.0f, 5.0f, 2.0f));
	lightNode->addComponent(_own_mk<LightComponent>(LightType::Directional));
	WorldObject* sunPtr = lightNode.get();
	root.addChild(std::move(lightNode));

	LevelEditor editor(&harness.ui, &root, &cam, nullptr);
	harness.frame();

	EXPECT_EQ(editor.getMode(), EditorMode::Edit);
	EXPECT_TRUE(editor.arePanelsVisible());

	// Select the Sun entity and verify Inspector populates fields.
	editor.selectEntity(sunPtr);
	harness.frame();
	EXPECT_EQ(editor.getSelectedEntity(), sunPtr);
	ASSERT_NE(editor.getInspectorPanel()->getNameBox(), nullptr);
	EXPECT_EQ(editor.getInspectorPanel()->getNameBox()->getText(), "Sun");
	EXPECT_NEAR(
		editor.getInspectorPanel()->getPositionFields()[0]->getValue(), 1.0,
		1e-3);

	// Edit position X via Inspector number field -> records Undo command.
	editor.getInspectorPanel()->getPositionFields()[0]->setValue(4.5);
	EXPECT_NEAR(sunPtr->getWorldPosition().x, 4.5f, 1e-3f);
	EXPECT_TRUE(editor.getUndoStack().canUndo());

	// Create a new entity via Hierarchy panel and undo it.
	WorldObject* created =
		editor.getHierarchyPanel()->createEntity("SpawnPoint");
	ASSERT_NE(created, nullptr);
	EXPECT_EQ(root.getChildren().size(), 2u);
	EXPECT_EQ(editor.getSelectedEntity(), created);

	EXPECT_TRUE(editor.getUndoStack().undo());
	EXPECT_EQ(root.getChildren().size(), 1u);

	// Play mode hides panels and snapshots transforms; Stop restores them.
	editor.play();
	EXPECT_EQ(editor.getMode(), EditorMode::Play);
	EXPECT_FALSE(editor.arePanelsVisible());

	// Simulate gameplay moving the Sun.
	sunPtr->setWorldPosition(Vector3(99.0f, 99.0f, 99.0f));
	editor.stop();
	EXPECT_EQ(editor.getMode(), EditorMode::Edit);
	EXPECT_TRUE(editor.arePanelsVisible());
	EXPECT_NEAR(sunPtr->getWorldPosition().x, 4.5f, 1e-3f);
}

TEST(LevelEditorPanelsTest, CustomComponentRegistrationAndSerializationHooks) {
	ComponentInspectorRegistry::instance()
		.registerInspector<CustomHealthComponent>(
			"Health", [](CustomHealthComponent* c, InspectorBuilder& ui) {
				ui.addFloat(
					"Max Health", [c] { return c->maxHealth; },
					[c](float v) { c->maxHealth = v; }, 1.0, 1000.0, 5.0, 1);
				ui.addBool(
					"Invulnerable", [c] { return c->invulnerable; },
					[c](bool b) { c->invulnerable = b; });
			});

	test::UIHarness harness(Vector2(1280.0f, 720.0f));
	Scene root;
	auto hero = _own_mk<WorldObject>();
	hero->setName("Player");
	hero->addComponent(_own_mk<CustomHealthComponent>());
	WorldObject* heroPtr = hero.get();
	root.addChild(std::move(hero));

	LevelEditor editor(&harness.ui, &root);
	editor.selectEntity(heroPtr);
	harness.frame();

	EXPECT_EQ(ComponentInspectorRegistry::instance().displayNameFor(
				  *heroPtr->getComponents()[0]),
			  "Health");

	// Verify Save/Load Scene hooks dispatch cleanly.
	std::string savedPath;
	std::string loadedPath;
	SceneSerializationHooks hooks;
	hooks.defaultScenePath = "levels/arena.scene.bird";
	hooks.saveScene = [&](Scene* s, const std::string& path) {
		EXPECT_EQ(s, &root);
		savedPath = path;
		return true;
	};
	hooks.loadScene = [&](Scene* s, const std::string& path) {
		EXPECT_EQ(s, &root);
		loadedPath = path;
		return true;
	};
	editor.setSerializationHooks(std::move(hooks));

	EXPECT_TRUE(editor.saveScene());
	EXPECT_EQ(savedPath, "levels/arena.scene.bird");
	EXPECT_TRUE(editor.loadScene());
	EXPECT_EQ(loadedPath, "levels/arena.scene.bird");
}

}  // namespace
}  // namespace bird::editor
