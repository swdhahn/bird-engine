#include "LevelEditor.h"

#include "../Application.h"
#include "../physics/RigidBodyComponent.h"
#include "ViewportPicker.h"

namespace bird::editor {

namespace {

const Vector4 kSelectionBoxColor{1.0f, 0.78f, 0.18f, 1.0f};
const Vector4 kGridLineColor{0.32f, 0.34f, 0.38f, 0.65f};
const Vector4 kGridAxisXColor{0.75f, 0.25f, 0.25f, 0.85f};
const Vector4 kGridAxisZColor{0.25f, 0.45f, 0.85f, 0.85f};

void drawGroundGrid(DebugDrawList& draw, int halfExtent = 10,
					float spacing = 1.0f) {
	const float ext = static_cast<float>(halfExtent) * spacing;
	for (int i = -halfExtent; i <= halfExtent; ++i) {
		const float coord = static_cast<float>(i) * spacing;
		const Vector4 colZ = (i == 0) ? kGridAxisZColor : kGridLineColor;
		const Vector4 colX = (i == 0) ? kGridAxisXColor : kGridLineColor;
		draw.line(Vector3(coord, 0.0f, -ext), Vector3(coord, 0.0f, ext), colZ);
		draw.line(Vector3(-ext, 0.0f, coord), Vector3(ext, 0.0f, coord), colX);
	}
}

void wakeOrTeleportBodies(WorldObject* entity) {
	if (!entity) return;
	for (auto& comp : entity->getComponents()) {
		if (auto* rb = dynamic_cast<RigidBodyComponent*>(comp.get())) {
			if (rb->isInWorld()) {
				rb->teleport(entity->getWorldPosition(),
							 entity->getWorldRotation());
				rb->setLinearVelocity(Vector3(0.0f));
				rb->setAngularVelocity(Vector3(0.0f));
			}
		}
	}
}

}  // namespace

LevelEditor::LevelEditor(UIManager* uiManager, Scene* rootScene, Camera* camera,
						 Application* app)
	: m_uiManager(uiManager),
	  m_rootScene(rootScene),
	  m_app(app ? app : APPLICATION),
	  m_editorCamera(camera) {
	m_undoStack.setOnChanged([this] { syncToolbarUndoState(); });

	if (m_uiManager && m_uiManager->getRoot()) {
		UIObject* root = m_uiManager->getRoot();

		EditorToolbar::Callbacks cb;
		cb.onPlay = [this] { play(); };
		cb.onPause = [this] { pause(); };
		cb.onStop = [this] { stop(); };
		cb.onToggleEditor = [this] { toggleEditPlay(); };
		cb.onSaveScene = [this] { saveScene(); };
		cb.onLoadScene = [this] { loadScene(); };
		cb.onUndo = [this] {
			if (m_undoStack.undo()) {
				refreshUI();
			}
		};
		cb.onRedo = [this] {
			if (m_undoStack.redo()) {
				refreshUI();
			}
		};
		cb.onCreateEntity = [this] {
			if (m_hierarchy) m_hierarchy->createEntity();
		};
		cb.onDeleteEntity = [this] {
			if (m_hierarchy) m_hierarchy->deleteSelectedEntity();
		};
		cb.onGizmoMode = [this](GizmoMode m) {
			m_gizmo.setMode(m);
			if (m_toolbar) {
				m_toolbar->setGizmoState(m_gizmo.getMode(), m_gizmo.getSpace());
			}
		};
		cb.onToggleGizmoSpace = [this] {
			const GizmoSpace next = (m_gizmo.getSpace() == GizmoSpace::World)
										? GizmoSpace::Local
										: GizmoSpace::World;
			m_gizmo.setSpace(next);
			if (m_toolbar) {
				m_toolbar->setGizmoState(m_gizmo.getMode(), m_gizmo.getSpace());
			}
		};

		m_toolbar = std::make_unique<EditorToolbar>(root, std::move(cb));
		m_hierarchy =
			std::make_unique<HierarchyPanel>(root, m_rootScene, &m_undoStack);
		m_inspector = std::make_unique<InspectorPanel>(root, &m_undoStack);

		m_hierarchy->setOnSelectionChanged([this](WorldObject* entity) {
			m_selected = entity;
			m_gizmo.cancelDrag();
			if (m_inspector) m_inspector->setTarget(entity);
		});

		m_hierarchy->setOnEntityActivated([this](WorldObject* entity) {
			selectEntity(entity);
			focusSelectedEntity();
		});

		m_inspector->setOnEntityRenamed([this](WorldObject*) {
			if (m_hierarchy) m_hierarchy->refresh();
		});
	}

	// Default to Edit Mode (simulation paused, panels visible).
	setMode(EditorMode::Edit);
}

LevelEditor::~LevelEditor() {
	if (m_app) {
		m_app->setSimulationPaused(false);
	}
}

void LevelEditor::setMode(EditorMode mode) {
	if (m_mode == mode) {
		if (m_app) {
			m_app->setSimulationPaused(mode != EditorMode::Play);
		}
		return;
	}

	const EditorMode prevMode = m_mode;
	m_mode = mode;
	m_gizmo.cancelDrag();

	if (prevMode == EditorMode::Edit && mode == EditorMode::Play) {
		// Entering Play Mode from Edit Mode: snapshot scene transforms.
		m_playSnapshots.clear();
		capturePlaySnapshot(m_rootScene);
		if (Camera* cam = m_editorCamera.getCamera()) {
			m_cameraPrePlaySnapshot = TransformSnapshot::capture(*cam);
			m_hasCameraPrePlaySnapshot = true;
		}
		setPanelsVisible(false);
		if (m_toolbar) m_toolbar->setStatusText("Playing...");
	} else if (mode == EditorMode::Paused) {
		setPanelsVisible(true);
		m_editorCamera.syncAnglesFromCamera();
		if (m_toolbar) m_toolbar->setStatusText("Paused");
	} else if (mode == EditorMode::Edit) {
		// Returning to Edit Mode: restore pre-play transforms.
		if (!m_playSnapshots.empty()) {
			restorePlaySnapshot(m_rootScene);
			m_playSnapshots.clear();
		}
		if (m_hasCameraPrePlaySnapshot) {
			if (Camera* cam = m_editorCamera.getCamera()) {
				m_cameraPrePlaySnapshot.apply(*cam);
				m_editorCamera.syncAnglesFromCamera();
			}
			m_hasCameraPrePlaySnapshot = false;
		}
		if (INPUT) {
			INPUT->setMouseCaptured(false);
		}
		setPanelsVisible(true);
		refreshUI();
		if (m_toolbar) m_toolbar->setStatusText("Ready");
	} else if (prevMode == EditorMode::Paused && mode == EditorMode::Play) {
		setPanelsVisible(false);
		if (m_toolbar) m_toolbar->setStatusText("Playing...");
	}

	if (m_app) {
		m_app->setSimulationPaused(mode != EditorMode::Play);
	}
	if (m_toolbar) {
		m_toolbar->setMode(mode);
	}
}

void LevelEditor::play() {
	setMode(EditorMode::Play);
}

void LevelEditor::pause() {
	if (m_mode == EditorMode::Play) {
		setMode(EditorMode::Paused);
	} else if (m_mode == EditorMode::Paused) {
		setMode(EditorMode::Play);
	}
}

void LevelEditor::stop() {
	setMode(EditorMode::Edit);
}

void LevelEditor::toggleEditPlay() {
	if (m_mode == EditorMode::Edit) {
		play();
	} else {
		stop();
	}
}

void LevelEditor::setPanelsVisible(bool visible) {
	m_panelsVisible = visible;
	if (m_hierarchy && m_hierarchy->getPanel()) {
		m_hierarchy->getPanel()->setVisible(visible);
	}
	if (m_inspector && m_inspector->getPanel()) {
		m_inspector->getPanel()->setVisible(visible);
	}
}

void LevelEditor::setRootScene(Scene* rootScene) {
	m_rootScene = rootScene;
	m_selected = nullptr;
	if (m_hierarchy) m_hierarchy->setRootScene(rootScene);
	if (m_inspector) m_inspector->setTarget(nullptr);
}

void LevelEditor::setCamera(Camera* camera) {
	m_editorCamera.setCamera(camera);
}

void LevelEditor::selectEntity(WorldObject* entity) {
	m_selected = entity;
	m_gizmo.cancelDrag();
	if (m_hierarchy) m_hierarchy->setSelectedEntity(entity, false);
	if (m_inspector) m_inspector->setTarget(entity);
}

void LevelEditor::focusSelectedEntity() {
	if (!m_selected) return;
	const AABB bounds = ViewportPicker::computeEntityBounds(m_selected);
	const float radius = glm::length(bounds.extents());
	const float distance = std::max(3.0f, radius * 2.5f);
	m_editorCamera.focusOn(bounds.center(), distance);
}

void LevelEditor::refreshUI() {
	if (m_hierarchy) m_hierarchy->refresh();
	if (m_inspector) {
		m_inspector->setTarget(m_selected);
		m_inspector->rebuild();
	}
	syncToolbarUndoState();
}

void LevelEditor::update(float delta, Input* input, DebugDrawList* debugDraw) {
	if (!input) input = INPUT;
	if (!debugDraw && m_app && m_app->getGraphicsPipeline()) {
		debugDraw = &m_app->getGraphicsPipeline()->getDebugDraw();
	}
	if (!m_editorCamera.getCamera() && m_app) {
		m_editorCamera.setCamera(m_app->getCamera());
	}

	if (input) {
		handleShortcuts(*input);
	}

	if (m_mode != EditorMode::Play) {
		bool flying = false;
		if (input) {
			flying = m_editorCamera.updateFromInput(*input, m_uiManager, delta);
			if (!flying) {
				handleViewportPointer(*input);
			}
		}
		if (m_inspector) {
			m_inspector->syncTransformFields();
		}
		if (debugDraw) {
			drawViewportOverlays(*debugDraw);
		}
	}
}

void LevelEditor::handleShortcuts(Input& input) {
	const bool uiTyping = m_uiManager && m_uiManager->wantsKeyboard();

	// F1 / F5 / F6 / F7 mode hotkeys always work when not typing in a text box.
	if (!uiTyping) {
		if (input.wasKeyPressed(KEY_F1)) {
			toggleEditPlay();
			return;
		}
		if (input.wasKeyPressed(KEY_F5)) {
			play();
			return;
		}
		if (input.wasKeyPressed(KEY_F6)) {
			pause();
			return;
		}
		if (input.wasKeyPressed(KEY_F7)) {
			stop();
			return;
		}
	}

	if (m_mode == EditorMode::Play || uiTyping) return;

	const bool ctrl = input.isKeyDown(KEY_LEFT_CONTROL) ||
					  input.isKeyDown(KEY_RIGHT_CONTROL);
	const bool shift =
		input.isKeyDown(KEY_LEFT_SHIFT) || input.isKeyDown(KEY_RIGHT_SHIFT);

	if (ctrl) {
		if (input.wasKeyPressed(KEY_Z)) {
			if (shift) {
				if (m_undoStack.redo()) refreshUI();
			} else {
				if (m_undoStack.undo()) refreshUI();
			}
			return;
		}
		if (input.wasKeyPressed(KEY_Y)) {
			if (m_undoStack.redo()) refreshUI();
			return;
		}
		if (input.wasKeyPressed(KEY_S)) {
			saveScene();
			return;
		}
		if (input.wasKeyPressed(KEY_O)) {
			loadScene();
			return;
		}
		if (input.wasKeyPressed(KEY_N)) {
			if (m_hierarchy) m_hierarchy->createEntity();
			return;
		}
	}

	// While flying with RMB held, W/E/R/Q are movement keys, not gizmo shortcuts.
	if (m_editorCamera.isFlying()) return;

	if (input.wasKeyPressed(KEY_W)) {
		m_gizmo.setMode(GizmoMode::Translate);
		if (m_toolbar)
			m_toolbar->setGizmoState(m_gizmo.getMode(), m_gizmo.getSpace());
	} else if (input.wasKeyPressed(KEY_E)) {
		m_gizmo.setMode(GizmoMode::Rotate);
		if (m_toolbar)
			m_toolbar->setGizmoState(m_gizmo.getMode(), m_gizmo.getSpace());
	} else if (input.wasKeyPressed(KEY_R)) {
		m_gizmo.setMode(GizmoMode::Scale);
		if (m_toolbar)
			m_toolbar->setGizmoState(m_gizmo.getMode(), m_gizmo.getSpace());
	} else if (input.wasKeyPressed(KEY_T)) {
		const GizmoSpace next = (m_gizmo.getSpace() == GizmoSpace::World)
									? GizmoSpace::Local
									: GizmoSpace::World;
		m_gizmo.setSpace(next);
		if (m_toolbar)
			m_toolbar->setGizmoState(m_gizmo.getMode(), m_gizmo.getSpace());
	} else if (input.wasKeyPressed(KEY_F)) {
		focusSelectedEntity();
	} else if (input.wasKeyPressed(KEY_DELETE)) {
		if (m_hierarchy) m_hierarchy->deleteSelectedEntity();
	}
}

void LevelEditor::handleViewportPointer(Input& input) {
	Camera* cam = m_editorCamera.getCamera();
	if (!cam) return;

	const Vector2 viewport =
		m_uiManager ? m_uiManager->getViewportSize() : Vector2(1280.0f, 720.0f);
	const Vector2 cursor = input.getCursorPosition();
	const Ray ray = ViewportPicker::screenToRay(cursor, viewport, *cam);
	const bool uiWantsMouse = m_uiManager && m_uiManager->wantsMouse();

	if (m_gizmo.isDragging()) {
		if (input.isMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			m_gizmo.updateDrag(ray, *cam);
			if (m_selected && m_selected->needsMatrixUpdate()) {
				m_selected->updateTransformationMatrix();
			}
		} else {
			if (auto cmd = m_gizmo.endDrag()) {
				m_undoStack.push(std::move(cmd), false);
			}
			if (m_inspector) m_inspector->syncTransformFields();
		}
		return;
	}

	if (uiWantsMouse) {
		m_gizmo.updateHover(Ray{}, nullptr, *cam);
		return;
	}

	m_gizmo.updateHover(ray, m_selected, *cam);

	if (input.wasMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		if (m_selected && m_gizmo.beginDrag(ray, m_selected, *cam)) {
			return;
		}
		const PickHit hit = ViewportPicker::pick(ray, m_rootScene, cam);
		selectEntity(hit.entity);
	}
}

void LevelEditor::drawViewportOverlays(DebugDrawList& debugDraw) const {
	if (m_showGrid) {
		drawGroundGrid(debugDraw);
	}
	if (m_selected) {
		const AABB bounds = ViewportPicker::computeEntityBounds(m_selected);
		debugDraw.box(bounds.min, bounds.max, kSelectionBoxColor);
		if (const Camera* cam = m_editorCamera.getCamera()) {
			m_gizmo.draw(debugDraw, m_selected, *cam);
		}
	}
}

void LevelEditor::setSerializationHooks(SceneSerializationHooks hooks) {
	m_serializationHooks = std::move(hooks);
}

bool LevelEditor::saveScene(const std::string& path) {
	const std::string targetPath =
		path.empty() ? m_serializationHooks.defaultScenePath : path;
	if (!m_serializationHooks.saveScene) {
		if (m_toolbar) {
			m_toolbar->setStatusText("Save hook ready (" + targetPath + ")");
		}
		return false;
	}
	const bool ok = m_serializationHooks.saveScene(m_rootScene, targetPath);
	if (m_toolbar) {
		m_toolbar->setStatusText(ok ? ("Saved: " + targetPath)
									: ("Save failed: " + targetPath));
	}
	return ok;
}

bool LevelEditor::loadScene(const std::string& path) {
	const std::string targetPath =
		path.empty() ? m_serializationHooks.defaultScenePath : path;
	if (!m_serializationHooks.loadScene) {
		if (m_toolbar) {
			m_toolbar->setStatusText("Load hook ready (" + targetPath + ")");
		}
		return false;
	}
	selectEntity(nullptr);
	m_undoStack.clear();
	const bool ok = m_serializationHooks.loadScene(m_rootScene, targetPath);
	refreshUI();
	if (m_toolbar) {
		m_toolbar->setStatusText(ok ? ("Loaded: " + targetPath)
									: ("Load failed: " + targetPath));
	}
	return ok;
}

void LevelEditor::capturePlaySnapshot(Scene* scene) {
	if (!scene) return;
	for (auto& childPtr : scene->getChildren()) {
		WorldObject* child = childPtr.get();
		if (!child) continue;
		m_playSnapshots[child->getID()] = TransformSnapshot::capture(*child);
		if (child->getType() == SCENE) {
			capturePlaySnapshot(static_cast<Scene*>(child));
		}
	}
}

void LevelEditor::restorePlaySnapshot(Scene* scene) {
	if (!scene) return;
	for (auto& childPtr : scene->getChildren()) {
		WorldObject* child = childPtr.get();
		if (!child) continue;
		const auto it = m_playSnapshots.find(child->getID());
		if (it != m_playSnapshots.end()) {
			it->second.apply(*child);
			child->updateTransformationMatrix();
			wakeOrTeleportBodies(child);
		}
		if (child->getType() == SCENE) {
			restorePlaySnapshot(static_cast<Scene*>(child));
		}
	}
}

void LevelEditor::syncToolbarUndoState() {
	if (!m_toolbar) return;
	m_toolbar->setUndoRedoState(m_undoStack.canUndo(),
								m_undoStack.undoDescription(),
								m_undoStack.canRedo(),
								m_undoStack.redoDescription());
}

}  // namespace bird::editor
