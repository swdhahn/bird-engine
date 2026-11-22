#include <Application.h>
#include <animation/skeletal/BlendTree.h>
#include <animation/skeletal/PoseOps.h>
#include <animation/skeletal/SkeletalAnimationComponent.h>
#include <audio/AudioListenerComponent.h>
#include <audio/AudioSourceComponent.h>
#include <audio/AudioSystem.h>
#include <diagnostics/Logger.h>
#include <diagnostics/Profiler.h>
#include <editor/LevelEditor.h>
#include <events/EventBus.h>
#include <navigation/NavAgent.h>
#include <navigation/NavMesh.h>
#include <navigation/NavWorld.h>
#include <renders/GraphicsPipeline.h>
#include <renders/generic/EnvironmentMap.h>
#include <scene/components/ParticleEmitter.h>
#include <serialization/Json.h>
#include <serialization/SaveGame.h>
#include <serialization/SceneSerializer.h>
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
#include <util/ResourceManager.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// True while the camera rides the walking character (toggle with C).
bool g_walkMode = false;

/** Horizontal screen margins for HUD overlays (wider beside editor panels). */
struct HudInsets {
	float left = 16.0f;
	float right = 16.0f;
};

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

void build(bird::Scene* scene, bird::Camera* camera, bird::UIManager* ui,
		   const HudInsets& insets) {
	using namespace bird;
	auto cube = RESOURCE_MANAGER->loadMesh("assets/models/cube.obj");  // -1..1
	const CollisionShape unitBox = CollisionShape::box(Vector3(1.0f));
	RigidBodySettings still;
	still.motionType = MotionType::Static;

	// Floor slab: top face at y = -2.
	addObject(scene, cube, {0, -2.5f, -18}, {12, 0.5f, 12},
			  _own_mk<RigidBodyComponent>(unitBox, still))
		->setName("Floor Slab");

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
				object->setName("Crate " + std::to_string(i + 1));
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
			addObject(scene, cube, position, Vector3(0.4f), std::move(body))
				->setName("Chain Link " + std::to_string(i + 1));
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
	addObject(scene, {}, {0, -1, -12}, Vector3(1.0f), std::move(zone))
		->setName("Trigger Zone");

	auto* hudColumn = ui->getRoot()->add<UIStack>(UIAxis::Vertical);
	hudColumn->setAnchor(UIAlignment::TopRight);
	hudColumn->setPivot(UIAlignment::TopRight);
	hudColumn->setOffset(Vector2(-insets.right, 44.0f));
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
	walkerObject->setName("Walker (character controller)");
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
	controller->setName("Playground Controller");
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

/** Adds a named static mesh object rendered with a material override. */
void addMaterialObject(bird::Scene* scene, const std::string& name,
					   const std::vector<std::shared_ptr<bird::Mesh>>& meshes,
					   bird::Vector3 position, bird::Vector3 scale,
					   std::shared_ptr<bird::Material> material) {
	auto mesh = bird::_own_mk<bird::MeshComponent>(meshes);
	mesh->setAllMaterials(std::move(material));
	auto object = bird::_own_mk<bird::WorldObject>();
	object->setName(name);
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
			 glm::angleAxis(0.6f, up) * glm::angleAxis(-0.9f, right))
		->setName("Sun");

	const Vector3 playground(0.0f, -1.0f, -18.0f);
	auto red = _own_mk<LightComponent>(LightType::Point);
	red->setColor(Vector3(1.0f, 0.25f, 0.2f));
	red->setIntensity(25.0f);
	red->setRange(9.0f);
	addLight(scene, std::move(red), playground + Vector3(-7.0f, 0.5f, 2.0f))
		->setName("Red Point Light");
	auto blue = _own_mk<LightComponent>(LightType::Point);
	blue->setColor(Vector3(0.25f, 0.45f, 1.0f));
	blue->setIntensity(25.0f);
	blue->setRange(9.0f);
	addLight(scene, std::move(blue), playground + Vector3(7.0f, 0.5f, 2.0f))
		->setName("Blue Point Light");

	auto orbiting = _own_mk<LightComponent>(LightType::Point);
	orbiting->setColor(Vector3(1.0f, 0.85f, 0.6f));
	orbiting->setIntensity(40.0f);
	orbiting->setRange(12.0f);
	orbiting->setCastsShadows(true);
	WorldObject* orbitObject = addLight(scene, std::move(orbiting), playground);
	orbitObject->setName("Orbiting Light");
	orbitObject->addComponent(_own_mk<OrbitScript>(
		playground + Vector3(0.0f, 1.5f, 0.0f), 6.0f, 0.8f));

	auto spot = _own_mk<LightComponent>(LightType::Spot);
	spot->setColor(Vector3(0.9f, 1.0f, 0.9f));
	spot->setIntensity(120.0f);
	spot->setRange(25.0f);
	spot->setConeAngles(0.3f, 0.45f);
	spot->setCastsShadows(true);
	addLight(scene, std::move(spot), Vector3(0, 10, -8),
			 glm::angleAxis(-1.0f, right))
		->setName("Spot Light");

	auto cube = RESOURCE_MANAGER->loadMesh("assets/models/cube.obj");
	auto glass = Material::create();
	glass->setBaseColor(Vector4(0.6f, 0.85f, 1.0f, 0.35f))
		.setRoughness(0.1f)
		.setBlendMode(BlendMode::Transparent);
	addMaterialObject(scene, "Glass Cube", cube, Vector3(5, -1, -11), Vector3(1.0f), glass);

	auto neon = Material::create();
	neon->setBaseColor(Vector4(0, 0, 0, 1))
		.setEmissive(Vector3(1.0f, 0.3f, 0.9f))
		.setEmissiveStrength(4.0f)
		.setDomain(MaterialDomain::Unlit);
	addMaterialObject(scene, "Neon Cube", cube, Vector3(-5, -1, -11), Vector3(0.6f), neon);

	auto metal = Material::create();
	metal->setBaseColor(Vector4(0.95f, 0.64f, 0.54f, 1.0f))
		.setMetallic(1.0f)
		.setRoughness(0.3f);
	addMaterialObject(scene, "Copper Cube", cube, Vector3(0, -1, -11), Vector3(0.8f), metal);
}

}  // namespace lighting

namespace animation {

constexpr const char* kKnightModel = "assets/models/Character_Skeleton_Knight.gltf";
/** The pack holds 16 characters on one rig; mesh 11 is the skeleton knight. */
constexpr size_t kKnightMeshIndex = 11;
constexpr float kCycleSeconds = 1.1f;
constexpr int kKeysPerCycle = 32;
constexpr float kPi = 3.14159265358979f;

float radians(float degrees) { return degrees * kPi / 180.0f; }

/** Fails fast when the rig lacks a joint the walk cycle drives. */
int32_t requireJoint(const bird::Skeleton& skeleton, const std::string& name) {
	const int32_t joint = skeleton.findJoint(name);
	if (joint < 0) {
		throw std::runtime_error("animation demo: rig has no joint '" + name + "'");
	}
	return joint;
}

/**
 * Rest-pose context for authoring rotations in model space: a model-space
 * rotation R on joint j becomes the local value P^-1 * R * P * L, where P
 * is the parent's rest model rotation and L the joint's rest rotation.
 */
class RestFrame {
   public:
	explicit RestFrame(const bird::Skeleton& skeleton) : m_skeleton(skeleton) {
		bird::localToModel(skeleton, skeleton.getRestPose(), m_model);
	}

	bird::Vector3 modelPosition(int32_t joint) const {
		return bird::Vector3(m_model[joint][3]);
	}

	/** Local rotation that applies modelRotation at the joint. */
	bird::Quaternion localRotation(int32_t joint,
								   const bird::Quaternion& modelRotation) const {
		const bird::Quaternion parent = parentRotation(joint);
		return glm::normalize(glm::inverse(parent) * modelRotation * parent *
							  m_skeleton.getRestPose().joints[joint].rotation);
	}

	/** Local translation that offsets the joint by a model-space vector. */
	bird::Vector3 localTranslation(int32_t joint, const bird::Vector3& offset) const {
		const int32_t parentJoint = m_skeleton.getParent(joint);
		const bird::Vector3 rest = m_skeleton.getRestPose().joints[joint].translation;
		if (parentJoint < 0) return rest + offset;
		const glm::mat3 parentLinear(m_model[parentJoint]);
		return rest + glm::inverse(parentLinear) * offset;
	}

   private:
	bird::Quaternion parentRotation(int32_t joint) const {
		const int32_t parentJoint = m_skeleton.getParent(joint);
		if (parentJoint < 0) return bird::Quaternion(1, 0, 0, 0);
		glm::mat3 linear(m_model[parentJoint]);
		for (int axis = 0; axis < 3; axis++) linear[axis] = glm::normalize(linear[axis]);
		return glm::quat_cast(linear);
	}

	const bird::Skeleton& m_skeleton;
	std::vector<bird::Matrix4> m_model;
};

/** Model-space axis rotation shorthand. */
bird::Quaternion around(float degrees, const bird::Vector3& axis) {
	return glm::angleAxis(radians(degrees), axis);
}

/** Samples rotationAt(phase) over one cycle into a looping joint track. */
template <typename RotationAt>
bird::JointTrack rotationTrack(const RestFrame& rest, int32_t joint,
							   RotationAt rotationAt) {
	bird::JointTrack track;
	track.joint = joint;
	for (int key = 0; key <= kKeysPerCycle; key++) {
		const float phase = 2.0f * kPi * key / kKeysPerCycle;
		track.rotation.addKey(kCycleSeconds * key / kKeysPerCycle,
							  rest.localRotation(joint, rotationAt(phase)));
	}
	return track;
}

/**
 * Procedural walk for the pack's T-posed rig: legs swing with knee lift,
 * arms drop to the sides and counter-swing, hips bob and twist against the
 * spine. Sides and facing are measured from the rest pose rather than
 * assumed, so the cycle holds for either axis convention.
 *
 * @param forwardSign Receives +1 when the rig faces +Z, -1 for -Z.
 * @throws std::runtime_error when a required joint is missing.
 */
std::shared_ptr<bird::SkeletalClip> buildWalkClip(const bird::Skeleton& skeleton,
												  float& forwardSign) {
	using bird::Vector3;
	const RestFrame rest(skeleton);
	const Vector3 xAxis(1, 0, 0), yAxis(0, 1, 0), zAxis(0, 0, 1);
	forwardSign = rest.modelPosition(requireJoint(skeleton, "Toes_L")).z >
						  rest.modelPosition(requireJoint(skeleton, "Ankle_L")).z
					  ? 1.0f
					  : -1.0f;
	const float f = forwardSign;
	auto clip = std::make_shared<bird::SkeletalClip>("Walk", kCycleSeconds);

	struct Side {
		const char* upperLeg;
		const char* lowerLeg;
		const char* shoulder;
		const char* elbow;
		const char* hand;
		float phaseOffset;
	};
	const Side sides[] = {
		{"UpperLeg_L", "LowerLeg_L", "Shoulder_L", "Elbow_L", "Hand_L", 0.0f},
		{"UpperLeg_R", "LowerLeg_R", "Shoulder_R", "Elbow_R", "Hand_R", kPi},
	};
	for (const Side& side : sides) {
		const float offset = side.phaseOffset;
		const int32_t shoulder = requireJoint(skeleton, side.shoulder);
		const float armSide =
			rest.modelPosition(requireJoint(skeleton, side.hand)).x >
					rest.modelPosition(shoulder).x
				? 1.0f
				: -1.0f;

		clip->addTrack(rotationTrack(
			rest, requireJoint(skeleton, side.upperLeg), [=](float phase) {
				return around(-f * 26.0f * std::sin(phase + offset), xAxis);
			}));
		clip->addTrack(rotationTrack(
			rest, requireJoint(skeleton, side.lowerLeg), [=](float phase) {
				const float lift = std::max(0.0f, std::cos(phase + offset));
				return around(f * (6.0f + 45.0f * lift), xAxis);
			}));
		clip->addTrack(rotationTrack(rest, shoulder, [=](float phase) {
			const float swing = 18.0f * std::sin(phase + offset);
			return around(f * swing, xAxis) * around(-armSide * 74.0f, zAxis);
		}));
		clip->addTrack(rotationTrack(
			rest, requireJoint(skeleton, side.elbow), [=](float phase) {
				const float bend = 14.0f + 12.0f * std::max(0.0f, -std::sin(phase + offset));
				return around(-armSide * f * bend, yAxis);
			}));
	}

	const int32_t hips = requireJoint(skeleton, "Hips");
	bird::JointTrack hipsTrack =
		rotationTrack(rest, hips, [=](float phase) { return around(6.0f * std::sin(phase), yAxis); });
	for (int key = 0; key <= kKeysPerCycle; key++) {
		const float phase = 2.0f * kPi * key / kKeysPerCycle;
		const float bob = 0.035f * std::cos(2.0f * phase);
		hipsTrack.translation.addKey(kCycleSeconds * key / kKeysPerCycle,
									 rest.localTranslation(hips, Vector3(0, bob, 0)));
	}
	clip->addTrack(std::move(hipsTrack));
	clip->addTrack(rotationTrack(rest, requireJoint(skeleton, "Spine_02"), [=](float phase) {
		return around(-9.0f * std::sin(phase), yAxis);
	}));
	clip->addTrack(rotationTrack(rest, requireJoint(skeleton, "Head"), [=](float phase) {
		return around(f * 3.0f * std::cos(2.0f * phase), xAxis);
	}));
	return clip;
}

/** Walks its host around a circle, facing along the path. */
class CircleWalkScript : public bird::ProcessScript<bird::WorldObject> {
   public:
	CircleWalkScript(bird::Vector3 center, float radius, float speed, float forwardSign)
		: m_center(center),
		  m_radius(radius),
		  m_angularSpeed(speed / radius),
		  m_baseYaw(forwardSign > 0.0f ? 0.0f : kPi) {}

	/** Places the host on its circle before the first (possibly paused) frame. */
	void init() override { process(0.0f); }
	void deinit() override {}

	void process(float delta) override {
		m_angle += m_angularSpeed * delta;
		object()->setWorldPosition(
			m_center + bird::Vector3(std::cos(m_angle) * m_radius, 0.0f,
									 std::sin(m_angle) * m_radius));
		object()->setWorldRotation(
			glm::angleAxis(m_baseYaw - m_angle, bird::Vector3(0, 1, 0)));
	}

   private:
	bird::Vector3 m_center;
	float m_radius;
	float m_angularSpeed;
	float m_baseYaw;
	float m_angle = 0.0f;
};

/** The knight's skinned mesh and a private copy of its baked rig. */
struct KnightRig {
	std::shared_ptr<bird::Mesh> mesh;
	std::shared_ptr<bird::SkeletalAsset> asset;
};

/**
 * Loads the skeleton knight from the bundled character pack.
 *
 * @return The skinned knight mesh and a mutable copy of its skeletal asset.
 * @throws std::runtime_error when the model, its skeleton bake, or the
 *         knight mesh is missing.
 */
KnightRig loadKnight() {
	const auto meshes = bird::RESOURCE_MANAGER->loadMesh(kKnightModel);
	const auto baked = bird::RESOURCE_MANAGER->loadSkeletalAsset(kKnightModel);
	if (!baked || meshes.size() <= kKnightMeshIndex ||
		!meshes[kKnightMeshIndex]->isSkinned()) {
		throw std::runtime_error("animation demo: knight model or skeleton bake missing");
	}
	return {meshes[kKnightMeshIndex], std::make_shared<bird::SkeletalAsset>(*baked)};
}

/**
 * Procedural idle for the same rig: arms rest at the sides with a slight
 * sway while the chest and head breathe. Shares the walk clip's length so a
 * 1D blend tree can keep both cycles phase-aligned.
 *
 * @param forwardSign +1 when the rig faces +Z, -1 for -Z (from buildWalkClip).
 * @throws std::runtime_error when a required joint is missing.
 */
std::shared_ptr<bird::SkeletalClip> buildIdleClip(const bird::Skeleton& skeleton,
												  float forwardSign) {
	using bird::Vector3;
	const RestFrame rest(skeleton);
	const Vector3 xAxis(1, 0, 0), zAxis(0, 0, 1);
	const float f = forwardSign;
	auto clip = std::make_shared<bird::SkeletalClip>("Idle", kCycleSeconds);
	const std::pair<const char*, const char*> arms[] = {{"Shoulder_L", "Hand_L"},
														{"Shoulder_R", "Hand_R"}};
	for (const auto& [shoulderName, handName] : arms) {
		const int32_t shoulder = requireJoint(skeleton, shoulderName);
		const float armSide = rest.modelPosition(requireJoint(skeleton, handName)).x >
									  rest.modelPosition(shoulder).x
								  ? 1.0f
								  : -1.0f;
		clip->addTrack(rotationTrack(rest, shoulder, [=](float phase) {
			return around(f * 2.0f * std::sin(phase), xAxis) *
				   around(-armSide * (76.0f + 1.5f * std::cos(phase)), zAxis);
		}));
	}
	clip->addTrack(rotationTrack(rest, requireJoint(skeleton, "Spine_02"), [=](float phase) {
		return around(-f * 2.5f * std::sin(phase), xAxis);
	}));
	clip->addTrack(rotationTrack(rest, requireJoint(skeleton, "Head"), [=](float phase) {
		return around(f * 2.0f * std::sin(phase + 0.6f), xAxis);
	}));
	return clip;
}

/**
 * Skinned character showcase: the skeleton knight from the bundled pack,
 * driven by a procedural walk clip while circling the playground floor.
 *
 * @throws std::runtime_error when the knight cannot be loaded.
 */
void build(bird::Scene* scene) {
	using namespace bird;
	KnightRig rig = loadKnight();
	float forwardSign = 1.0f;
	rig.asset->clips.push_back(buildWalkClip(*rig.asset->skeleton, forwardSign));
	auto animator = _own_mk<SkeletalAnimationComponent>(
		std::shared_ptr<const SkeletalAsset>(std::move(rig.asset)));
	animator->play("Walk");

	auto knight = _own_mk<WorldObject>();
	knight->setName("Circling Knight");
	knight->addComponent(_own_mk<MeshComponent>(std::vector<std::shared_ptr<Mesh>>{rig.mesh}));
	knight->addComponent(std::move(animator));
	knight->addComponent(_own_mk<CircleWalkScript>(Vector3(-3.0f, -2.0f, -8.2f),
												   1.4f, 1.2f, forwardSign));
	scene->addChild(std::move(knight));
}

}  // namespace animation

namespace environment {

/** Sun orientation shared by the directional light and the sky's sun disc. */
bird::Quaternion sunRotation() {
	return glm::angleAxis(0.6f, bird::Vector3(0, 1, 0)) *
		   glm::angleAxis(-0.9f, bird::Vector3(1, 0, 0));
}

/**
 * Procedural sky rendered as the skybox and baked into image-based lighting
 * (irradiance + prefiltered specular) for every lit surface the camera sees.
 * Device builds bake smaller cubemaps to keep startup short.
 */
void build(bird::Camera* camera) {
	bird::SkyboxProceduralParams sky;
	// The sun disc sits where the directional light shines from.
	sky.sunDirection = glm::normalize(-(sunRotation() * bird::Vector3(0, 0, -1)));
	bird::IBLQualitySettings quality;
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
	quality.environmentSize = 128;
	quality.irradianceSize = 16;
	quality.prefilterSize = 64;
#endif
	bird::WorldEnvironment& env = camera->getWorldEnvironment();
	env.environmentMap = bird::EnvironmentMap::createProcedural(sky, quality);
	env.renderSkybox = true;
	env.iblIntensity = 1.0f;
}

}  // namespace environment

namespace effects {

constexpr const char* kCampfireSound = "assets/audio/campfire.wav";

/** Modulates its light's intensity like firelight. */
class FlickerScript : public bird::ProcessScript<bird::WorldObject> {
   public:
	FlickerScript(bird::LightComponent* light, float baseIntensity)
		: m_light(light), m_baseIntensity(baseIntensity) {}

	void init() override {}
	void deinit() override {}

	void process(float delta) override {
		m_time += delta;
		const float wave = 0.8f + 0.12f * std::sin(m_time * 9.0f) +
						   0.08f * std::sin(m_time * 23.0f + 1.3f);
		m_light->setIntensity(m_baseIntensity * wave);
	}

   private:
	bird::LightComponent* m_light;
	float m_baseIntensity;
	float m_time = 0.0f;
};

/** Places an emitter on its own named object and returns it. */
bird::ParticleEmitter* addEmitter(bird::Scene* scene, const std::string& name,
								  bird::Vector3 position,
								  std::unique_ptr<bird::ParticleEmitter> emitter) {
	bird::ParticleEmitter* raw = emitter.get();
	auto object = bird::_own_mk<bird::WorldObject>();
	object->setName(name);
	object->addComponent(std::move(emitter));
	object->setWorldPosition(position);
	scene->addChild(std::move(object));
	return raw;
}

/**
 * Campfire (additive flames, alpha smoke, a flickering point light and a
 * looping 3D crackle) plus a spark fountain that bursts on demand.
 *
 * @return The spark emitter, burst by the showcase controls.
 */
bird::ParticleEmitter* build(bird::Scene* scene) {
	using namespace bird;
	const Vector3 fire(-8.0f, -2.0f, -24.0f);

	auto flames = _own_mk<ParticleEmitter>(192);
	flames->setSpawnRate(70.0f);
	flames->setLifetimeRange(0.45f, 0.9f);
	flames->setInitialSpeedRange(0.8f, 1.8f);
	flames->setEmissionCone(Vector3(0, 1, 0), 0.3f);
	flames->setGravity(Vector3(0.0f, 1.8f, 0.0f));
	flames->setColorGradient(Vector4(1.0f, 0.8f, 0.3f, 1.0f),
							 Vector4(0.9f, 0.15f, 0.02f, 0.0f));
	flames->setSizeRange(0.55f, 0.1f);
	flames->setBlendMode(ParticleBlendMode::Additive);
	addEmitter(scene, "Campfire Flames", fire, std::move(flames));

	auto smoke = _own_mk<ParticleEmitter>(96);
	smoke->setSpawnRate(10.0f);
	smoke->setLifetimeRange(2.5f, 4.0f);
	smoke->setInitialSpeedRange(0.6f, 1.1f);
	smoke->setEmissionCone(Vector3(0, 1, 0), 0.2f);
	smoke->setGravity(Vector3(0.3f, 0.4f, 0.0f));
	smoke->setColorGradient(Vector4(0.35f, 0.35f, 0.38f, 0.45f),
							Vector4(0.6f, 0.6f, 0.65f, 0.0f));
	smoke->setSizeRange(0.4f, 1.6f);
	smoke->setPositionOffset(Vector3(0.0f, 1.0f, 0.0f));
	addEmitter(scene, "Campfire Smoke", fire, std::move(smoke));

	auto glow = _own_mk<LightComponent>(LightType::Point);
	glow->setColor(Vector3(1.0f, 0.55f, 0.2f));
	glow->setRange(9.0f);
	LightComponent* glowRaw = glow.get();
	auto crackle = _own_mk<AudioSourceComponent>(kCampfireSound);
	crackle->setLooping(true);
	crackle->setSpatialized(true);
	crackle->setMinDistance(2.0f);
	crackle->setMaxDistance(35.0f);
	crackle->setPlayOnInit(true);
	auto fireObject = _own_mk<WorldObject>();
	fireObject->setName("Campfire Light & Sound");
	fireObject->addComponent(std::move(glow));
	fireObject->addComponent(_own_mk<FlickerScript>(glowRaw, 30.0f));
	fireObject->addComponent(std::move(crackle));
	fireObject->setWorldPosition(fire + Vector3(0.0f, 0.8f, 0.0f));
	scene->addChild(std::move(fireObject));

	auto sparks = _own_mk<ParticleEmitter>(512);
	sparks->setSpawnRate(0.0f);
	sparks->setLifetimeRange(1.2f, 2.2f);
	sparks->setInitialSpeedRange(5.0f, 9.0f);
	sparks->setEmissionCone(Vector3(0, 1, 0), 0.35f);
	sparks->setColorGradient(Vector4(0.6f, 0.9f, 1.0f, 1.0f),
							 Vector4(0.2f, 0.3f, 1.0f, 0.0f));
	sparks->setSizeRange(0.18f, 0.04f);
	sparks->setDrag(0.4f);
	sparks->setBlendMode(ParticleBlendMode::Additive);
	return addEmitter(scene, "Spark Fountain", Vector3(8.0f, -2.0f, -24.0f),
					  std::move(sparks));
}

}  // namespace effects

namespace patrol {

constexpr const char* kFootstepSound = "assets/audio/footstep.wav";
constexpr const char* kFootstepEvent = "demo.footstep";
/** Agent top speed, which is also the blend tree's full-walk threshold. */
constexpr float kWalkSpeed = 1.4f;
constexpr float kWaitSeconds = 1.5f;

/**
 * Walks a NavAgent between waypoints, pausing at each, and feeds the
 * agent's ground speed into the animator's "speed" blend parameter.
 */
class PatrolScript : public bird::ProcessScript<bird::WorldObject> {
   public:
	PatrolScript(bird::NavAgent* agent, bird::SkeletalAnimationComponent* animator,
				 std::vector<bird::Vector3> waypoints, float forwardSign)
		: m_agent(agent),
		  m_animator(animator),
		  m_waypoints(std::move(waypoints)),
		  m_baseYaw(forwardSign > 0.0f ? 0.0f : animation::kPi) {}

	void init() override { m_agent->setDestination(m_waypoints[m_next]); }
	void deinit() override {}

	void process(float delta) override {
		if (m_agent->hasReachedDestination()) {
			m_wait += delta;
			if (m_wait >= kWaitSeconds) {
				m_wait = 0.0f;
				m_next = (m_next + 1) % m_waypoints.size();
				m_agent->setDestination(m_waypoints[m_next]);
			}
		}
		bird::Vector3 velocity = m_agent->getVelocity();
		velocity.y = 0.0f;
		const float speed = glm::length(velocity);
		m_animator->setFloat("speed", speed);
		// Face the direction of travel; the rig's forward axis may be -Z.
		if (speed > 0.15f) {
			const float yaw = std::atan2(velocity.x, velocity.z) + m_baseYaw;
			const bird::Quaternion target = glm::angleAxis(yaw, bird::Vector3(0, 1, 0));
			object()->setWorldRotation(
				glm::slerp(object()->getWorldRotation(), target,
						   std::min(1.0f, delta * 8.0f)));
		}
	}

   private:
	bird::NavAgent* m_agent;
	bird::SkeletalAnimationComponent* m_animator;
	std::vector<bird::Vector3> m_waypoints;
	float m_baseYaw;
	size_t m_next = 0;
	float m_wait = 0.0f;
};

/**
 * Bakes a navmesh over the playground slab and adds a patrolling guard:
 * a NavAgent steers it, a 1D blend tree mixes Idle and Walk by speed, and
 * Walk's foot-plant notifies play 3D footsteps and post "demo.footstep" on
 * the event bus.
 *
 * @throws std::runtime_error when the knight cannot be loaded.
 */
void build(bird::Scene* scene) {
	using namespace bird;
	// The slab's top face (y = -2), wound counter-clockwise seen from above.
	const std::vector<Vector3> floor = {
		{-12.0f, -2.0f, -30.0f}, {12.0f, -2.0f, -30.0f},
		{12.0f, -2.0f, -6.0f},	 {-12.0f, -2.0f, -6.0f}};
	const std::vector<uint32_t> triangles = {0, 2, 1, 0, 3, 2};
	scene->getNavWorld()->setNavMesh(NavMesh::build(floor, triangles));

	animation::KnightRig rig = animation::loadKnight();
	float forwardSign = 1.0f;
	auto walk = animation::buildWalkClip(*rig.asset->skeleton, forwardSign);
	auto idle = animation::buildIdleClip(*rig.asset->skeleton, forwardSign);
	rig.asset->clips.push_back(walk);
	rig.asset->clips.push_back(idle);

	auto tree = std::make_shared<BlendTree1D>("speed");
	tree->addClip(idle, 0.0f);
	tree->addClip(walk, kWalkSpeed);
	auto animator = _own_mk<SkeletalAnimationComponent>(
		std::shared_ptr<const SkeletalAsset>(rig.asset));
	animator->setRootNode(tree);
	// Each leg plants once per cycle, half a cycle apart.
	animator->addClipNotify("Walk", 0.0f, "footstep");
	animator->addClipNotify("Walk", animation::kCycleSeconds * 0.5f, "footstep");
	SkeletalAnimationComponent* animatorRaw = animator.get();

	NavAgentSettings agentSettings;
	agentSettings.maxSpeed = kWalkSpeed;
	agentSettings.radius = 0.5f;
	agentSettings.updateRotation = false;  // PatrolScript turns the rig
	auto agent = _own_mk<NavAgent>(agentSettings);
	NavAgent* agentRaw = agent.get();

	auto guard = _own_mk<WorldObject>();
	guard->setName("Patrolling Guard");
	WorldObject* guardRaw = guard.get();
	animator->setOnNotify([guardRaw, animatorRaw](const AnimationNotifyEvent& e) {
		// The blend tree reports Walk's notifies even at low weight.
		if (e.eventName != "footstep" || animatorRaw->getFloat("speed") < 0.4f) return;
		const Vector3 position = guardRaw->getWorldPosition();
		if (AudioSystem* audio = AudioSystem::getGlobal()) {
			audio->playOneShot3D(kFootstepSound, position, AudioBus::kSFX, 0.7f);
		}
		if (EventBus* bus = EventBus::getGlobal()) {
			EventPayload payload;
			payload.set("position", position);
			bus->emit(kFootstepEvent, std::move(payload));
		}
	});
	guard->addComponent(_own_mk<MeshComponent>(std::vector<std::shared_ptr<Mesh>>{rig.mesh}));
	guard->addComponent(std::move(animator));
	guard->addComponent(std::move(agent));
	guard->addComponent(_own_mk<PatrolScript>(
		agentRaw, animatorRaw,
		std::vector<Vector3>{{-9.0f, -2.0f, -27.0f},
							 {9.0f, -2.0f, -27.0f},
							 {9.0f, -2.0f, -13.0f},
							 {-9.0f, -2.0f, -13.0f}},
		forwardSign));
	guard->setWorldPosition(Vector3(-9.0f, -2.0f, -13.0f));
	scene->addChild(std::move(guard));
}

}  // namespace patrol

namespace showcase {

constexpr const char* kAppName = "BirdShowcase";
constexpr const char* kSceneSnapshotFile = "scene_snapshot.json";
constexpr const char* kCameraSaveFile = "camera.json";
constexpr const char* kImpactSound = "assets/audio/impact.wav";
constexpr float kImpactCooldownSeconds = 0.12f;
constexpr size_t kHudLines = 5;

/**
 * Engine-systems overlay and controls: culling, profiler, event bus and
 * audio readouts, plus navmesh debug draw, spark bursts, muting and
 * save/load through the serialization module.
 */
class ShowcaseScript : public bird::ProcessScript<bird::WorldObject> {
   public:
	ShowcaseScript(bird::Camera* camera, bird::ParticleEmitter* sparks,
				   std::array<bird::UILabel*, kHudLines> hud)
		: m_camera(camera), m_sparks(sparks), m_hud(hud) {
		m_hud[kHudLines - 1]->setText(
			"N: navmesh   B: sparks   M: mute   F9: save   F10: load camera");
	}

	void init() override {
		bird::EventBus* bus = bird::EventBus::getGlobal();
		if (bus == nullptr) return;
		// Owned by this entity: the bus drops both when it is destroyed.
		const bird::EntityID owner = object()->getID();
		bus->subscribe(
			"physics.contact_begin",
			[this](const bird::EventPayload& payload) { onContact(payload); }, owner);
		bus->subscribe(
			patrol::kFootstepEvent,
			[this](const bird::EventPayload&) { m_footsteps++; }, owner);
	}

	void deinit() override {}

	void process(float delta) override {
		m_impactCooldown = std::max(0.0f, m_impactCooldown - delta);
		if (pressed("showcase_nav", m_navHeld)) {
			bird::APPLICATION->setNavDebugDraw(!bird::APPLICATION->isNavDebugDrawEnabled());
		}
		if (pressed("showcase_sparks", m_sparksHeld)) {
			m_sparks->emitBurst(160);
			m_message = "Particles: 160-spark burst";
		}
		if (pressed("showcase_mute", m_muteHeld)) toggleMute();
		if (pressed("showcase_save", m_saveHeld)) save();
		if (pressed("showcase_load", m_loadHeld)) loadCamera();
		refreshHud();
	}

   private:
	/** True on the frame an action goes down (edge-triggered). */
	bool pressed(const char* action, bool& held) {
		const bool down = input()->getActionValue(action) > 0.5f;
		const bool edge = down && !held;
		held = down;
		return edge;
	}

	void onContact(const bird::EventPayload& payload) {
		m_contacts++;
		if (m_impactCooldown > 0.0f || !payload.has("point")) return;
		m_impactCooldown = kImpactCooldownSeconds;
		if (bird::AudioSystem* audio = bird::AudioSystem::getGlobal()) {
			audio->playOneShot3D(kImpactSound, payload.getVector3("point"),
								 bird::AudioBus::kSFX, 0.5f);
		}
	}

	void toggleMute() {
		bird::AudioSystem* audio = bird::AudioSystem::getGlobal();
		if (audio == nullptr) return;
		audio->setMasterMuted(!audio->isMasterMuted());
		m_message = audio->isMasterMuted() ? "Audio: muted" : "Audio: unmuted";
	}

	/** Writes the whole scene as JSON and the camera position as a save. */
	void save() {
		using bird::SaveGame;
		SaveGame::ensureDirectoryExists(SaveGame::getUserDataDirectory(kAppName));
		const std::string scenePath = SaveGame::getSaveFilePath(kAppName, kSceneSnapshotFile);
		const bool sceneSaved = bird::SceneSerializer::saveSceneToFile(scene(), scenePath);

		const bird::Vector3 eye = m_camera->getWorldPosition();
		bird::JsonValue position = bird::JsonValue::array();
		for (int axis = 0; axis < 3; axis++) position.pushBack(eye[axis]);
		bird::JsonValue save = bird::JsonValue::object();
		save.set("cameraPosition", std::move(position));
		const bool cameraSaved = SaveGame::writeTextFile(kAppName, kCameraSaveFile, save.dump());

		m_message = sceneSaved && cameraSaved ? "Saved scene + camera to " + scenePath
											  : "Save failed (see log)";
		BIRD_LOG_INFO("Showcase", m_message);
	}

	/** Restores the camera position written by save(). */
	void loadCamera() {
		const std::string text = bird::SaveGame::readTextFile(kAppName, kCameraSaveFile);
		const std::optional<bird::JsonValue> save = bird::JsonValue::parse(text);
		const bird::JsonValue* position = save ? save->find("cameraPosition") : nullptr;
		if (position == nullptr || !position->isArray() || position->asArray().size() != 3) {
			m_message = "Load: no saved camera yet (press F9 first)";
			return;
		}
		const auto& xyz = position->asArray();
		m_camera->setWorldPosition(bird::Vector3(xyz[0].asDouble(), xyz[1].asDouble(),
												 xyz[2].asDouble()));
		m_message = "Loaded camera position from save";
	}

	void refreshHud() {
		const bird::CullingStats& culling =
			bird::APPLICATION->getGraphicsPipeline()->getCullingStats();
		m_hud[0]->setText("Culling  drawn " + std::to_string(culling.rendered) + " / " +
						  std::to_string(culling.totalTested) + "   emitters " +
						  std::to_string(culling.emittersRendered) + "   particles " +
						  std::to_string(m_sparks->getActiveCount()));
		const bird::diag::ProfilerSummary profile = bird::diag::Profiler::get().summary(120);
		m_hud[1]->setText("Profiler  " + std::to_string(int(profile.averageFps)) +
						  " fps   p95 " + std::to_string(int(profile.p95FrameTimeMs)) +
						  " ms   agents " +
						  std::to_string(scene()->getNavWorld()->getAgentCount()));
		m_hud[2]->setText("Events  contacts " + std::to_string(m_contacts) +
						  "   footsteps " + std::to_string(m_footsteps));
		m_hud[3]->setText(m_message);
	}

	bird::Camera* m_camera;
	bird::ParticleEmitter* m_sparks;
	std::array<bird::UILabel*, kHudLines> m_hud;
	std::string m_message = "Edit assets/ in a Debug build to hot-reload them";
	uint64_t m_contacts = 0;
	uint64_t m_footsteps = 0;
	float m_impactCooldown = 0.0f;
	bool m_navHeld = false;
	bool m_sparksHeld = false;
	bool m_muteHeld = false;
	bool m_saveHeld = false;
	bool m_loadHeld = false;
};

/** Registers the showcase key bindings. */
void addActions() {
	bird::INPUT->addAction(bird::Action("showcase_nav").addKey(KEY_N));
	bird::INPUT->addAction(bird::Action("showcase_sparks").addKey(KEY_B));
	bird::INPUT->addAction(bird::Action("showcase_mute").addKey(KEY_M));
	bird::INPUT->addAction(bird::Action("showcase_save").addKey(KEY_F9));
	bird::INPUT->addAction(bird::Action("showcase_load").addKey(KEY_F10));
}

/** Adds the bottom-left systems overlay and its controller. */
void build(bird::Scene* scene, bird::Camera* camera, bird::ParticleEmitter* sparks,
		   bird::UIManager* ui, const HudInsets& insets) {
	using namespace bird;
	addActions();
	auto* column = ui->getRoot()->add<UIStack>(UIAxis::Vertical);
	column->setAnchor(UIAlignment::BottomLeft);
	column->setPivot(UIAlignment::BottomLeft);
	column->setOffset(Vector2(insets.left, -16.0f));
	column->setGap(4.0f);
	std::array<UILabel*, kHudLines> hud{};
	for (UILabel*& line : hud) {
		line = column->add<UILabel>("");
		line->setColor(Vector4(0.7f, 1.0f, 0.85f, 1.0f));
	}
	auto controller = _own_mk<WorldObject>();
	controller->setName("Showcase Controller");
	controller->addComponent(_own_mk<ShowcaseScript>(camera, sparks, hud));
	scene->addChild(std::move(controller));
}

}  // namespace showcase

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
		e->setName("Village Map");
		e->addComponent(bird::_own_mk<bird::MeshComponent>(mapMeshes));
		e->translate(mapPosition);
		object()->addChild(std::move(e));
		bird::RigidBodySettings level;
		level.motionType = bird::MotionType::Static;
		for (const bird::CollisionShape& chunk :
			 bird::CollisionShape::triangleMeshChunks(mapMeshes, 3, 3)) {
			auto collider = bird::_own_mk<bird::WorldObject>();
			collider->setName("Map Collider");
			collider->addComponent(bird::_own_mk<bird::RigidBodyComponent>(chunk, level));
			collider->setWorldPosition(mapPosition);
			object()->addChild(std::move(collider));
		}

		auto cam = bird::_own_mk<bird::Camera>(1280, 720);
		cam->setName("Main Camera");
		cam->addComponent(bird::_own_mk<POVCamera>());
		cam->addComponent(bird::_own_mk<bird::AudioListenerComponent>(true));
		cam->enable();
		bird::Camera* camera = cam.get();
		object()->addChild(std::move(cam));

		// BIRD_EDITOR=1 launches the level editor; its panels replace the
		// widget gallery, which would otherwise cover them.
		const bool editorRequested = std::getenv("BIRD_EDITOR") != nullptr;
		bird::UIManager* ui = bird::APPLICATION->getUIManager();
		if (!editorRequested) buildGallery(ui);
		// Editor side panels: hierarchy 260 px + 8 margin, inspector 300 + 8.
		const HudInsets insets =
			editorRequested ? HudInsets{284.0f, 324.0f} : HudInsets{};
		environment::build(camera);
		lighting::build(object());
		animation::build(object());
		playground::build(object(), camera, ui, insets);
		bird::ParticleEmitter* sparks = effects::build(object());
		patrol::build(object());
		showcase::build(object(), camera, sparks, ui, insets);
		// BIRD_PHYSICS_DEBUG=1 starts with collider wireframes on (G toggles).
		if (std::getenv("BIRD_PHYSICS_DEBUG") != nullptr) {
			bird::APPLICATION->setPhysicsDebugDraw(true);
		}
		if (editorRequested) startEditor(camera);
	}

	void deinit() override {
		if (m_editor) {
			bird::APPLICATION->setEditorTickCallback(nullptr);
			m_editor.reset();
		}
	}

   private:
	/**
	 * Opens the level editor in Edit mode (simulation paused). F1 or the
	 * toolbar's Play button runs the game; Ctrl+S saves the scene as JSON.
	 */
	void startEditor(bird::Camera* camera) {
		m_editor = std::make_unique<bird::editor::LevelEditor>(
			bird::APPLICATION->getUIManager(), object(), camera, bird::APPLICATION);
		bird::editor::SceneSerializationHooks hooks;
		hooks.defaultScenePath =
			bird::SaveGame::getSaveFilePath(showcase::kAppName, "editor_scene.json");
		hooks.saveScene = [](bird::Scene* root, const std::string& path) {
			bird::SaveGame::ensureDirectoryExists(
				bird::SaveGame::getUserDataDirectory(showcase::kAppName));
			return bird::SceneSerializer::saveSceneToFile(root, path);
		};
		m_editor->setSerializationHooks(std::move(hooks));
		bird::editor::LevelEditor* editor = m_editor.get();
		bird::APPLICATION->setEditorTickCallback(
			[editor](float delta) { editor->update(delta); });
	}

	std::unique_ptr<bird::editor::LevelEditor> m_editor;
};

extern "C" BIRD_API_EXPORT bird::Script<bird::Scene>* app_entry() {
	return new MainScript;
}
