#include "HierarchyPanel.h"

#include <sstream>

namespace bird::editor {

std::string HierarchyPanel::formatEntityLabel(const WorldObject& entity) {
	std::string prefix;
	switch (entity.getType()) {
		case SCENE: prefix = "[Scene] "; break;
		case CAMERA: prefix = "[Camera] "; break;
		case LIGHT: prefix = "[Light] "; break;
		default: break;
	}
	if (!entity.getName().empty()) {
		return prefix + entity.getName();
	}
	const char* base = "Entity";
	if (entity.getType() == SCENE) base = "Scene";
	else if (entity.getType() == CAMERA) base = "Camera";
	else if (entity.getType() == LIGHT) base = "Light";
	return prefix + std::string(base) + " #" + std::to_string(entity.getID());
}

std::string HierarchyPanel::pathKey(const UITreePath& path) {
	std::ostringstream oss;
	for (size_t i = 0; i < path.size(); ++i) {
		if (i > 0) oss << '/';
		oss << path[i];
	}
	return oss.str();
}

HierarchyPanel::HierarchyPanel(UIObject* parentRoot, Scene* rootScene,
							   UndoStack* undoStack)
	: m_rootScene(rootScene), m_undoStack(undoStack) {
	m_panel = parentRoot->add<UIPanel>();
	m_panel->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomLeft);
	m_panel->setPivot(UIAlignment::TopLeft);
	m_panel->setMargin(UIInsets{8.0f, 38.0f, 8.0f, 8.0f});
	m_panel->setWidth(UISizeRule::fixed(260.0f));
	m_panel->setPadding(UIInsets::all(8.0f));
	m_panel->setClipChildren(true);

	auto* layout = m_panel->add<UIStack>(UIAxis::Vertical);
	layout->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	layout->setGap(6.0f);
	layout->setAlign(UIAlign::Stretch);

	// Title row
	auto* headerRow = layout->add<UIStack>(UIAxis::Horizontal);
	headerRow->setWidth(UISizeRule::fill());
	headerRow->setHeight(UISizeRule::fit());
	headerRow->setJustify(UIJustify::SpaceBetween);
	headerRow->setAlign(UIAlign::Center);

	auto* title = headerRow->add<UILabel>("Scene Hierarchy");
	title->setFontSize(15.0f);
	m_countLabel = headerRow->add<UILabel>("0");
	m_countLabel->setColor(m_countLabel->getTheme().mutedTextColor);

	// Action buttons row
	auto* btnRow = layout->add<UIStack>(UIAxis::Horizontal);
	btnRow->setWidth(UISizeRule::fill());
	btnRow->setHeight(UISizeRule::fit());
	btnRow->setGap(4.0f);

	m_createBtn = btnRow->add<UIButton>("+ Entity");
	m_createBtn->setWidth(UISizeRule::fill(1.0f));
	m_createBtn->setTooltip("Create a new empty entity");
	m_createBtn->setOnClick([this] { createEntity(); });

	m_createGroupBtn = btnRow->add<UIButton>("+ Group");
	m_createGroupBtn->setWidth(UISizeRule::fill(1.0f));
	m_createGroupBtn->setTooltip("Create a nested Scene group");
	m_createGroupBtn->setOnClick([this] { createSubScene(); });

	m_deleteBtn = btnRow->add<UIButton>("Delete");
	m_deleteBtn->setWidth(UISizeRule::fill(1.0f));
	m_deleteBtn->setTooltip("Delete the selected entity (Delete)");
	m_deleteBtn->setOnClick([this] { deleteSelectedEntity(); });

	// Tree view
	m_treeView = layout->add<UITreeView>();
	m_treeView->setWidth(UISizeRule::fill());
	m_treeView->setHeight(UISizeRule::fill(1.0f));

	m_treeView->setOnSelectionChanged(
		[this](UITreeNode&, const UITreePath& path) {
			if (m_suppressCallback) return;
			const auto it = m_pathToEntity.find(pathKey(path));
			WorldObject* picked =
				(it != m_pathToEntity.end()) ? it->second : nullptr;
			if (picked == m_selected) return;
			m_selected = picked;
			updateButtonStates();
			if (m_onSelectionChanged) m_onSelectionChanged(m_selected);
		});

	m_treeView->setOnActivate([this](UITreeNode&, const UITreePath& path) {
		const auto it = m_pathToEntity.find(pathKey(path));
		if (it != m_pathToEntity.end() && m_onEntityActivated) {
			m_onEntityActivated(it->second);
		}
	});

	refresh();
}

void HierarchyPanel::setRootScene(Scene* rootScene) {
	m_rootScene = rootScene;
	m_selected = nullptr;
	refresh();
}

UITreeNode HierarchyPanel::buildNode(WorldObject* obj, UITreePath currentPath) {
	m_pathToEntity[pathKey(currentPath)] = obj;
	m_entityToPath[obj] = currentPath;

	UITreeNode node;
	node.text = formatEntityLabel(*obj);
	node.expanded = true;

	if (obj->getType() == SCENE) {
		auto* scene = static_cast<Scene*>(obj);
		auto& children = scene->getChildren();
		for (size_t i = 0; i < children.size(); ++i) {
			UITreePath childPath = currentPath;
			childPath.push_back(i);
			node.children.push_back(
				buildNode(children[i].get(), std::move(childPath)));
		}
	}
	return node;
}

void HierarchyPanel::refresh() {
	m_pathToEntity.clear();
	m_entityToPath.clear();

	std::vector<UITreeNode> roots;
	if (m_rootScene) {
		auto& children = m_rootScene->getChildren();
		roots.reserve(children.size());
		for (size_t i = 0; i < children.size(); ++i) {
			roots.push_back(buildNode(children[i].get(), UITreePath{i}));
		}
	}

	m_suppressCallback = true;
	m_treeView->setRoots(std::move(roots));
	if (m_selected) {
		const auto it = m_entityToPath.find(m_selected);
		if (it != m_entityToPath.end()) {
			m_treeView->selectPath(it->second);
		} else {
			m_selected = nullptr;
		}
	}
	m_suppressCallback = false;

	if (m_countLabel) {
		m_countLabel->setText(std::to_string(m_entityToPath.size()));
	}
	updateButtonStates();
}

void HierarchyPanel::setSelectedEntity(WorldObject* entity, bool notify) {
	if (entity && m_entityToPath.find(entity) == m_entityToPath.end()) {
		refresh();
	}
	m_selected = entity;
	m_suppressCallback = true;
	if (m_selected) {
		const auto it = m_entityToPath.find(m_selected);
		if (it != m_entityToPath.end()) {
			m_treeView->selectPath(it->second);
		} else {
			m_selected = nullptr;
		}
	}
	m_suppressCallback = false;
	updateButtonStates();
	if (notify && m_onSelectionChanged) {
		m_onSelectionChanged(m_selected);
	}
}

Scene* HierarchyPanel::targetParentScene() const {
	if (m_selected) {
		if (m_selected->getType() == SCENE) {
			return static_cast<Scene*>(m_selected);
		}
		if (m_selected->getParent()) {
			return m_selected->getParent();
		}
	}
	return m_rootScene;
}

WorldObject* HierarchyPanel::createEntity(std::string name) {
	Scene* parent = targetParentScene();
	if (!parent) return nullptr;

	++m_createdCounter;
	if (name.empty()) {
		name = "Entity " + std::to_string(m_createdCounter);
	}
	auto obj = _own_mk<WorldObject>();
	obj->setName(std::move(name));
	WorldObject* rawPtr = obj.get();

	if (m_undoStack) {
		auto cmd = std::make_unique<CreateEntityCommand>(
			parent, std::move(obj), [this](WorldObject* active) {
				refresh();
				setSelectedEntity(active, true);
			});
		m_undoStack->push(std::move(cmd), true);
	} else {
		parent->addChild(std::move(obj));
		refresh();
		setSelectedEntity(rawPtr, true);
	}
	return rawPtr;
}

Scene* HierarchyPanel::createSubScene(std::string name) {
	Scene* parent = targetParentScene();
	if (!parent) return nullptr;

	++m_createdCounter;
	if (name.empty()) {
		name = "Group " + std::to_string(m_createdCounter);
	}
	auto sub = _own_mk<Scene>();
	sub->setName(std::move(name));
	Scene* rawPtr = sub.get();

	if (m_undoStack) {
		auto cmd = std::make_unique<CreateEntityCommand>(
			parent, std::move(sub), [this](WorldObject* active) {
				refresh();
				setSelectedEntity(active, true);
			});
		m_undoStack->push(std::move(cmd), true);
	} else {
		parent->addChild(std::move(sub));
		refresh();
		setSelectedEntity(rawPtr, true);
	}
	return rawPtr;
}

bool HierarchyPanel::deleteSelectedEntity() {
	if (!m_selected) return false;
	Scene* parent = m_selected->getParent();
	if (!parent) return false;

	WorldObject* toDelete = m_selected;
	if (m_undoStack) {
		auto cmd = std::make_unique<DeleteEntityCommand>(
			parent, toDelete, [this](WorldObject* active) {
				m_selected = active;
				refresh();
				if (m_onSelectionChanged) m_onSelectionChanged(m_selected);
			});
		m_undoStack->push(std::move(cmd), true);
	} else {
		m_selected = nullptr;
		parent->removeChild(toDelete);
		refresh();
		if (m_onSelectionChanged) m_onSelectionChanged(nullptr);
	}
	return true;
}

void HierarchyPanel::setOnSelectionChanged(
	std::function<void(WorldObject*)> callback) {
	m_onSelectionChanged = std::move(callback);
}

void HierarchyPanel::setOnEntityActivated(
	std::function<void(WorldObject*)> callback) {
	m_onEntityActivated = std::move(callback);
}

void HierarchyPanel::updateButtonStates() {
	if (m_deleteBtn) {
		m_deleteBtn->setEnabled(m_selected != nullptr &&
								m_selected->getParent() != nullptr);
	}
}

}  // namespace bird::editor
