#include "EditorToolbar.h"

namespace bird::editor {

EditorToolbar::EditorToolbar(UIObject* parentRoot, Callbacks callbacks)
	: m_callbacks(std::move(callbacks)) {
	// Top application menu bar
	m_menuBar = parentRoot->add<UIMenuBar>();
	m_menuBar->setAnchors(UIAlignment::TopLeft, UIAlignment::TopRight);
	m_menuBar->setPivot(UIAlignment::TopLeft);

	m_menuBar->addMenu(
		"File",
		{UIMenuItem::action(
			 "Save Scene",
			 [this] {
				 if (m_callbacks.onSaveScene) m_callbacks.onSaveScene();
			 },
			 "Ctrl+S"),
		 UIMenuItem::action(
			 "Load Scene",
			 [this] {
				 if (m_callbacks.onLoadScene) m_callbacks.onLoadScene();
			 },
			 "Ctrl+O")});

	m_menuBar->addMenu(
		"Edit",
		{UIMenuItem::action(
			 "Undo",
			 [this] {
				 if (m_callbacks.onUndo) m_callbacks.onUndo();
			 },
			 "Ctrl+Z"),
		 UIMenuItem::action(
			 "Redo",
			 [this] {
				 if (m_callbacks.onRedo) m_callbacks.onRedo();
			 },
			 "Ctrl+Y"),
		 UIMenuItem::separator(),
		 UIMenuItem::action(
			 "Create Entity",
			 [this] {
				 if (m_callbacks.onCreateEntity) m_callbacks.onCreateEntity();
			 },
			 "Ctrl+N"),
		 UIMenuItem::action(
			 "Delete Selected",
			 [this] {
				 if (m_callbacks.onDeleteEntity) m_callbacks.onDeleteEntity();
			 },
			 "Delete")});

	m_menuBar->addMenu(
		"Gizmo",
		{UIMenuItem::action(
			 "Translate",
			 [this] {
				 if (m_callbacks.onGizmoMode)
					 m_callbacks.onGizmoMode(GizmoMode::Translate);
			 },
			 "W"),
		 UIMenuItem::action(
			 "Rotate",
			 [this] {
				 if (m_callbacks.onGizmoMode)
					 m_callbacks.onGizmoMode(GizmoMode::Rotate);
			 },
			 "E"),
		 UIMenuItem::action(
			 "Scale",
			 [this] {
				 if (m_callbacks.onGizmoMode)
					 m_callbacks.onGizmoMode(GizmoMode::Scale);
			 },
			 "R"),
		 UIMenuItem::separator(),
		 UIMenuItem::action(
			 "Toggle World / Local Space",
			 [this] {
				 if (m_callbacks.onToggleGizmoSpace)
					 m_callbacks.onToggleGizmoSpace();
			 },
			 "T")});

	m_menuBar->addMenu(
		"Simulation",
		{UIMenuItem::action(
			 "Play",
			 [this] {
				 if (m_callbacks.onPlay) m_callbacks.onPlay();
			 },
			 "F5"),
		 UIMenuItem::action(
			 "Pause",
			 [this] {
				 if (m_callbacks.onPause) m_callbacks.onPause();
			 },
			 "F6"),
		 UIMenuItem::action(
			 "Stop",
			 [this] {
				 if (m_callbacks.onStop) m_callbacks.onStop();
			 },
			 "F7"),
		 UIMenuItem::separator(),
		 UIMenuItem::action(
			 "Toggle Edit / Play Mode",
			 [this] {
				 if (m_callbacks.onToggleEditor) m_callbacks.onToggleEditor();
			 },
			 "F1")});

	// Floating top-center transport & tool bar (sits between Hierarchy and Inspector)
	m_transportPanel = parentRoot->add<UIPanel>();
	m_transportPanel->setAnchor(UIAlignment::TopCenter);
	m_transportPanel->setPivot(UIAlignment::TopCenter);
	m_transportPanel->setOffset(Vector2(0.0f, 34.0f));
	m_transportPanel->setWidth(UISizeRule::fit());
	m_transportPanel->setHeight(UISizeRule::fit());
	m_transportPanel->setPadding(UIInsets{8.0f, 5.0f, 8.0f, 5.0f});

	auto* row = m_transportPanel->add<UIStack>(UIAxis::Horizontal);
	row->setWidth(UISizeRule::fit());
	row->setHeight(UISizeRule::fit());
	row->setGap(5.0f);
	row->setAlign(UIAlign::Center);

	m_modeLabel = row->add<UILabel>("[EDIT]");
	m_modeLabel->setColor(Vector4(0.35f, 0.85f, 1.0f, 1.0f));

	m_playBtn = row->add<UIButton>("Play");
	m_playBtn->setTooltip("Start Play Mode (F5)");
	m_playBtn->setOnClick([this] {
		if (m_callbacks.onPlay) m_callbacks.onPlay();
	});

	m_pauseBtn = row->add<UIButton>("Pause");
	m_pauseBtn->setTooltip("Pause / Resume Simulation (F6)");
	m_pauseBtn->setOnClick([this] {
		if (m_callbacks.onPause) m_callbacks.onPause();
	});

	m_stopBtn = row->add<UIButton>("Stop");
	m_stopBtn->setTooltip("Stop Play Mode and restore scene (F7)");
	m_stopBtn->setOnClick([this] {
		if (m_callbacks.onStop) m_callbacks.onStop();
	});

	m_translateBtn = row->add<UIButton>("[Move]");
	m_translateBtn->setTooltip("Translate Gizmo (W)");
	m_translateBtn->setOnClick([this] {
		if (m_callbacks.onGizmoMode)
			m_callbacks.onGizmoMode(GizmoMode::Translate);
	});

	m_rotateBtn = row->add<UIButton>("Rot");
	m_rotateBtn->setTooltip("Rotate Gizmo (E)");
	m_rotateBtn->setOnClick([this] {
		if (m_callbacks.onGizmoMode) m_callbacks.onGizmoMode(GizmoMode::Rotate);
	});

	m_scaleBtn = row->add<UIButton>("Scale");
	m_scaleBtn->setTooltip("Scale Gizmo (R)");
	m_scaleBtn->setOnClick([this] {
		if (m_callbacks.onGizmoMode) m_callbacks.onGizmoMode(GizmoMode::Scale);
	});

	m_spaceBtn = row->add<UIButton>("World");
	m_spaceBtn->setTooltip("Toggle World / Local Gizmo Space (T)");
	m_spaceBtn->setOnClick([this] {
		if (m_callbacks.onToggleGizmoSpace) m_callbacks.onToggleGizmoSpace();
	});

	m_undoBtn = row->add<UIButton>("Undo");
	m_undoBtn->setTooltip("Undo (Ctrl+Z)");
	m_undoBtn->setEnabled(false);
	m_undoBtn->setOnClick([this] {
		if (m_callbacks.onUndo) m_callbacks.onUndo();
	});

	m_redoBtn = row->add<UIButton>("Redo");
	m_redoBtn->setTooltip("Redo (Ctrl+Y)");
	m_redoBtn->setEnabled(false);
	m_redoBtn->setOnClick([this] {
		if (m_callbacks.onRedo) m_callbacks.onRedo();
	});

	m_saveBtn = row->add<UIButton>("Save");
	m_saveBtn->setTooltip("Save Scene (Ctrl+S)");
	m_saveBtn->setOnClick([this] {
		if (m_callbacks.onSaveScene) m_callbacks.onSaveScene();
	});

	m_loadBtn = row->add<UIButton>("Load");
	m_loadBtn->setTooltip("Load Scene (Ctrl+O)");
	m_loadBtn->setOnClick([this] {
		if (m_callbacks.onLoadScene) m_callbacks.onLoadScene();
	});

	m_statusLabel = row->add<UILabel>("Ready");
	m_statusLabel->setColor(m_statusLabel->getTheme().mutedTextColor);

	refreshLabels();
}

void EditorToolbar::setMode(EditorMode mode) {
	m_mode = mode;
	refreshLabels();
}

void EditorToolbar::setGizmoState(GizmoMode gizmoMode, GizmoSpace space) {
	m_gizmoMode = gizmoMode;
	m_gizmoSpace = space;
	refreshLabels();
}

void EditorToolbar::setUndoRedoState(bool canUndo, const std::string& undoDesc,
									 bool canRedo,
									 const std::string& redoDesc) {
	if (m_undoBtn) {
		m_undoBtn->setEnabled(canUndo);
		m_undoBtn->setTooltip(canUndo ? ("Undo " + undoDesc + " (Ctrl+Z)")
									  : "Nothing to Undo");
	}
	if (m_redoBtn) {
		m_redoBtn->setEnabled(canRedo);
		m_redoBtn->setTooltip(canRedo ? ("Redo " + redoDesc + " (Ctrl+Y)")
									  : "Nothing to Redo");
	}
}

void EditorToolbar::setStatusText(const std::string& text) {
	if (m_statusLabel) {
		m_statusLabel->setText(text);
	}
}

const std::string& EditorToolbar::getStatusText() const {
	static const std::string kEmpty;
	return m_statusLabel ? m_statusLabel->getText() : kEmpty;
}

void EditorToolbar::refreshLabels() {
	if (m_modeLabel) {
		switch (m_mode) {
			case EditorMode::Edit:
				m_modeLabel->setText("[EDIT]");
				m_modeLabel->setColor(Vector4(0.35f, 0.85f, 1.0f, 1.0f));
				break;
			case EditorMode::Play:
				m_modeLabel->setText("[PLAY]");
				m_modeLabel->setColor(Vector4(0.30f, 0.92f, 0.45f, 1.0f));
				break;
			case EditorMode::Paused:
				m_modeLabel->setText("[PAUSED]");
				m_modeLabel->setColor(Vector4(1.00f, 0.80f, 0.25f, 1.0f));
				break;
		}
	}
	if (m_playBtn) {
		m_playBtn->setEnabled(m_mode != EditorMode::Play);
	}
	if (m_pauseBtn) {
		m_pauseBtn->setEnabled(m_mode != EditorMode::Edit);
		m_pauseBtn->setText(m_mode == EditorMode::Paused ? "Resume" : "Pause");
	}
	if (m_stopBtn) {
		m_stopBtn->setEnabled(m_mode != EditorMode::Edit);
	}
	if (m_translateBtn) {
		m_translateBtn->setText(m_gizmoMode == GizmoMode::Translate ? "[Move]"
																	: "Move");
	}
	if (m_rotateBtn) {
		m_rotateBtn->setText(m_gizmoMode == GizmoMode::Rotate ? "[Rot]"
															  : "Rot");
	}
	if (m_scaleBtn) {
		m_scaleBtn->setText(m_gizmoMode == GizmoMode::Scale ? "[Scale]"
															: "Scale");
	}
	if (m_spaceBtn) {
		m_spaceBtn->setText(m_gizmoSpace == GizmoSpace::World ? "World"
															  : "Local");
	}
}

}  // namespace bird::editor
