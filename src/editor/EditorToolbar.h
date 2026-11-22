#ifndef BIRD_EDITOR_TOOLBAR_H
#define BIRD_EDITOR_TOOLBAR_H

#include <cstdint>
#include <functional>
#include <string>

#include "../scene/2D/UIButton.h"
#include "../scene/2D/UILabel.h"
#include "../scene/2D/UIMenuBar.h"
#include "../scene/2D/UIPanel.h"
#include "../scene/2D/UIStack.h"
#include "TransformGizmo.h"

namespace bird::editor {

/** Execution state of the Level Editor. */
enum class EditorMode : uint8_t {
	/** Simulation paused; editor panels, picking, and 3D gizmos active. */
	Edit = 0,
	/** Game simulation running; pre-play transform snapshot captured. */
	Play = 1,
	/** Simulation temporarily frozen during a Play session. */
	Paused = 2
};

/**
 * Top menu bar and floating action toolbar for the Level Editor.
 *
 * Provides:
 * - Application UIMenuBar (`File`: Save/Load Scene; `Edit`: Undo/Redo,
 *   Create/Delete Entity; `Gizmo`: Translate/Rotate/Scale, World/Local;
 *   `Simulation`: Play/Pause/Stop, Toggle Editor Mode).
 * - Compact center transport pill with Play, Pause, Stop, Gizmo mode buttons
 *   (W / E / R), Space toggle, Undo/Redo buttons, Save/Load buttons, and a
 *   live mode/status badge.
 */
class EditorToolbar {
   public:
	/** Callbacks wired by LevelEditor to respond to toolbar & menu actions. */
	struct Callbacks {
		std::function<void()> onPlay;
		std::function<void()> onPause;
		std::function<void()> onStop;
		std::function<void()> onToggleEditor;
		std::function<void()> onSaveScene;
		std::function<void()> onLoadScene;
		std::function<void()> onUndo;
		std::function<void()> onRedo;
		std::function<void()> onCreateEntity;
		std::function<void()> onDeleteEntity;
		std::function<void(GizmoMode)> onGizmoMode;
		std::function<void()> onToggleGizmoSpace;
	};

	/**
	 * Constructs and mounts the menu bar and transport bar onto @p parentRoot.
	 * @param parentRoot Parent UI node (typically UIManager::getRoot()).
	 * @param callbacks Action callbacks invoked by buttons and menu items.
	 */
	EditorToolbar(UIObject* parentRoot, Callbacks callbacks);

	/** Updates button enablement and labels to reflect @p mode. */
	void setMode(EditorMode mode);
	EditorMode getMode() const { return m_mode; }

	/** Updates the displayed active gizmo mode and coordinate space. */
	void setGizmoState(GizmoMode gizmoMode, GizmoSpace space);

	/** Updates Undo/Redo button enabled states and tooltips. */
	void setUndoRedoState(bool canUndo, const std::string& undoDesc,
						  bool canRedo, const std::string& redoDesc);

	/** Displays a transient or persistent status message in the toolbar. */
	void setStatusText(const std::string& text);
	const std::string& getStatusText() const;

	UIMenuBar* getMenuBar() const { return m_menuBar; }
	UIPanel* getTransportPanel() const { return m_transportPanel; }
	UIButton* getPlayButton() const { return m_playBtn; }
	UIButton* getPauseButton() const { return m_pauseBtn; }
	UIButton* getStopButton() const { return m_stopBtn; }
	UIButton* getTranslateButton() const { return m_translateBtn; }
	UIButton* getRotateButton() const { return m_rotateBtn; }
	UIButton* getScaleButton() const { return m_scaleBtn; }
	UIButton* getSpaceButton() const { return m_spaceBtn; }
	UIButton* getUndoButton() const { return m_undoBtn; }
	UIButton* getRedoButton() const { return m_redoBtn; }
	UIButton* getSaveButton() const { return m_saveBtn; }
	UIButton* getLoadButton() const { return m_loadBtn; }

   private:
	void refreshLabels();

	Callbacks m_callbacks;
	EditorMode m_mode = EditorMode::Edit;
	GizmoMode m_gizmoMode = GizmoMode::Translate;
	GizmoSpace m_gizmoSpace = GizmoSpace::World;

	UIMenuBar* m_menuBar = nullptr;
	UIPanel* m_transportPanel = nullptr;
	UILabel* m_modeLabel = nullptr;
	UILabel* m_statusLabel = nullptr;

	UIButton* m_playBtn = nullptr;
	UIButton* m_pauseBtn = nullptr;
	UIButton* m_stopBtn = nullptr;
	UIButton* m_translateBtn = nullptr;
	UIButton* m_rotateBtn = nullptr;
	UIButton* m_scaleBtn = nullptr;
	UIButton* m_spaceBtn = nullptr;
	UIButton* m_undoBtn = nullptr;
	UIButton* m_redoBtn = nullptr;
	UIButton* m_saveBtn = nullptr;
	UIButton* m_loadBtn = nullptr;
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_TOOLBAR_H
