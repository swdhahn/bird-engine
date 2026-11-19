#include <Application.h>
#include <physics/CharacterControllerComponent.h>
#include <physics/CollisionShape.h>
#include <physics/PhysicsWorld.h>
#include <physics/RigidBodyComponent.h>
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
#include <scene/Scene.h>
#include <scene/components/ProcessScript.h>
#include <renders/generic/Material.h>
#include <scene/components/LightComponent.h>
#include <scene/components/MeshComponent.h>
#include <scene/components/Script.h>
#include <util/Assets.h>
#include <util/Math.h>

#include <array>
#include <cstdlib>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// True while the camera rides the walking character (toggle with C).
bool g_walkMode = false;

class POVCamera : public bird::ProcessScript<bird::Camera> {
   public:
	POVCamera() {}

	~POVCamera() {}

	void init() {
		bird::WorldEnvironment& env = object()->getWorldEnvironment();
		env.clearColor = bird::Vector3(0.2, 0.6, 0.85);
		env.ambientColor = bird::Vector3(0.55, 0.7, 0.95);
		env.ambientIntensity = 0.35f;
		env.exposure = 1.0f;
		// BIRD_DEBUG_VIEW=1..4 starts in a G-buffer debug view (V cycles).
		if (const char* view = std::getenv("BIRD_DEBUG_VIEW")) {
			env.debugView = std::atoi(view);
		}
	}

	void deinit() {}

	void process(float delta) {
		// V cycles G-buffer debug views: lit, albedo, normal, rough/metal,
		// depth.
		const bool debugViewDown = input()->getActionValue("debug_view") > 0.5f;
		if (debugViewDown && !m_debugViewHeld) {
			bird::WorldEnvironment& env = object()->getWorldEnvironment();
			env.debugView = (env.debugView + 1) % 5;
		}
		m_debugViewHeld = debugViewDown;

		const float speed = 10;
		// Hold the right mouse button to look around (always while walking).
		// The captured cursor is hidden and the UI ignores the pointer.
		const bool look = g_walkMode || input()->getActionValue("look") > 0.5f;
		input()->setMouseCaptured(look);
		if (look) {
			const bird::Vector2 mouse = input()->getMouseDelta();
			m_yaw -= mouse.x * kSensitivity;
			m_pitch = glm::clamp(m_pitch - mouse.y * kSensitivity,
								 glm::radians(-89.0f), glm::radians(89.0f));
		}
		// Yaw about world up, then pitch about the camera's right axis; the
		// camera looks down its local -Z.
		object()->setWorldRotation(glm::angleAxis(m_yaw, bird::Vector3(0, 1, 0)) *
								   glm::angleAxis(m_pitch, bird::Vector3(1, 0, 0)));

		if (g_walkMode) return;	 // the character script moves the camera

		const bird::Vector3 local(
			input()->getActionValue("move_right") -
				input()->getActionValue("move_left"),
			input()->getActionValue("move_up") -
				input()->getActionValue("move_down"),
			input()->getActionValue("move_backward") -
				input()->getActionValue("move_forward"));
		object()->translate(object()->getWorldRotation() *
							object()->getLocalRotation() * local * speed *
							delta);
	}

   private:
	static constexpr float kSensitivity = 0.0025f;	// radians per pixel
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;
	bool m_debugViewHeld = false;
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


// ------------------------------------------------------------- physics demo

namespace playground {

/** A stack of dynamic boxes, a trigger zone and a HUD. F pushes the box under
 * the crosshair (camera forward), R rebuilds the stack. */
class PlaygroundScript : public bird::ProcessScript<bird::WorldObject> {
   public:
	struct Spawn {
		bird::RigidBodyComponent* body;
		bird::Vector3 position;
		bird::Quaternion rotation;
	};

	/** hud: three lines (counts, last event, key help). */
	PlaygroundScript(bird::Camera* camera, std::vector<Spawn> boxes,
					 bird::CharacterControllerComponent* walker,
					 std::array<bird::UILabel*, 3> hud)
		: m_camera(camera), m_boxes(std::move(boxes)), m_walker(walker), m_hud(hud) {
		m_hud[2]->setText("Right mouse: look   F: push   R: drop again   C: walk / fly   "
						  "G: colliders");
	}

	void init() override {}
	void deinit() override {}

	void enterZone(const bird::ContactEvent& e) {
		m_inZone++;
		m_message = e.otherCharacter ? "Trigger: you entered the zone"
									 : "Trigger: a box entered the zone";
	}
	void leaveZone(const bird::ContactEvent& e) {
		m_inZone--;
		m_message = e.otherCharacter ? "Trigger: you left the zone"
									 : "Trigger: a box left the zone";
	}
	/** Character contact events: report bumping into boxes. */
	void walkerTouched(const bird::ContactEvent& e) {
		if (e.other && e.other->getMotionType() == bird::MotionType::Dynamic) {
			m_message = "Character: bumped into a box";
		}
	}

	void process(float delta) override {
		const bool push = input()->getActionValue("physics_push") > 0.5f;
		const bool reset = input()->getActionValue("physics_reset") > 0.5f;
		if (push && !m_pushHeld) pushUnderCrosshair();
		if (reset && !m_resetHeld) resetStack();
		const bool walk = input()->getActionValue("physics_walk") > 0.5f;
		if (walk && !m_walkHeld) toggleWalk();
		m_pushHeld = push;
		m_resetHeld = reset;
		m_walkHeld = walk;
		const bool debug = input()->getActionValue("physics_debug") > 0.5f;
		if (debug && !m_debugHeld) {
			bird::APPLICATION->setPhysicsDebugDraw(
				!bird::APPLICATION->isPhysicsDebugDrawEnabled());
		}
		m_debugHeld = debug;

		if (g_walkMode) {
			// Eye height above the character's feet.
			m_camera->setWorldPosition(m_walker->getInterpolatedPosition() +
									   bird::Vector3(0, 1.6f, 0));
		}

		bird::PhysicsWorld* world = scene()->getPhysicsWorld();
		m_hud[0]->setText("Bodies " + std::to_string(world->getBodyCount()) +
						  "   awake " +
						  std::to_string(world->getActiveBodyCount()) +
						  "   in zone " + std::to_string(m_inZone));
		m_hud[1]->setText(m_message);
	}

	void physicsProcess(float fixedDelta) override {
		if (!g_walkMode) {
			m_walker->setMoveVelocity(bird::Vector3(0.0f));
			return;
		}
		// Steer in the camera's horizontal frame.
		const bird::Quaternion rotation =
			m_camera->getWorldRotation() * m_camera->getLocalRotation();
		bird::Vector3 forward = rotation * bird::Vector3(0, 0, -1);
		forward.y = 0;
		forward = glm::length(forward) > 0 ? glm::normalize(forward) : bird::Vector3(0, 0, -1);
		const bird::Vector3 right(-forward.z, 0, forward.x);
		const float ahead = input()->getActionValue("move_forward") -
							input()->getActionValue("move_backward");
		const float side = input()->getActionValue("move_right") -
						   input()->getActionValue("move_left");
		m_walker->setMoveVelocity((forward * ahead + right * side) * 4.5f);
		if (m_walker->world_object()->getWorldPosition().y < -30.0f) {
			m_walker->teleport(bird::Vector3(0, -1.0f, -8));  // fell off the slab
		}
		if (input()->getActionValue("move_up") > 0.5f) m_walker->jump(5.0f);
	}

   private:
	void toggleWalk() {
		g_walkMode = !g_walkMode;
		if (g_walkMode) {
			// Drop in below the camera, clamped onto the slab (x +-12, z -30..-6).
			const bird::Vector3 eye = m_camera->getWorldPosition();
			m_walker->teleport(bird::Vector3(glm::clamp(eye.x, -11.0f, 11.0f), -1.0f,
											 glm::clamp(eye.z, -29.0f, -7.0f)));
		}
		m_message = g_walkMode ? "Walking: WASD, Space jumps, mouse looks"
							   : "Flying: WASD, Space/Shift up/down";
	}

	void pushUnderCrosshair() {
		const bird::Quaternion rotation =
			m_camera->getWorldRotation() * m_camera->getLocalRotation();
		const bird::Vector3 forward = rotation * bird::Vector3(0, 0, -1);
		const bird::Vector3 eye =
			m_camera->getWorldPosition() + m_camera->getLocalPosition();
		auto hit = scene()->getPhysicsWorld()->raycast(eye, forward, 200.0f,
													   bird::kAllPhysicsLayers, m_walker);
		if (!hit || hit->body == nullptr ||
			hit->body->getMotionType() != bird::MotionType::Dynamic) {
			m_message = "Push: nothing movable under the crosshair";
			return;
		}
		hit->body->applyImpulseAt(forward * 12.0f * hit->body->getMass(),
								  hit->point);
		m_message = "Push: hit at distance " +
					std::to_string(int(hit->distance)) + " m";
	}

	void resetStack() {
		for (const Spawn& box : m_boxes) {
			box.body->teleport(box.position, box.rotation);
			box.body->setLinearVelocity(bird::Vector3(0.0f));
			box.body->setAngularVelocity(bird::Vector3(0.0f));
		}
		m_message = "Boxes dropped again";
	}

	bird::Camera* m_camera;
	std::vector<Spawn> m_boxes;
	bird::CharacterControllerComponent* m_walker;
	std::array<bird::UILabel*, 3> m_hud;
	std::string m_message = "Physics ready";
	int m_inZone = 0;
	bool m_pushHeld = false;
	bool m_resetHeld = false;
	bool m_walkHeld = false;
	bool m_debugHeld = false;
};

/** Adds a mesh object with an optional body to the scene. */
bird::WorldObject* addObject(bird::Scene* scene,
							 const std::vector<std::shared_ptr<bird::Mesh>>& mesh,
							 bird::Vector3 position, bird::Vector3 scale,
							 std::unique_ptr<bird::RigidBodyComponent> body) {
	auto object = bird::_own_mk<bird::WorldObject>();
	if (!mesh.empty()) {
		object->addComponent(bird::_own_mk<bird::MeshComponent>(mesh));
	}
	if (body) object->addComponent(std::move(body));
	object->setWorldPosition(position);
	object->setScale(scale);
	bird::WorldObject* raw = object.get();
	scene->addChild(std::move(object));
	return raw;
}

void build(bird::Scene* scene, bird::Camera* camera, bird::UIManager* ui) {
	using namespace bird;
	auto cube = RESOURCE_MANAGER->loadMesh("assets/models/cube.obj");  // -1..1
	const CollisionShape unitBox = CollisionShape::box(Vector3(1.0f));
	RigidBodySettings still;
	still.motionType = MotionType::Static;

	// Floor slab: top face at y = -2.
	addObject(scene, cube, {0, -2.5f, -18}, {12, 0.5f, 12},
			  _own_mk<RigidBodyComponent>(unitBox, still));

	// 80 half-metre boxes dropped from a height with varied rotations, so
	// they tumble into a pile (deterministic, so R replays the same drop).
	std::vector<PlaygroundScript::Spawn> boxes;
	for (int y = 0; y < 5; y++) {
		for (int x = 0; x < 4; x++) {
			for (int z = 0; z < 4; z++) {
				const int i = (y * 4 + x) * 4 + z;
				const Vector3 position(-2.4f + x * 1.6f, 2.0f + y * 1.8f,
									   -21.0f + z * 1.6f);
				const Quaternion rotation = glm::angleAxis(
					0.37f * i, glm::normalize(Vector3(i % 3, 1, (i * 7) % 5)));
				RigidBodySettings crate;
				crate.mass = 20.0f;	 // light enough for the character to shove
				auto body = _own_mk<RigidBodyComponent>(unitBox, crate);
				RigidBodyComponent* raw = body.get();
				WorldObject* object = addObject(scene, cube, position,
												Vector3(0.5f), std::move(body));
				object->setWorldRotation(rotation);	 // synced at the first step
				boxes.push_back({raw, position, rotation});
			}
		}
	}

	// A chain of five boxes hanging from a world pivot by ball joints (push it with F).
	{
		RigidBodyComponent* previous = nullptr;
		for (int i = 0; i < 5; i++) {
			const Vector3 position(6.0f, 6.0f - i * 1.1f, -14.0f);
			RigidBodySettings link;
			link.mass = 10.0f;
			auto body = _own_mk<RigidBodyComponent>(unitBox, link);
			RigidBodyComponent* raw = body.get();
			addObject(scene, cube, position, Vector3(0.4f), std::move(body));
			scene->getPhysicsWorld()->createPointJoint(raw, previous,
													   position + Vector3(0, 0.55f, 0));
			previous = raw;
		}
	}

	// Invisible trigger zone in front of the stack.
	RigidBodySettings sensor = still;
	sensor.isSensor = true;
	auto zone = _own_mk<RigidBodyComponent>(CollisionShape::box({3, 1, 2}), sensor);
	RigidBodyComponent* zoneBody = zone.get();
	addObject(scene, {}, {0, -1, -12}, Vector3(1.0f), std::move(zone));

	auto* hudColumn = ui->getRoot()->add<UIStack>(UIAxis::Vertical);
	hudColumn->setAnchor(UIAlignment::TopRight);
	hudColumn->setPivot(UIAlignment::TopRight);
	hudColumn->setOffset(Vector2(-16.0f, 44.0f));
	hudColumn->setAlign(UIAlign::End);
	hudColumn->setGap(4.0f);
	std::array<UILabel*, 3> hud{};
	for (UILabel*& line : hud) {
		line = hudColumn->add<UILabel>("");
		line->setColor(Vector4(1.0f, 0.95f, 0.6f, 1.0f));
	}

	// Walking character, parked on the slab until C is pressed.
	CharacterSettings walkerSettings;
	walkerSettings.pushStrength = 600.0f;
	auto walker = _own_mk<CharacterControllerComponent>(walkerSettings);
	CharacterControllerComponent* walkerRaw = walker.get();
	auto walkerObject = _own_mk<WorldObject>();
	walkerObject->addComponent(std::move(walker));
	walkerObject->setWorldPosition({0, -2.0f, -10});
	scene->addChild(std::move(walkerObject));

	auto script = _own_mk<PlaygroundScript>(camera, std::move(boxes), walkerRaw, hud);
	PlaygroundScript* scriptRaw = script.get();
	zoneBody->setOnTriggerEnter(
		[scriptRaw](const ContactEvent& e) { scriptRaw->enterZone(e); });
	zoneBody->setOnTriggerExit(
		[scriptRaw](const ContactEvent& e) { scriptRaw->leaveZone(e); });
	walkerRaw->setOnContactBegin(
		[scriptRaw](const ContactEvent& e) { scriptRaw->walkerTouched(e); });
	auto controller = _own_mk<WorldObject>();
	controller->addComponent(std::move(script));
	scene->addChild(std::move(controller));
}

}  // namespace playground

namespace lighting {

/** Circles its host around a center point (animated point light). */
class OrbitScript : public bird::ProcessScript<bird::WorldObject> {
   public:
	OrbitScript(bird::Vector3 center, float radius, float speed)
		: m_center(center), m_radius(radius), m_speed(speed) {}

	void init() override {}
	void deinit() override {}

	void process(float delta) override {
		m_angle += m_speed * delta;
		object()->setWorldPosition(
			m_center + bird::Vector3(std::cos(m_angle) * m_radius, 0.0f,
									 std::sin(m_angle) * m_radius));
	}

   private:
	bird::Vector3 m_center;
	float m_radius;
	float m_speed;
	float m_angle = 0.0f;
};

/** Adds a light on its own object and returns that object. */
bird::WorldObject* addLight(bird::Scene* scene,
							std::unique_ptr<bird::LightComponent> light,
							bird::Vector3 position,
							bird::Quaternion rotation = bird::Quaternion(1, 0, 0, 0)) {
	auto object = bird::_own_mk<bird::WorldObject>();
	object->addComponent(std::move(light));
	object->setWorldPosition(position);
	object->setWorldRotation(rotation);
	bird::WorldObject* raw = object.get();
	scene->addChild(std::move(object));
	return raw;
}

/** Adds a static mesh object rendered with a material override. */
void addMaterialObject(bird::Scene* scene,
					   const std::vector<std::shared_ptr<bird::Mesh>>& meshes,
					   bird::Vector3 position, bird::Vector3 scale,
					   std::shared_ptr<bird::Material> material) {
	auto mesh = bird::_own_mk<bird::MeshComponent>(meshes);
	mesh->setAllMaterials(std::move(material));
	auto object = bird::_own_mk<bird::WorldObject>();
	object->addComponent(std::move(mesh));
	object->setWorldPosition(position);
	object->setScale(scale);
	scene->addChild(std::move(object));
}

/**
 * Dynamic lighting showcase: a shadowed sun, colored and orbiting point
 * lights, a shadowed spot light, and forward-rendered transparent and unlit
 * materials. Mirrored by `buildLighting` in test/main.dart.
 */
void build(bird::Scene* scene) {
	using namespace bird;
	const Vector3 up(0, 1, 0), right(1, 0, 0);

	auto sun = _own_mk<LightComponent>(LightType::Directional);
	sun->setColor(Vector3(1.0f, 0.95f, 0.85f));
	sun->setIntensity(3.0f);
	sun->setCastsShadows(true);
	addLight(scene, std::move(sun), Vector3(0, 20, 0),
			 glm::angleAxis(0.6f, up) * glm::angleAxis(-0.9f, right));

	const Vector3 playground(0.0f, -1.0f, -18.0f);
	auto red = _own_mk<LightComponent>(LightType::Point);
	red->setColor(Vector3(1.0f, 0.25f, 0.2f));
	red->setIntensity(25.0f);
	red->setRange(9.0f);
	addLight(scene, std::move(red), playground + Vector3(-7.0f, 0.5f, 2.0f));
	auto blue = _own_mk<LightComponent>(LightType::Point);
	blue->setColor(Vector3(0.25f, 0.45f, 1.0f));
	blue->setIntensity(25.0f);
	blue->setRange(9.0f);
	addLight(scene, std::move(blue), playground + Vector3(7.0f, 0.5f, 2.0f));

	auto orbiting = _own_mk<LightComponent>(LightType::Point);
	orbiting->setColor(Vector3(1.0f, 0.85f, 0.6f));
	orbiting->setIntensity(40.0f);
	orbiting->setRange(12.0f);
	orbiting->setCastsShadows(true);
	WorldObject* orbitObject = addLight(scene, std::move(orbiting), playground);
	orbitObject->addComponent(_own_mk<OrbitScript>(
		playground + Vector3(0.0f, 1.5f, 0.0f), 6.0f, 0.8f));

	auto spot = _own_mk<LightComponent>(LightType::Spot);
	spot->setColor(Vector3(0.9f, 1.0f, 0.9f));
	spot->setIntensity(120.0f);
	spot->setRange(25.0f);
	spot->setConeAngles(0.3f, 0.45f);
	spot->setCastsShadows(true);
	addLight(scene, std::move(spot), Vector3(0, 10, -8),
			 glm::angleAxis(-1.0f, right));

	auto cube = RESOURCE_MANAGER->loadMesh("assets/models/cube.obj");
	auto glass = Material::create();
	glass->setBaseColor(Vector4(0.6f, 0.85f, 1.0f, 0.35f))
		.setRoughness(0.1f)
		.setBlendMode(BlendMode::Transparent);
	addMaterialObject(scene, cube, Vector3(5, -1, -11), Vector3(1.0f), glass);

	auto neon = Material::create();
	neon->setBaseColor(Vector4(0, 0, 0, 1))
		.setEmissive(Vector3(1.0f, 0.3f, 0.9f))
		.setEmissiveStrength(4.0f)
		.setDomain(MaterialDomain::Unlit);
	addMaterialObject(scene, cube, Vector3(-5, -1, -11), Vector3(0.6f), neon);

	auto metal = Material::create();
	metal->setBaseColor(Vector4(0.95f, 0.64f, 0.54f, 1.0f))
		.setMetallic(1.0f)
		.setRoughness(0.3f);
	addMaterialObject(scene, cube, Vector3(0, -1, -11), Vector3(0.8f), metal);
}

}  // namespace lighting

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
		bird::INPUT->addAction(
			bird::Action("look").addMouseButton(MOUSE_BUTTON_2));  // right button
		bird::INPUT->addAction(bird::Action("physics_debug").addKey(KEY_G));
		bird::INPUT->addAction(bird::Action("physics_push").addKey(KEY_F));
		bird::INPUT->addAction(bird::Action("physics_reset").addKey(KEY_R));
		bird::INPUT->addAction(bird::Action("physics_walk").addKey(KEY_C));
		bird::INPUT->addAction(bird::Action("debug_view").addKey(KEY_V));

		// The village map: rendered as one object, collided as a few static
		// triangle-mesh chunks (3 x 3 grid) sharing its transform.
		auto mapMeshes = resource_manager()->loadMesh("assets/models/maps/Map1.glb");
		const bird::Vector3 mapPosition(0, -5, 30);
		auto e = bird::_own_mk<bird::WorldObject>();
		e->addComponent(bird::_own_mk<bird::MeshComponent>(mapMeshes));
		e->translate(mapPosition);
		object()->addChild(std::move(e));
		bird::RigidBodySettings level;
		level.motionType = bird::MotionType::Static;
		for (const bird::CollisionShape& chunk :
			 bird::CollisionShape::triangleMeshChunks(mapMeshes, 3, 3)) {
			auto collider = bird::_own_mk<bird::WorldObject>();
			collider->addComponent(bird::_own_mk<bird::RigidBodyComponent>(chunk, level));
			collider->setWorldPosition(mapPosition);
			object()->addChild(std::move(collider));
		}

		auto cam = bird::_own_mk<bird::Camera>(1280, 720);
		cam->addComponent(bird::_own_mk<POVCamera>());
		cam->enable();
		bird::Camera* camera = cam.get();
		object()->addChild(std::move(cam));
		buildGallery(bird::APPLICATION->getUIManager());
		lighting::build(object());
		playground::build(object(), camera, bird::APPLICATION->getUIManager());
		// BIRD_PHYSICS_DEBUG=1 starts with collider wireframes on (G toggles).
		if (std::getenv("BIRD_PHYSICS_DEBUG") != nullptr) {
			bird::APPLICATION->setPhysicsDebugDraw(true);
		}
	}

	void deinit() override {}

   private:
};

extern "C" BIRD_API_EXPORT bird::Script<bird::Scene>* app_entry() {
	return new MainScript;
}
