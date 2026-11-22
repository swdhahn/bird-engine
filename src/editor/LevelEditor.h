#ifndef BIRD_EDITOR_LEVEL_EDITOR_H
#define BIRD_EDITOR_LEVEL_EDITOR_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "../renders/DebugDraw.h"
#include "../scene/2D/UIManager.h"
#include "../scene/Camera.h"
#include "../scene/Scene.h"
#include "../scene/WorldObject.h"
#include "../util/Input.h"
#include "EditorCamera.h"
#include "EditorToolbar.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "TransformGizmo.h"
#include "UndoStack.h"

namespace bird {
class Application;
}

namespace bird::editor {

/**
 * Pluggable Scene Serialization callback hooks used by Save Scene / Load Scene.
 *
 * Designed to plug cleanly into the engine's Scene Serialization API when
 * bound, with a default file path for quick Ctrl+S / Ctrl+O workflows.
 */
struct SceneSerializationHooks {
	/** Default scene path passed to saveScene / loadScene. */
	std::string defaultScenePath = "assets/scenes/default.scene.bird";
	/** Invoked on Save Scene; returns true on success. */
	std::function<bool(Scene* rootScene, const std::string& path)> saveScene;
	/** Invoked on Load Scene; returns true on success. */
	std::function<bool(Scene* rootScene, const std::string& path)> loadScene;
};

/**
 * Complete in-engine Level Editor controller.
 *
 * Orchestrates:
 * - **Edit Mode vs. Play Mode vs. Paused Mode:** Toggles simulation pausing on
 *   `bird::Application`, shows/hides the Hierarchy and Inspector panels, and
 *   snapshots/restores entity transforms across Play sessions.
 * - **Viewport Free-Fly Camera (`EditorCamera`):** WASD + Q/E + Right-Mouse
 *   look + scroll-wheel speed adjustment in Edit and Paused modes.
 * - **Core Custom UI Panels:** `HierarchyPanel`, `InspectorPanel`, and
 *   `EditorToolbar` mounted onto `UIManager`.
 * - **3D Viewport Picking & Transform Gizmos:** Left-click raycast picking
 *   (`ViewportPicker`), selection AABB wireframe highlight, and interactive
 *   Translate / Rotate / Scale handles (`TransformGizmo`).
 * - **Undo/Redo Stack (`UndoStack`):** Reversible history for gizmo drags,
 *   Inspector edits, entity renames, and entity creation/deletion.
 */
class LevelEditor {
   public:
	/**
	 * Constructs a LevelEditor bound to the given subsystems.
	 *
	 * @param uiManager Retained 2D UIManager where editor panels are mounted.
	 * @param rootScene Active root Scene to inspect and edit.
	 * @param camera Active 3D viewport Camera (optional; may be set later).
	 * @param app Optional Application pointer for simulation pause control.
	 */
	LevelEditor(UIManager* uiManager, Scene* rootScene, Camera* camera = nullptr,
				Application* app = nullptr);
	~LevelEditor();

	LevelEditor(const LevelEditor&) = delete;
	LevelEditor& operator=(const LevelEditor&) = delete;

	// -------------------------------------------------- Mode & Play Lifecycle
	/**
	 * Switches between Edit, Play, and Paused modes.
	 *
	 * - Entering `Play` from `Edit` snapshots every entity's transform and
	 *   resumes simulation (`Application::setSimulationPaused(false)`).
	 * - Entering `Paused` freezes simulation while keeping the Play session.
	 * - Returning to `Edit` stops the Play session, restores the pre-play
	 *   transform snapshot, pauses simulation, and reveals editor panels.
	 */
	void setMode(EditorMode mode);
	EditorMode getMode() const { return m_mode; }

	/** Convenience helpers for transport controls. */
	void play();
	void pause();
	void stop();
	/** Toggles directly between Edit Mode and Play Mode (F1). */
	void toggleEditPlay();

	/** Controls whether the Hierarchy and Inspector panels are visible. */
	void setPanelsVisible(bool visible);
	bool arePanelsVisible() const { return m_panelsVisible; }

	// -------------------------------------------------- Selection & Scene
	void setRootScene(Scene* rootScene);
	Scene* getRootScene() const { return m_rootScene; }

	void setCamera(Camera* camera);
	Camera* getCamera() const { return m_editorCamera.getCamera(); }

	/** Selects @p entity in both the Hierarchy and Inspector panels. */
	void selectEntity(WorldObject* entity);
	WorldObject* getSelectedEntity() const { return m_selected; }

	/** Frames the editor camera onto the currently selected entity (F key). */
	void focusSelectedEntity();

	/** Refreshes both Hierarchy and Inspector panels against current scene state. */
	void refreshUI();

	// -------------------------------------------------- Per-Frame Update
	/**
	 * Per-frame editor tick: processes keyboard shortcuts, free-fly camera,
	 * viewport mouse picking, gizmo dragging, Inspector transform sync, and
	 * emits 3D debug lines (grid, selection box, and transform gizmo).
	 *
	 * @param delta Frame delta time in seconds.
	 * @param input Optional Input subsystem (defaults to global `bird::INPUT`).
	 * @param debugDraw Optional DebugDrawList (defaults to pipeline's list).
	 */
	void update(float delta, Input* input = nullptr,
				DebugDrawList* debugDraw = nullptr);

	/**
	 * Renders the reference grid, selected entity's bounding box, and the 3D
	 * transform gizmo into @p debugDraw.
	 */
	void drawViewportOverlays(DebugDrawList& debugDraw) const;

	void setShowGrid(bool show) { m_showGrid = show; }
	bool isGridShown() const { return m_showGrid; }

	// -------------------------------------------------- Serialization Hooks
	void setSerializationHooks(SceneSerializationHooks hooks);
	const SceneSerializationHooks& getSerializationHooks() const {
		return m_serializationHooks;
	}

	/** Invokes the configured Save Scene hook and updates the toolbar status. */
	bool saveScene(const std::string& path = "");
	/** Invokes the configured Load Scene hook, refreshes UI, and updates status. */
	bool loadScene(const std::string& path = "");

	// -------------------------------------------------- Subsystem Accessors
	UndoStack& getUndoStack() { return m_undoStack; }
	const UndoStack& getUndoStack() const { return m_undoStack; }
	EditorCamera& getEditorCamera() { return m_editorCamera; }
	const EditorCamera& getEditorCamera() const { return m_editorCamera; }
	TransformGizmo& getGizmo() { return m_gizmo; }
	const TransformGizmo& getGizmo() const { return m_gizmo; }
	HierarchyPanel* getHierarchyPanel() const { return m_hierarchy.get(); }
	InspectorPanel* getInspectorPanel() const { return m_inspector.get(); }
	EditorToolbar* getToolbar() const { return m_toolbar.get(); }

   private:
	void handleShortcuts(Input& input);
	void handleViewportPointer(Input& input);
	void capturePlaySnapshot(Scene* scene);
	void restorePlaySnapshot(Scene* scene);
	void syncToolbarUndoState();

	UIManager* m_uiManager = nullptr;
	Scene* m_rootScene = nullptr;
	Application* m_app = nullptr;

	EditorMode m_mode = EditorMode::Edit;
	bool m_panelsVisible = true;
	bool m_showGrid = true;
	WorldObject* m_selected = nullptr;

	UndoStack m_undoStack;
	EditorCamera m_editorCamera;
	TransformGizmo m_gizmo;

	std::unique_ptr<EditorToolbar> m_toolbar;
	std::unique_ptr<HierarchyPanel> m_hierarchy;
	std::unique_ptr<InspectorPanel> m_inspector;

	SceneSerializationHooks m_serializationHooks;
	std::unordered_map<uint64_t, TransformSnapshot> m_playSnapshots;
	TransformSnapshot m_cameraPrePlaySnapshot;
	bool m_hasCameraPrePlaySnapshot = false;
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_LEVEL_EDITOR_H
