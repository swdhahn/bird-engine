#ifndef BIRD_EDITOR_HIERARCHY_PANEL_H
#define BIRD_EDITOR_HIERARCHY_PANEL_H

#include <functional>
#include <string>
#include <unordered_map>

#include "../scene/2D/UIButton.h"
#include "../scene/2D/UILabel.h"
#include "../scene/2D/UIPanel.h"
#include "../scene/2D/UIStack.h"
#include "../scene/2D/UITreeView.h"
#include "../scene/Scene.h"
#include "../scene/WorldObject.h"
#include "UndoStack.h"

namespace bird::editor {

/**
 * Scene Hierarchy Panel widget built on Bird Engine's custom UI system.
 *
 * Displays a recursive UITreeView of all entities in the current root Scene
 * (including nested sub-scenes), along with action buttons to create a new
 * entity, create a child sub-scene, or delete the selected entity. All
 * create/delete mutations go through UndoStack when provided.
 */
class HierarchyPanel {
   public:
	/**
	 * Constructs and mounts the Hierarchy Panel onto @p parentRoot.
	 * @param parentRoot Parent UI node (typically UIManager::getRoot()).
	 * @param rootScene Active root scene whose children are listed.
	 * @param undoStack Optional UndoStack for reversible entity creation/deletion.
	 */
	HierarchyPanel(UIObject* parentRoot, Scene* rootScene,
				   UndoStack* undoStack = nullptr);

	/** Updates the root scene pointer and rebuilds the tree view. */
	void setRootScene(Scene* rootScene);
	Scene* getRootScene() const { return m_rootScene; }

	/** Rebuilds the UITreeView from the current Scene graph while preserving selection. */
	void refresh();

	/**
	 * Programmatically selects @p entity in the tree view (or clears selection
	 * when null). Does not fire the onSelectionChanged callback unless
	 * @p notify is true.
	 */
	void setSelectedEntity(WorldObject* entity, bool notify = false);

	/** Returns the currently selected entity (or nullptr). */
	WorldObject* getSelectedEntity() const { return m_selected; }

	/**
	 * Creates a new empty WorldObject in the currently selected Scene (or the
	 * root Scene), records the action on the UndoStack, and selects it.
	 * @param name Optional entity name (auto-numbered when empty).
	 * @return Pointer to the newly created entity.
	 */
	WorldObject* createEntity(std::string name = "");

	/**
	 * Creates a new child Scene container under the currently selected Scene
	 * (or root Scene), records the action on the UndoStack, and selects it.
	 * @param name Optional sub-scene name.
	 * @return Pointer to the newly created Scene node.
	 */
	Scene* createSubScene(std::string name = "");

	/**
	 * Deletes the currently selected entity (if any) via DeleteEntityCommand.
	 * @return True if an entity was deleted.
	 */
	bool deleteSelectedEntity();

	/** Registers a callback fired whenever the selected entity changes. */
	void setOnSelectionChanged(std::function<void(WorldObject*)> callback);

	/** Registers a callback fired when an entity is double-clicked / activated (e.g. frame camera). */
	void setOnEntityActivated(std::function<void(WorldObject*)> callback);

	/** Returns the root UIPanel for visibility toggling or layout inspection. */
	UIPanel* getPanel() const { return m_panel; }
	UITreeView* getTreeView() const { return m_treeView; }
	UIButton* getCreateButton() const { return m_createBtn; }
	UIButton* getCreateGroupButton() const { return m_createGroupBtn; }
	UIButton* getDeleteButton() const { return m_deleteBtn; }

	/** Formats a display label for @p entity (uses getName() or fallback type + ID). */
	static std::string formatEntityLabel(const WorldObject& entity);

   private:
	static std::string pathKey(const UITreePath& path);
	UITreeNode buildNode(WorldObject* obj, UITreePath currentPath);
	Scene* targetParentScene() const;
	void updateButtonStates();

	Scene* m_rootScene = nullptr;
	UndoStack* m_undoStack = nullptr;
	WorldObject* m_selected = nullptr;

	UIPanel* m_panel = nullptr;
	UILabel* m_countLabel = nullptr;
	UIButton* m_createBtn = nullptr;
	UIButton* m_createGroupBtn = nullptr;
	UIButton* m_deleteBtn = nullptr;
	UITreeView* m_treeView = nullptr;

	std::unordered_map<std::string, WorldObject*> m_pathToEntity;
	std::unordered_map<const WorldObject*, UITreePath> m_entityToPath;
	uint32_t m_createdCounter = 0;
	bool m_suppressCallback = false;

	std::function<void(WorldObject*)> m_onSelectionChanged;
	std::function<void(WorldObject*)> m_onEntityActivated;
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_HIERARCHY_PANEL_H
