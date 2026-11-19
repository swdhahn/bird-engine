#include <Application.h>
#include <scene/2D/UIButton.h>
#include <scene/2D/UICheckBox.h>
#include <scene/2D/UIColorPicker.h>
#include <scene/2D/UIDialog.h>
#include <scene/2D/UIDropdown.h>
#include <scene/2D/UIExpander.h>
#include <scene/2D/UILabel.h>
#include <scene/2D/UIListView.h>
#include <scene/2D/UIMenuBar.h>
#include <scene/2D/UINumberField.h>
#include <scene/2D/UIPanel.h>
#include <scene/2D/UIProgressBar.h>
#include <scene/2D/UIRadioGroup.h>
#include <scene/2D/UIRichInputs.h>
#include <scene/2D/UIScrollView.h>
#include <scene/2D/UISlider.h>
#include <scene/2D/UIStack.h>
#include <scene/2D/UITabView.h>
#include <scene/2D/UITextArea.h>
#include <scene/2D/UITextBox.h>
#include <scene/2D/UIToggle.h>
#include <scene/2D/UITreeView.h>
#include <scene/2D/UIWindow.h>
#include <scene/2D/UIManager.h>
#include <scene/Camera.h>
#include <scene/components/MeshComponent.h>
#include <scene/components/Script.h>
#include <util/Assets.h>
#include <util/Math.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class POVCamera : public bird::ProcessScript<bird::Camera> {
   public:
	POVCamera() {}

	~POVCamera() {}

	void init() {
		object()->getWorldEnvironment().clearColor =
			bird::Vector3(0.2, 0.6, 0.85);
	}

	void deinit() {}

	void process(float delta) {
		float speed = 10;
		float rotSpeed = 5;
		float rotVal = input()->getActionValue("look_right") -
					   input()->getActionValue("look_left");
		object()->rotateLocal(bird::Vector3(0, 1, 0),
							  rotVal * rotSpeed * delta);

		object()->translate(
			bird::Vector3((-input()->getActionValue("move_right") +
						   input()->getActionValue("move_left")) *
							  speed * delta,
						  (-input()->getActionValue("move_up") +
						   input()->getActionValue("move_down")) *
							  speed * delta,
						  (-input()->getActionValue("move_backward") +
						   input()->getActionValue("move_forward")) *
							  speed * delta) *
			object()->getLocalRotation() * object()->getWorldRotation());
	}
};


namespace gallery {

using namespace bird;

/** A labelled row: fixed-width caption, then a control that fills. */
UIStack* row(UIObject* parent, const std::string& caption) {
	auto* line = parent->add<UIStack>(UIAxis::Horizontal);
	line->setAlign(UIAlign::Center);
	line->add<UILabel>(caption)->setWidth(UISizeRule::fixed(70.0f));
	return line;
}

void buildControlsTab(UIManager* ui, UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setAlign(UIAlign::Stretch);
	column->setGap(10.0f);

	auto* status = column->add<UILabel>("Interact with the controls below");
	status->setColor(Vector4(0.55f, 0.75f, 1.0f, 1.0f));

	auto* name = row(column, "Name")->add<UITextBox>();
	name->setPlaceholder("Type a name, press Enter");
	name->setWidth(UISizeRule::fill());
	name->setOnSubmit([status](const std::string& text) {
		status->setText("Submitted: " + text);
	});

	column->add<UICheckBox>("Enable shadows", true)
		->setOnChanged([status](bool on) {
			status->setText(on ? "Shadows on" : "Shadows off");
		});

	auto* progress = column->add<UIProgressBar>(0.35f);
	auto* volume = row(column, "Volume")->add<UISlider>(0.0f, 1.0f, 0.35f);
	volume->setWidth(UISizeRule::fill());
	volume->setStep(0.05f);
	volume->setOnChanged([progress](float value) { progress->setValue(value); });
	volume->setTooltip("Drag, click, or use arrow keys");

	auto* buttons = column->add<UIStack>(UIAxis::Horizontal);
	buttons->add<UIButton>("Save")
		->setOnClick([status] { status->setText("Saved (not really)"); })
		.setTooltip("Writes settings to disk");
	buttons->add<UIButton>("Open dialog")->setOnClick([ui, status] {
		auto dialog = std::make_unique<UIDialog>(
			"Discard changes?", "Unsaved settings will be lost.");
		dialog->addButton("Cancel", [status] { status->setText("Kept changes"); });
		dialog->addButton("Discard", [status] { status->setText("Discarded"); });
		ui->pushModal(std::move(dialog));
	});
	buttons->add<UIButton>("Disabled")->setEnabled(false);
}

void buildLayoutTab(UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setAlign(UIAlign::Stretch);
	column->setGap(8.0f);

	page->setContextMenu(
		{UIMenuItem::action("Reset layout", [] {}),
		 UIMenuItem::toggle("Show guides", true),
		 UIMenuItem::separator(),
		 UIMenuItem::submenu("Align", {UIMenuItem::action("Left", [] {}),
									   UIMenuItem::action("Center", [] {}),
									   UIMenuItem::action("Right", [] {})}),
		 UIMenuItem::action("Delete", [] {}).disabled()});
	column->add<UILabel>("Right-click this page for a context menu")
		->setColor(Vector4(0.55f, 0.75f, 1.0f, 1.0f));

	const std::pair<const char*, UIJustify> modes[] = {
		{"Center", UIJustify::Center},
		{"End", UIJustify::End},
		{"SpaceBetween", UIJustify::SpaceBetween}};
	for (const auto& [name, justify] : modes) {
		column->add<UILabel>(std::string("justify ") + name)
			->setColor(Vector4(0.6f, 0.62f, 0.66f, 1.0f));
		auto* line = column->add<UIStack>(UIAxis::Horizontal);
		line->setJustify(justify);
		for (int i = 0; i < 3; i++) {
			auto* box = line->add<UIPanel>();
			box->setSize(Vector2(48.0f, 20.0f));
			box->setBackground(Vector4(0.0f, 0.48f, 0.8f, 0.35f + 0.2f * i));
		}
	}
	column->add<UILabel>("fill weights 1 : 2 : 1")
		->setColor(Vector4(0.6f, 0.62f, 0.66f, 1.0f));
	auto* weights = column->add<UIStack>(UIAxis::Horizontal);
	for (float weight : {1.0f, 2.0f, 1.0f}) {
		auto* box = weights->add<UIPanel>();
		box->setWidth(UISizeRule::fill(weight));
		box->setHeight(UISizeRule::fixed(20.0f));
		box->setBackground(Vector4(0.2f, 0.7f, 0.45f, 0.6f));
	}

	// Cross-axis anchors inside a horizontal stack, plus an Absolute badge.
	column->add<UILabel>("cross-axis anchors + absolute badge")
		->setColor(Vector4(0.6f, 0.62f, 0.66f, 1.0f));
	auto* anchored = column->add<UIStack>(UIAxis::Horizontal);
	anchored->setHeight(UISizeRule::fixed(56.0f));
	anchored->setPadding(UIInsets::all(4.0f));
	const Vector2 anchors[] = {UIAlignment::TopLeft, UIAlignment::CenterLeft,
							   UIAlignment::BottomLeft};
	for (const Vector2& anchor : anchors) {
		auto* box = anchored->add<UIPanel>();
		box->setSize(Vector2(48.0f, 16.0f));
		box->setAnchor(anchor).setPivot(anchor);
		box->setBackground(Vector4(0.8f, 0.5f, 0.1f, 0.7f));
	}
	auto* tall = anchored->add<UIPanel>();
	tall->setWidth(UISizeRule::fixed(48.0f));
	tall->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomLeft);
	tall->setBackground(Vector4(0.8f, 0.5f, 0.1f, 0.4f));
	auto* badge = anchored->add<UILabel>("NEW");
	badge->setLayoutMode(UILayoutMode::Absolute);
	badge->setAnchor(UIAlignment::TopRight).setPivot(UIAlignment::TopRight);
	badge->setColor(Vector4(1.0f, 0.4f, 0.4f, 1.0f));
}

void buildChoicesTab(UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setAlign(UIAlign::Stretch);
	column->setGap(10.0f);
	auto* status = column->add<UILabel>("Pick some settings");
	status->setColor(Vector4(0.55f, 0.75f, 1.0f, 1.0f));

	auto* quality = row(column, "Quality")->add<UIDropdown>(
		std::vector<std::string>{"Low", "Medium", "High", "Ultra"});
	quality->setWidth(UISizeRule::fill());
	quality->setSelectedIndex(2);
	quality->setOnChanged([status, quality](size_t) {
		status->setText("Quality: " + quality->getSelectedText());
	});

	std::vector<std::string> resolutions;
	for (int w : {640, 800, 1024, 1152, 1280, 1366, 1440, 1600, 1680, 1920,
				  2048, 2560, 3440, 3840}) {
		resolutions.push_back(std::to_string(w) + " px wide");
	}
	auto* resolution = row(column, "Width")->add<UIDropdown>(resolutions);
	resolution->setWidth(UISizeRule::fill());
	resolution->setPlaceholder("Choose a resolution");

	auto* modes = column->add<UIRadioGroup>(UIAxis::Horizontal);
	modes->setGap(14.0f);
	modes->addOption("Windowed");
	modes->addOption("Borderless");
	modes->addOption("Fullscreen");
	modes->setSelected(0);

	column->add<UIToggle>("V-Sync", true)->setOnChanged([status](bool on) {
		status->setText(on ? "V-Sync on" : "V-Sync off");
	});

	auto* fov = row(column, "FOV")->add<UINumberField>(60.0, 120.0, 90.0);
	fov->setTooltip("Arrows, wheel, or drag the steppers");
	auto* sensitivity = row(column, "Mouse")->add<UINumberField>(0.1, 5.0, 1.0);
	sensitivity->setStep(0.1).setPrecision(2);
}

void buildTextTab(UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setAlign(UIAlign::Stretch);
	column->setGap(10.0f);
	auto* password = row(column, "Password")->add<UITextBox>("hunter2");
	password->setPassword(true);
	password->setWidth(UISizeRule::fill());

	auto* count = column->add<UILabel>("");
	count->setColor(Vector4(0.6f, 0.62f, 0.66f, 1.0f));
	auto* notes = column->add<UITextArea>(
		"Multi-line text area. Long lines wrap at word boundaries, Enter "
		"inserts a newline and Ctrl+Enter submits.\n\nUp and Down keep the "
		"caret's column; PageUp, PageDown and the mouse wheel scroll.\n\n"
		"Line 6\nLine 7\nLine 8\nLine 9\nLine 10");
	notes->setVisibleLines(9);
	notes->setOnChanged([count](const std::string& text) {
		count->setText(std::to_string(text.size()) + " characters");
	});
	count->setText(std::to_string(notes->getText().size()) + " characters");
}

void buildListsTab(UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setAlign(UIAlign::Stretch);
	column->setGap(8.0f);
	auto* status = column->add<UILabel>("Ctrl/Shift+click to multi-select");
	status->setColor(Vector4(0.55f, 0.75f, 1.0f, 1.0f));

	auto* views = column->add<UIStack>(UIAxis::Horizontal);
	views->setAlign(UIAlign::Stretch);
	// 10,000 generated rows: only the visible ones are ever built.
	auto* list = views->add<UIListView>();
	list->setItemProvider(10000, [](size_t i) {
		return "Asset " + std::to_string(i + 1) + (i % 7 == 0 ? ".png" : ".mesh");
	});
	list->setSelectionMode(UISelectionMode::Multiple);
	list->setWidth(UISizeRule::fill());
	list->setVisibleRows(8);
	list->setOnSelectionChanged([status, list] {
		status->setText(std::to_string(list->getSelection().size()) +
						" of 10000 assets selected");
	});
	list->select(2);

	auto* tree = views->add<UITreeView>(std::vector<UITreeNode>{
		UITreeNode::branch(
			"Village",
			{UITreeNode::leaf("Main Camera"), UITreeNode::leaf("Sun"),
			 UITreeNode::branch("Houses",
								{UITreeNode::leaf("Inn"), UITreeNode::leaf("Smithy"),
								 UITreeNode::branch("Market", {UITreeNode::leaf("Stall A"),
															   UITreeNode::leaf("Stall B")})},
								true),
			 UITreeNode::branch("Trees", {UITreeNode::leaf("Oak"), UITreeNode::leaf("Pine")})},
			true),
		UITreeNode::leaf("Skybox")});
	tree->setWidth(UISizeRule::fill());
	tree->setVisibleRows(8);
	tree->setOnSelectionChanged([status](UITreeNode& node, const UITreePath&) {
		status->setText("Selected " + node.text);
	});
	tree->selectPath({0, 2, 1});

	auto* advanced = column->add<UIExpander>("Import options", true);
	advanced->getContent()->add<UICheckBox>("Generate mipmaps", true);
	column->add<UIExpander>("Collapsed section", false)
		->getContent()
		->add<UILabel>("Hidden until expanded");
}

void buildColorTab(UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setGap(10.0f);
	auto* picker = column->add<UIColorPicker>(Vector4(0.0f, 0.48f, 0.8f, 1.0f));
	auto* line = column->add<UIStack>(UIAxis::Horizontal);
	line->setAlign(UIAlign::Center);
	line->add<UILabel>("Compact button:");
	auto* button = line->add<UIColorButton>(Vector4(0.9f, 0.3f, 0.2f, 1.0f));
	auto* readout = line->add<UILabel>("#E64D33");
	button->setOnColorChanged([readout](const Vector4& c) {
		readout->setText(color::toHex(c));
	});
	picker->setOnChanged([button](const Vector4& c) { button->setColor(c); });
}

void buildRichTab(UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setAlign(UIAlign::Stretch);
	column->setGap(8.0f);

	auto* rangeRow = row(column, "Price");
	auto* range = rangeRow->add<UIRangeSlider>(0.0f, 500.0f, 120.0f, 380.0f);
	range->setStep(10.0f).setMinGap(20.0f);
	range->setWidth(UISizeRule::fill());
	auto* rangeLabel = rangeRow->add<UILabel>("120 - 380");
	rangeLabel->setWidth(UISizeRule::fixed(80.0f));
	range->setOnChanged([rangeLabel](float lo, float hi) {
		rangeLabel->setText(std::to_string(static_cast<int>(lo)) + " - " +
							std::to_string(static_cast<int>(hi)));
	});

	const std::pair<const char*, int> binds[] = {
		{"Jump", 32}, {"Sprint", 340}, {"Interact", 69}};	// GLFW key codes
	for (const auto& [action, key] : binds) {
		row(column, action)->add<UIKeyBindField>(key)->setWidth(UISizeRule::fixed(150.0f));
	}

	auto texture = RESOURCE_MANAGER->loadTexture("assets/textures/waterfall.png");
	auto* split = column->add<UISplitter>(UIAxis::Horizontal, 0.45f);
	split->setHeight(UISizeRule::fill());
	auto* cover = split->getFirst()->add<UIImage>(texture, UIImageFit::Cover);
	cover->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	cover->setCornerRadius(8.0f);
	auto* contain = split->getSecond()->add<UIImage>(texture, UIImageFit::Contain);
	contain->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	contain->setTint(Vector4(1.0f, 0.85f, 0.7f, 1.0f));
	column->add<UILabel>("Drag the divider; double-click it to reset")
		->setColor(Vector4(0.6f, 0.62f, 0.66f, 1.0f));
}

void buildScrollTab(UIObject* page) {
	auto* scroll = page->add<UIScrollView>();
	scroll->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	auto* list = scroll->setContent<UIStack>(UIAxis::Vertical);
	list->setPadding(UIInsets::all(6.0f));
	list->setAlign(UIAlign::Stretch);
	for (int i = 1; i <= 40; i++) {
		auto* line = list->add<UIStack>(UIAxis::Horizontal);
		line->setAlign(UIAlign::Center);
		line->add<UILabel>("Row " + std::to_string(i))->setWidth(UISizeRule::fill());
		line->add<UICheckBox>("", i % 3 == 0);
	}
}

void openGalleryWindow(UIManager* ui) {
	auto* window = ui->getRoot()->add<UIWindow>("UI Gallery", Vector2(640, 540));
	window->setOffset(Vector2(40.0f, 120.0f));
	auto* tabs = window->getContent()->add<UITabView>();
	tabs->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	buildControlsTab(ui, tabs->addTab<UIObject>("Controls"));
	buildChoicesTab(tabs->addTab<UIObject>("Choices"));
	buildTextTab(tabs->addTab<UIObject>("Text"));
	buildListsTab(tabs->addTab<UIObject>("Lists"));
	buildColorTab(tabs->addTab<UIObject>("Color"));
	buildRichTab(tabs->addTab<UIObject>("Rich"));
	buildLayoutTab(tabs->addTab<UIObject>("Layout"));
	buildScrollTab(tabs->addTab<UIObject>("Scroll"));
}

}  // namespace gallery

void buildGallery(bird::UIManager* ui) {
	using namespace bird;
	auto* title = ui->getRoot()->add<UILabel>("Bird Engine");
	title->setFontSize(56.0f);
	title->setOffset(Vector2(40.0f, 44.0f));

	// Menu bar stretched across the top edge.
	auto* bar = ui->getRoot()->add<UIMenuBar>();
	bar->setAnchors(UIAlignment::TopLeft, UIAlignment::TopRight);
	bar->addMenu("File", {UIMenuItem::action("New scene", [] {}, "Ctrl+N"),
						  UIMenuItem::action("Open...", [] {}, "Ctrl+O"),
						  UIMenuItem::submenu("Open recent",
											  {UIMenuItem::action("village.scene", [] {}),
											   UIMenuItem::action("dungeon.scene", [] {})}),
						  UIMenuItem::separator(),
						  UIMenuItem::action("Save", [] {}, "Ctrl+S"),
						  UIMenuItem::action("Export", [] {}).disabled()});
	bar->addMenu("View", {UIMenuItem::toggle("Show grid", true),
						  UIMenuItem::toggle("Wireframe", false),
						  UIMenuItem::separator(),
						  UIMenuItem::action("UI Gallery", [ui] {
							  gallery::openGalleryWindow(ui);
						  })});
	bar->addMenu("Help", {UIMenuItem::action("About", [ui] {
						  auto about = std::make_unique<UIDialog>(
							  "Bird Engine", "GUI framework v2 demo.");
						  about->addButton("Close");
						  ui->pushModal(std::move(about));
					  })});

	// Anchored to the bottom-right corner; follows window resizes.
	auto* reopen = ui->getRoot()->add<UIButton>("Open UI Gallery");
	reopen->setAnchor(UIAlignment::BottomRight);
	reopen->setPivot(UIAlignment::BottomRight);
	reopen->setOffset(Vector2(-16.0f, -16.0f));
	reopen->setOnClick([ui] { gallery::openGalleryWindow(ui); });

	gallery::openGalleryWindow(ui);
}

class MainScript : public bird::Script<bird::Scene> {
   public:
	void init() override {
		bird::INPUT->addAction(bird::Action("move_forward").addKey(KEY_W));
		bird::INPUT->addAction(bird::Action("move_backward").addKey(KEY_S));
		bird::INPUT->addAction(bird::Action("move_left").addKey(KEY_A));
		bird::INPUT->addAction(bird::Action("move_right").addKey(KEY_D));
		bird::INPUT->addAction(bird::Action("move_up").addKey(KEY_SPACE));
		bird::INPUT->addAction(
			bird::Action("move_down").addKey(KEY_LEFT_SHIFT));
		bird::INPUT->addAction(bird::Action("look_up").addKey(KEY_UP));
		bird::INPUT->addAction(bird::Action("look_down").addKey(KEY_DOWN));
		bird::INPUT->addAction(bird::Action("look_left").addKey(KEY_LEFT));
		bird::INPUT->addAction(bird::Action("look_right").addKey(KEY_RIGHT));

		auto e = bird::_own_mk<bird::WorldObject>();
		e->addComponent(bird::_own_mk<bird::MeshComponent>(
			resource_manager()->loadMesh("assets/models/maps/Map1.glb")));
		e->translate(bird::Vector3(0, -5, 30));
		object()->addChild(std::move(e));

		auto e2 = bird::_own_mk<bird::WorldObject>();
		e2->addComponent(bird::_own_mk<bird::MeshComponent>(
			resource_manager()->loadMesh("assets/models/cube.obj")));
		e2->translate(bird::Vector3(0, 5, 0));
		object()->addChild(std::move(e2));

		auto cam = bird::_own_mk<bird::Camera>(1280, 720);
		cam->addComponent(bird::_own_mk<POVCamera>());
		cam->enable();
		object()->addChild(std::move(cam));
		buildGallery(bird::APPLICATION->getUIManager());
	}

	void deinit() override {}

   private:
};

extern "C" BIRD_API_EXPORT bird::Script<bird::Scene>* app_entry() {
	return new MainScript;
}
