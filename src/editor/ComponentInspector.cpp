#include "ComponentInspector.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include "../physics/RigidBodyComponent.h"
#include "../renders/generic/Material.h"
#include "../scene/components/LightComponent.h"
#include "../scene/components/MeshComponent.h"

namespace bird::editor {

namespace {
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

std::string formatFloatShort(float v) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << v;
	return oss.str();
}
}  // namespace

// ------------------------------------------------------------ InspectorBuilder

InspectorBuilder::InspectorBuilder(UIStack* container, UndoStack* undoStack,
								   std::function<void()> onMutated)
	: m_container(container),
	  m_undoStack(undoStack),
	  m_onMutated(std::move(onMutated)) {}

UIStack* InspectorBuilder::makeRow(const std::string& label) {
	auto* row = m_container->add<UIStack>(UIAxis::Horizontal);
	row->setWidth(UISizeRule::fill());
	row->setHeight(UISizeRule::fit());
	row->setGap(6.0f);
	row->setAlign(UIAlign::Center);

	auto* lbl = row->add<UILabel>(label);
	lbl->setWidth(UISizeRule::fixed(96.0f));
	return row;
}

UILabel* InspectorBuilder::addReadOnly(const std::string& label,
									   const std::string& value) {
	UIStack* row = makeRow(label);
	auto* val = row->add<UILabel>(value);
	val->setWidth(UISizeRule::fill());
	val->setColor(val->getTheme().mutedTextColor);
	return val;
}

UINumberField* InspectorBuilder::addFloat(const std::string& label,
										  std::function<float()> getter,
										  std::function<void(float)> setter,
										  double min, double max, double step,
										  int precision) {
	UIStack* row = makeRow(label);
	const double initial = static_cast<double>(getter());
	auto* field = row->add<UINumberField>(min, max, initial);
	field->setStep(step);
	field->setPrecision(precision);
	field->setWidth(UISizeRule::fill());

	UndoStack* undo = m_undoStack;
	auto onMutated = m_onMutated;
	field->setOnValueChanged([label, getter, setter, undo,
							  onMutated](double newVal) {
		const float prev = getter();
		const float next = static_cast<float>(newVal);
		if (std::abs(next - prev) < 1e-6f) return;
		setter(next);
		if (undo) {
			undo->push(std::make_unique<LambdaCommand>(
				"Edit " + label,
				[setter, next, onMutated] {
					setter(next);
					if (onMutated) onMutated();
				},
				[setter, prev, onMutated] {
					setter(prev);
					if (onMutated) onMutated();
				}));
		}
	});
	return field;
}

UINumberField* InspectorBuilder::addInt(const std::string& label,
										std::function<int()> getter,
										std::function<void(int)> setter,
										int min, int max, int step) {
	UIStack* row = makeRow(label);
	auto* field = row->add<UINumberField>(static_cast<double>(min),
										  static_cast<double>(max),
										  static_cast<double>(getter()));
	field->setStep(static_cast<double>(step));
	field->setPrecision(0);
	field->setWidth(UISizeRule::fill());

	UndoStack* undo = m_undoStack;
	auto onMutated = m_onMutated;
	field->setOnValueChanged([label, getter, setter, undo,
							  onMutated](double newVal) {
		const int prev = getter();
		const int next = static_cast<int>(std::lround(newVal));
		if (next == prev) return;
		setter(next);
		if (undo) {
			undo->push(std::make_unique<LambdaCommand>(
				"Edit " + label,
				[setter, next, onMutated] {
					setter(next);
					if (onMutated) onMutated();
				},
				[setter, prev, onMutated] {
					setter(prev);
					if (onMutated) onMutated();
				}));
		}
	});
	return field;
}

UICheckBox* InspectorBuilder::addBool(const std::string& label,
									  std::function<bool()> getter,
									  std::function<void(bool)> setter) {
	UIStack* row = makeRow(label);
	auto* cb = row->add<UICheckBox>("", getter());

	UndoStack* undo = m_undoStack;
	auto onMutated = m_onMutated;
	cb->setOnChanged([label, getter, setter, undo, onMutated](bool newVal) {
		const bool prev = getter();
		if (newVal == prev) return;
		setter(newVal);
		if (undo) {
			undo->push(std::make_unique<LambdaCommand>(
				"Toggle " + label,
				[setter, newVal, onMutated] {
					setter(newVal);
					if (onMutated) onMutated();
				},
				[setter, prev, onMutated] {
					setter(prev);
					if (onMutated) onMutated();
				}));
		}
	});
	return cb;
}

UITextBox* InspectorBuilder::addString(
	const std::string& label, std::function<std::string()> getter,
	std::function<void(const std::string&)> setter) {
	UIStack* row = makeRow(label);
	auto* box = row->add<UITextBox>(getter());
	box->setWidth(UISizeRule::fill());

	UndoStack* undo = m_undoStack;
	auto onMutated = m_onMutated;
	box->setOnSubmit(
		[label, getter, setter, undo, onMutated](const std::string& newVal) {
			const std::string prev = getter();
			if (newVal == prev) return;
			setter(newVal);
			if (undo) {
				undo->push(std::make_unique<LambdaCommand>(
					"Edit " + label,
					[setter, newVal, onMutated] {
						setter(newVal);
						if (onMutated) onMutated();
					},
					[setter, prev, onMutated] {
						setter(prev);
						if (onMutated) onMutated();
					}));
			}
		});
	return box;
}

std::array<UINumberField*, 3> InspectorBuilder::addVec3(
	const std::string& label, std::function<Vector3()> getter,
	std::function<void(const Vector3&)> setter, double min, double max,
	double step, int precision) {
	auto* block = m_container->add<UIStack>(UIAxis::Vertical);
	block->setWidth(UISizeRule::fill());
	block->setHeight(UISizeRule::fit());
	block->setGap(2.0f);

	auto* lbl = block->add<UILabel>(label);
	lbl->setColor(lbl->getTheme().mutedTextColor);

	auto* row = block->add<UIStack>(UIAxis::Horizontal);
	row->setWidth(UISizeRule::fill());
	row->setHeight(UISizeRule::fit());
	row->setGap(4.0f);

	const Vector3 initial = getter();
	std::array<UINumberField*, 3> fields{};
	UndoStack* undo = m_undoStack;
	auto onMutated = m_onMutated;

	for (int axis = 0; axis < 3; ++axis) {
		auto* f = row->add<UINumberField>(min, max,
										  static_cast<double>(initial[axis]));
		f->setStep(step);
		f->setPrecision(precision);
		f->setWidth(UISizeRule::fill(1.0f));
		f->setOnValueChanged(
			[label, axis, getter, setter, undo, onMutated](double newVal) {
				const Vector3 prev = getter();
				Vector3 next = prev;
				next[axis] = static_cast<float>(newVal);
				if (std::abs(next[axis] - prev[axis]) < 1e-6f) return;
				setter(next);
				if (undo) {
					undo->push(std::make_unique<LambdaCommand>(
						"Edit " + label,
						[setter, next, onMutated] {
							setter(next);
							if (onMutated) onMutated();
						},
						[setter, prev, onMutated] {
							setter(prev);
							if (onMutated) onMutated();
						}));
				}
			});
		fields[axis] = f;
	}
	return fields;
}

UIColorButton* InspectorBuilder::addColorRGB(
	const std::string& label, std::function<Vector3()> getter,
	std::function<void(const Vector3&)> setter) {
	UIStack* row = makeRow(label);
	const Vector3 init = getter();
	auto* btn = row->add<UIColorButton>(Vector4(init, 1.0f));
	btn->setWidth(UISizeRule::fill());

	UndoStack* undo = m_undoStack;
	auto onMutated = m_onMutated;
	btn->setOnColorChanged(
		[label, getter, setter, undo, onMutated](const Vector4& rgba) {
			const Vector3 prev = getter();
			const Vector3 next(rgba.r, rgba.g, rgba.b);
			if (glm::length(next - prev) < 1e-5f) return;
			setter(next);
			if (undo) {
				undo->push(std::make_unique<LambdaCommand>(
					"Edit " + label,
					[setter, next, onMutated] {
						setter(next);
						if (onMutated) onMutated();
					},
					[setter, prev, onMutated] {
						setter(prev);
						if (onMutated) onMutated();
					}));
			}
		});
	return btn;
}

UIColorButton* InspectorBuilder::addColorRGBA(
	const std::string& label, std::function<Vector4()> getter,
	std::function<void(const Vector4&)> setter) {
	UIStack* row = makeRow(label);
	auto* btn = row->add<UIColorButton>(getter());
	btn->setWidth(UISizeRule::fill());

	UndoStack* undo = m_undoStack;
	auto onMutated = m_onMutated;
	btn->setOnColorChanged(
		[label, getter, setter, undo, onMutated](const Vector4& next) {
			const Vector4 prev = getter();
			if (glm::length(next - prev) < 1e-5f) return;
			setter(next);
			if (undo) {
				undo->push(std::make_unique<LambdaCommand>(
					"Edit " + label,
					[setter, next, onMutated] {
						setter(next);
						if (onMutated) onMutated();
					},
					[setter, prev, onMutated] {
						setter(prev);
						if (onMutated) onMutated();
					}));
			}
		});
	return btn;
}

UIDropdown* InspectorBuilder::addDropdown(
	const std::string& label, std::vector<std::string> items,
	std::function<size_t()> getter, std::function<void(size_t)> setter) {
	UIStack* row = makeRow(label);
	auto* dd = row->add<UIDropdown>(std::move(items));
	dd->setWidth(UISizeRule::fill());
	const size_t current = getter();
	if (current < dd->getItems().size()) {
		dd->setSelectedIndex(current);
	}

	UndoStack* undo = m_undoStack;
	auto onMutated = m_onMutated;
	dd->setOnChanged([label, getter, setter, undo, onMutated](size_t next) {
		const size_t prev = getter();
		if (next == prev) return;
		setter(next);
		if (undo) {
			undo->push(std::make_unique<LambdaCommand>(
				"Edit " + label,
				[setter, next, onMutated] {
					setter(next);
					if (onMutated) onMutated();
				},
				[setter, prev, onMutated] {
					setter(prev);
					if (onMutated) onMutated();
				}));
		}
	});
	return dd;
}

// ------------------------------------------------ ComponentInspectorRegistry

ComponentInspectorRegistry& ComponentInspectorRegistry::instance() {
	static ComponentInspectorRegistry registry;
	return registry;
}

ComponentInspectorRegistry::ComponentInspectorRegistry() {
	registerBuiltins();
}

bool ComponentInspectorRegistry::hasInspector(std::type_index type) const {
	return m_entries.find(type) != m_entries.end();
}

std::string ComponentInspectorRegistry::displayNameFor(
	const Component& component) const {
	const auto it = m_entries.find(std::type_index(typeid(component)));
	if (it != m_entries.end()) {
		return it->second.displayName;
	}
	return std::string(typeid(component).name());
}

bool ComponentInspectorRegistry::buildInspector(
	Component* component, InspectorBuilder& builder) const {
	if (!component) return false;
	const auto it = m_entries.find(std::type_index(typeid(*component)));
	if (it == m_entries.end()) return false;
	it->second.inspect(component, builder);
	return true;
}

void ComponentInspectorRegistry::registerBuiltins() {
	// LightComponent
	registerInspector<LightComponent>(
		"Light", [](LightComponent* light, InspectorBuilder& ui) {
			const char* typeStr = "Directional";
			if (light->getType() == LightType::Point) typeStr = "Point";
			else if (light->getType() == LightType::Spot) typeStr = "Spot";
			ui.addReadOnly("Type", typeStr);

			ui.addColorRGB("Color", [light] { return light->getColor(); },
						   [light](const Vector3& c) { light->setColor(c); });
			ui.addFloat("Intensity", [light] { return light->getIntensity(); },
						[light](float v) { light->setIntensity(v); }, 0.0,
						1000.0, 0.25, 2);

			if (light->getType() != LightType::Directional) {
				ui.addFloat("Range", [light] { return light->getRange(); },
							[light](float v) { light->setRange(v); }, 0.1,
							500.0, 0.5, 2);
			}
			if (light->getType() == LightType::Spot) {
				ui.addFloat(
					"Inner Cone",
					[light] { return light->getInnerConeAngle() * kRadToDeg; },
					[light](float deg) {
						light->setConeAngles(deg * kDegToRad,
											 light->getOuterConeAngle());
					},
					1.0, 89.0, 1.0, 1);
				ui.addFloat(
					"Outer Cone",
					[light] { return light->getOuterConeAngle() * kRadToDeg; },
					[light](float deg) {
						light->setConeAngles(light->getInnerConeAngle(),
											 deg * kDegToRad);
					},
					1.0, 89.0, 1.0, 1);
			}
			ui.addBool("Shadows", [light] { return light->getCastsShadows(); },
					   [light](bool b) { light->setCastsShadows(b); });
			ui.addFloat(
				"Depth Bias", [light] { return light->getShadowDepthBias(); },
				[light](float v) { light->setShadowDepthBias(v); }, 0.0, 20.0,
				0.1, 2);
			ui.addFloat(
				"Normal Bias", [light] { return light->getShadowNormalBias(); },
				[light](float v) { light->setShadowNormalBias(v); }, 0.0, 20.0,
				0.1, 2);
		});

	// MeshComponent
	registerInspector<MeshComponent>(
		"Mesh", [](MeshComponent* meshComp, InspectorBuilder& ui) {
			const int count = meshComp->getMeshCount();
			ui.addReadOnly("Submeshes", std::to_string(count));
			if (count > 0) {
				_ref<Material> mat = meshComp->getMaterial(0);
				if (mat) {
					ui.addColorRGBA(
						"Base Color", [mat] { return mat->getBaseColor(); },
						[mat](const Vector4& c) { mat->setBaseColor(c); });
					ui.addFloat("Metallic",
								[mat] { return mat->getMetallic(); },
								[mat](float v) { mat->setMetallic(v); }, 0.0,
								1.0, 0.05, 2);
					ui.addFloat("Roughness",
								[mat] { return mat->getRoughness(); },
								[mat](float v) { mat->setRoughness(v); }, 0.02,
								1.0, 0.05, 2);
					ui.addColorRGB(
						"Emissive", [mat] { return mat->getEmissive(); },
						[mat](const Vector3& c) { mat->setEmissive(c); });
					ui.addFloat(
						"Emissive Str",
						[mat] { return mat->getEmissiveStrength(); },
						[mat](float v) { mat->setEmissiveStrength(v); }, 0.0,
						100.0, 0.1, 2);
					ui.addBool("Double Sided",
							   [mat] { return mat->isDoubleSided(); },
							   [mat](bool b) { mat->setDoubleSided(b); });
					ui.addBool("Shadows", [mat] { return mat->castsShadows(); },
							   [mat](bool b) { mat->setCastsShadows(b); });
				}
			}
		});

	// RigidBodyComponent
	registerInspector<RigidBodyComponent>(
		"Rigid Body", [](RigidBodyComponent* rb, InspectorBuilder& ui) {
			const char* motionStr = "Dynamic";
			if (rb->getMotionType() == MotionType::Static) motionStr = "Static";
			else if (rb->getMotionType() == MotionType::Kinematic)
				motionStr = "Kinematic";
			ui.addReadOnly("Motion", motionStr);
			ui.addReadOnly("Sensor",
						   rb->getSettings().isSensor ? "Yes" : "No");
			if (rb->isInWorld()) {
				ui.addReadOnly("Mass (kg)", formatFloatShort(rb->getMass()));
			}
			ui.addFloat("Friction",
						[rb] { return rb->getSettings().friction; },
						[rb](float v) { rb->setFriction(v); }, 0.0, 10.0, 0.05,
						2);
			ui.addFloat("Restitution",
						[rb] { return rb->getSettings().restitution; },
						[rb](float v) { rb->setRestitution(v); }, 0.0, 1.0,
						0.05, 2);
			ui.addFloat("Gravity",
						[rb] { return rb->getSettings().gravityFactor; },
						[rb](float v) { rb->setGravityFactor(v); }, -10.0, 10.0,
						0.1, 2);
		});
}

}  // namespace bird::editor
