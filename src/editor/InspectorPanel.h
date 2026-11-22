#ifndef BIRD_EDITOR_INSPECTOR_PANEL_H
#define BIRD_EDITOR_INSPECTOR_PANEL_H

#include <array>
#include <cstdint>
#include <functional>

#include "../scene/2D/UIExpander.h"
#include "../scene/2D/UILabel.h"
#include "../scene/2D/UINumberField.h"
#include "../scene/2D/UIPanel.h"
#include "../scene/2D/UIScrollView.h"
#include "../scene/2D/UIStack.h"
#include "../scene/2D/UITextBox.h"
#include "../scene/WorldObject.h"
#include "UndoStack.h"

namespace bird::editor {

/**
 * Inspector Panel widget built on Bird Engine's custom UI system.
 *
 * Displays and live-edits the currently selected entity's:
 * - Name (UITextBox, committed on Enter or focus change, Undo/Redo-backed).
 * - Spatial Transform (Position X/Y/Z, Euler Rotation X/Y/Z in degrees,
 *   Scale X/Y/Z via UINumberField controls, Undo/Redo-backed).
 * - Attached Components rendered in collapsible UIExpander sections via
 *   ComponentInspectorRegistry.
 *
 * Automatically watches the target entity's getTransformRevision() each frame
 * via syncTransformFields() so gizmo drags or Undo/Redo operations immediately
 * update the displayed numeric fields without rebuilding the entire panel.
 */
class InspectorPanel {
   public:
	/**
	 * Constructs and mounts the Inspector Panel onto @p parentRoot.
	 * @param parentRoot Parent UI node (typically UIManager::getRoot()).
	 * @param undoStack Optional UndoStack for recording reversible property edits.
	 */
	explicit InspectorPanel(UIObject* parentRoot,
							UndoStack* undoStack = nullptr);

	/**
	 * Binds @p target as the inspected entity and rebuilds the component list.
	 * Passing nullptr shows an empty-selection placeholder state.
	 */
	void setTarget(WorldObject* target);

	/** Returns the currently inspected entity (or nullptr). */
	WorldObject* getTarget() const { return m_target; }

	/**
	 * Forces a full rebuild of the inspector contents for the current target
	 * (called after Undo/Redo of component properties).
	 */
	void rebuild();

	/**
	 * Synchronizes the Transform numeric fields if the target entity's
	 * transform revision changed externally (e.g. from a 3D gizmo drag).
	 */
	void syncTransformFields();

	/** Registers a callback fired when the entity's name is edited in the Inspector. */
	void setOnEntityRenamed(std::function<void(WorldObject*)> callback);

	UIPanel* getPanel() const { return m_panel; }
	UITextBox* getNameBox() const { return m_nameBox; }
	const std::array<UINumberField*, 3>& getPositionFields() const {
		return m_posFields;
	}
	const std::array<UINumberField*, 3>& getRotationFields() const {
		return m_rotFields;
	}
	const std::array<UINumberField*, 3>& getScaleFields() const {
		return m_scaleFields;
	}

   private:
	void buildEmptyState();
	void buildEntityView();
	void applyTransformFromFields();

	UndoStack* m_undoStack = nullptr;
	WorldObject* m_target = nullptr;
	uint64_t m_syncedRevision = 0;
	bool m_syncing = false;

	UIPanel* m_panel = nullptr;
	UIScrollView* m_scroll = nullptr;
	UIStack* m_content = nullptr;
	UITextBox* m_nameBox = nullptr;
	std::array<UINumberField*, 3> m_posFields{nullptr, nullptr, nullptr};
	std::array<UINumberField*, 3> m_rotFields{nullptr, nullptr, nullptr};
	std::array<UINumberField*, 3> m_scaleFields{nullptr, nullptr, nullptr};

	std::function<void(WorldObject*)> m_onEntityRenamed;
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_INSPECTOR_PANEL_H
