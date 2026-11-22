#include "InspectorPanel.h"

#include <cmath>
#include <memory>
#include <string>

#include "../scene/2D/UIButton.h"
#include "ComponentInspector.h"
#include "HierarchyPanel.h"

namespace bird::editor {

namespace {
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

Vector3 quatToEulerDegrees(const Quaternion& q) {
	return glm::eulerAngles(glm::normalize(q)) * kRadToDeg;
}

Quaternion eulerDegreesToQuat(const Vector3& deg) {
	return Quaternion(deg * kDegToRad);
}
}  // namespace

InspectorPanel::InspectorPanel(UIObject* parentRoot, UndoStack* undoStack)
	: m_undoStack(undoStack) {
	m_panel = parentRoot->add<UIPanel>();
	m_panel->setAnchors(UIAlignment::TopRight, UIAlignment::BottomRight);
	m_panel->setPivot(UIAlignment::TopRight);
	m_panel->setMargin(UIInsets{8.0f, 38.0f, 8.0f, 8.0f});
	m_panel->setWidth(UISizeRule::fixed(300.0f));
	m_panel->setPadding(UIInsets::all(8.0f));
	m_panel->setClipChildren(true);

	auto* outer = m_panel->add<UIStack>(UIAxis::Vertical);
	outer->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	outer->setGap(6.0f);
	outer->setAlign(UIAlign::Stretch);

	auto* title = outer->add<UILabel>("Inspector");
	title->setFontSize(15.0f);

	m_scroll = outer->add<UIScrollView>();
	m_scroll->setWidth(UISizeRule::fill());
	m_scroll->setHeight(UISizeRule::fill(1.0f));

	rebuild();
}

void InspectorPanel::setTarget(WorldObject* target) {
	if (m_target == target) {
		syncTransformFields();
		return;
	}
	m_target = target;
	rebuild();
}

void InspectorPanel::rebuild() {
	m_nameBox = nullptr;
	m_posFields = {nullptr, nullptr, nullptr};
	m_rotFields = {nullptr, nullptr, nullptr};
	m_scaleFields = {nullptr, nullptr, nullptr};

	m_content = m_scroll->setContent<UIStack>(UIAxis::Vertical);
	m_content->setWidth(UISizeRule::fill());
	m_content->setHeight(UISizeRule::fit());
	m_content->setGap(8.0f);
	m_content->setAlign(UIAlign::Stretch);

	if (!m_target) {
		buildEmptyState();
	} else {
		buildEntityView();
	}
}

void InspectorPanel::buildEmptyState() {
	auto* hint = m_content->add<UILabel>("No entity selected.");
	hint->setColor(hint->getTheme().mutedTextColor);
}

void InspectorPanel::buildEntityView() {
	WorldObject* target = m_target;

	// --- Entity Header (ID + Editable Name) ---
	auto* headerStack = m_content->add<UIStack>(UIAxis::Vertical);
	headerStack->setWidth(UISizeRule::fill());
	headerStack->setHeight(UISizeRule::fit());
	headerStack->setGap(4.0f);

	auto* idLabel = headerStack->add<UILabel>(
		"Entity ID #" + std::to_string(target->getID()));
	idLabel->setColor(idLabel->getTheme().mutedTextColor);

	auto* nameRow = headerStack->add<UIStack>(UIAxis::Horizontal);
	nameRow->setWidth(UISizeRule::fill());
	nameRow->setHeight(UISizeRule::fit());
	nameRow->setGap(6.0f);
	nameRow->setAlign(UIAlign::Center);

	auto* nameLbl = nameRow->add<UILabel>("Name");
	nameLbl->setWidth(UISizeRule::fixed(50.0f));

	m_nameBox = nameRow->add<UITextBox>(target->getName());
	m_nameBox->setPlaceholder(HierarchyPanel::formatEntityLabel(*target));
	m_nameBox->setWidth(UISizeRule::fill());
	m_nameBox->setOnSubmit([this, target](const std::string& newName) {
		if (m_syncing || !m_target || m_target != target) return;
		const std::string oldName = target->getName();
		if (newName == oldName) return;
		target->setName(newName);
		if (m_onEntityRenamed) m_onEntityRenamed(target);
		if (m_undoStack) {
			m_undoStack->push(std::make_unique<LambdaCommand>(
				"Rename Entity",
				[this, target, newName] {
					target->setName(newName);
					if (m_nameBox && m_target == target) {
						m_nameBox->setText(newName);
					}
					if (m_onEntityRenamed) m_onEntityRenamed(target);
				},
				[this, target, oldName] {
					target->setName(oldName);
					if (m_nameBox && m_target == target) {
						m_nameBox->setText(oldName);
					}
					if (m_onEntityRenamed) m_onEntityRenamed(target);
				}));
		}
	});

	// --- Transform Section ---
	auto* transformExp = m_content->add<UIExpander>("Transform", true);
	transformExp->setWidth(UISizeRule::fill());
	auto* tfStack = transformExp->getContent()->add<UIStack>(UIAxis::Vertical);
	tfStack->setWidth(UISizeRule::fill());
	tfStack->setHeight(UISizeRule::fit());
	tfStack->setGap(6.0f);

	auto makeVec3Row = [&](const char* title, const Vector3& val, double min,
						   double max, double step,
						   std::array<UINumberField*, 3>& outFields,
						   std::function<void(int, double)> onAxisEdit) {
		auto* group = tfStack->add<UIStack>(UIAxis::Vertical);
		group->setWidth(UISizeRule::fill());
		group->setHeight(UISizeRule::fit());
		group->setGap(2.0f);

		auto* lbl = group->add<UILabel>(title);
		lbl->setColor(lbl->getTheme().mutedTextColor);

		auto* row = group->add<UIStack>(UIAxis::Horizontal);
		row->setWidth(UISizeRule::fill());
		row->setHeight(UISizeRule::fit());
		row->setGap(4.0f);

		for (int i = 0; i < 3; ++i) {
			auto* field = row->add<UINumberField>(min, max,
												  static_cast<double>(val[i]));
			field->setStep(step);
			field->setPrecision(2);
			field->setWidth(UISizeRule::fill(1.0f));
			field->setOnValueChanged(
				[this, i, onAxisEdit](double v) {
					if (m_syncing || !m_target) return;
					onAxisEdit(i, v);
				});
			outFields[i] = field;
		}
	};

	const Vector3 pos = target->getWorldPosition();
	const Vector3 rotDeg = quatToEulerDegrees(target->getWorldRotation());
	const Vector3 scl = target->getScale();

	makeVec3Row("Position (X, Y, Z)", pos, -100000.0, 100000.0, 0.25,
				m_posFields, [this](int axis, double v) {
					const TransformSnapshot before =
						TransformSnapshot::capture(*m_target);
					Vector3 next = m_target->getWorldPosition();
					next[axis] = static_cast<float>(v);
					m_target->setWorldPosition(next);
					m_syncedRevision = m_target->getTransformRevision();
					const TransformSnapshot after =
						TransformSnapshot::capture(*m_target);
					if (m_undoStack && before != after) {
						m_undoStack->push(std::make_unique<TransformCommand>(
							m_target, before, after, "Edit Position"));
					}
				});

	makeVec3Row("Rotation (Deg X, Y, Z)", rotDeg, -360.0, 360.0, 5.0,
				m_rotFields, [this](int, double) {
					const TransformSnapshot before =
						TransformSnapshot::capture(*m_target);
					const Vector3 deg(
						static_cast<float>(m_rotFields[0]->getValue()),
						static_cast<float>(m_rotFields[1]->getValue()),
						static_cast<float>(m_rotFields[2]->getValue()));
					m_target->setWorldRotation(eulerDegreesToQuat(deg));
					m_syncedRevision = m_target->getTransformRevision();
					const TransformSnapshot after =
						TransformSnapshot::capture(*m_target);
					if (m_undoStack && before != after) {
						m_undoStack->push(std::make_unique<TransformCommand>(
							m_target, before, after, "Edit Rotation"));
					}
				});

	makeVec3Row("Scale (X, Y, Z)", scl, 0.01, 10000.0, 0.1, m_scaleFields,
				[this](int axis, double v) {
					const TransformSnapshot before =
						TransformSnapshot::capture(*m_target);
					Vector3 next = m_target->getScale();
					next[axis] = static_cast<float>(v);
					m_target->setScale(next);
					m_syncedRevision = m_target->getTransformRevision();
					const TransformSnapshot after =
						TransformSnapshot::capture(*m_target);
					if (m_undoStack && before != after) {
						m_undoStack->push(std::make_unique<TransformCommand>(
							m_target, before, after, "Edit Scale"));
					}
				});

	auto* resetBtn = tfStack->add<UIButton>("Reset Transform");
	resetBtn->setWidth(UISizeRule::fill());
	resetBtn->setOnClick([this] {
		if (!m_target) return;
		const TransformSnapshot before = TransformSnapshot::capture(*m_target);
		TransformSnapshot clean;
		clean.apply(*m_target);
		syncTransformFields();
		if (m_undoStack && before != clean) {
			m_undoStack->push(std::make_unique<TransformCommand>(
				m_target, before, clean, "Reset Transform"));
		}
	});

	m_syncedRevision = target->getTransformRevision();

	// --- Attached Components ---
	auto& registry = ComponentInspectorRegistry::instance();
	for (auto& compPtr : target->getComponents()) {
		Component* comp = compPtr.get();
		if (!comp) continue;

		const std::string title = registry.displayNameFor(*comp);
		auto* exp = m_content->add<UIExpander>(title, true);
		exp->setWidth(UISizeRule::fill());

		auto* compStack = exp->getContent()->add<UIStack>(UIAxis::Vertical);
		compStack->setWidth(UISizeRule::fill());
		compStack->setHeight(UISizeRule::fit());
		compStack->setGap(6.0f);

		InspectorBuilder builder(compStack, m_undoStack);
		if (!registry.buildInspector(comp, builder)) {
			builder.addReadOnly("Status", "Active (no editable properties)");
		}
	}
}

void InspectorPanel::syncTransformFields() {
	if (!m_target || !m_posFields[0]) return;
	if (m_target->getTransformRevision() == m_syncedRevision) return;

	m_syncing = true;
	const Vector3 pos = m_target->getWorldPosition();
	const Vector3 rotDeg = quatToEulerDegrees(m_target->getWorldRotation());
	const Vector3 scl = m_target->getScale();

	for (int i = 0; i < 3; ++i) {
		if (m_posFields[i]) m_posFields[i]->setValue(pos[i]);
		if (m_rotFields[i]) m_rotFields[i]->setValue(rotDeg[i]);
		if (m_scaleFields[i]) m_scaleFields[i]->setValue(scl[i]);
	}
	if (m_nameBox && m_nameBox->getText() != m_target->getName()) {
		m_nameBox->setText(m_target->getName());
	}
	m_syncedRevision = m_target->getTransformRevision();
	m_syncing = false;
}

void InspectorPanel::setOnEntityRenamed(
	std::function<void(WorldObject*)> callback) {
	m_onEntityRenamed = std::move(callback);
}

}  // namespace bird::editor
