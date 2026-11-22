#include "UndoStack.h"

#include <algorithm>
#include <cmath>

namespace bird::editor {

namespace {

bool vec3NearlyEqual(const Vector3& a, const Vector3& b, float eps = 1e-5f) {
	return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps &&
		   std::abs(a.z - b.z) <= eps;
}

bool quatNearlyEqual(const Quaternion& a, const Quaternion& b,
					 float eps = 1e-5f) {
	// Quaternions q and -q represent the same orientation.
	const float d = std::abs(glm::dot(a, b));
	return (1.0f - d) <= eps;
}

}  // namespace

// ------------------------------------------------------------ TransformSnapshot

TransformSnapshot TransformSnapshot::capture(const WorldObject& obj) {
	TransformSnapshot snap;
	snap.worldPosition = obj.getWorldPosition();
	snap.localPosition = obj.getLocalPosition();
	snap.worldRotation = obj.getWorldRotation();
	snap.localRotation = obj.getLocalRotation();
	snap.scale = obj.getScale();
	return snap;
}

void TransformSnapshot::apply(WorldObject& obj) const {
	obj.setWorldPosition(worldPosition);
	obj.setLocalPosition(localPosition);
	obj.setWorldRotation(worldRotation);
	obj.setLocalRotation(localRotation);
	obj.setScale(scale);
}

bool TransformSnapshot::operator==(const TransformSnapshot& other) const {
	return vec3NearlyEqual(worldPosition, other.worldPosition) &&
		   vec3NearlyEqual(localPosition, other.localPosition) &&
		   quatNearlyEqual(worldRotation, other.worldRotation) &&
		   quatNearlyEqual(localRotation, other.localRotation) &&
		   vec3NearlyEqual(scale, other.scale);
}

// ------------------------------------------------------------ TransformCommand

TransformCommand::TransformCommand(WorldObject* target,
								   const TransformSnapshot& before,
								   const TransformSnapshot& after,
								   std::string description)
	: m_target(target),
	  m_before(before),
	  m_after(after),
	  m_description(std::move(description)) {}

void TransformCommand::execute() {
	if (m_target) {
		m_after.apply(*m_target);
	}
}

void TransformCommand::undo() {
	if (m_target) {
		m_before.apply(*m_target);
	}
}

// ------------------------------------------------------------ LambdaCommand

LambdaCommand::LambdaCommand(std::string description,
							 std::function<void()> doFn,
							 std::function<void()> undoFn)
	: m_description(std::move(description)),
	  m_doFn(std::move(doFn)),
	  m_undoFn(std::move(undoFn)) {}

void LambdaCommand::execute() {
	if (m_doFn) m_doFn();
}

void LambdaCommand::undo() {
	if (m_undoFn) m_undoFn();
}

// ------------------------------------------------------------ CreateEntityCommand

CreateEntityCommand::CreateEntityCommand(
	Scene* parent, _own<WorldObject> entity,
	std::function<void(WorldObject*)> onChanged)
	: m_parent(parent),
	  m_owned(std::move(entity)),
	  m_entityPtr(m_owned.get()),
	  m_onChanged(std::move(onChanged)) {
	const std::string name =
		(m_entityPtr && !m_entityPtr->getName().empty())
			? m_entityPtr->getName()
			: "Entity";
	m_description = "Create " + name;
}

void CreateEntityCommand::execute() {
	if (!m_parent || !m_owned) return;
	m_parent->addChild(std::move(m_owned));
	if (m_onChanged) m_onChanged(m_entityPtr);
}

void CreateEntityCommand::undo() {
	if (!m_parent || !m_entityPtr || m_owned) return;
	m_owned = m_parent->detachChild(m_entityPtr);
	if (m_onChanged) m_onChanged(nullptr);
}

// ------------------------------------------------------------ DeleteEntityCommand

DeleteEntityCommand::DeleteEntityCommand(
	Scene* parent, WorldObject* entity,
	std::function<void(WorldObject*)> onChanged)
	: m_parent(parent),
	  m_entityPtr(entity),
	  m_onChanged(std::move(onChanged)) {
	const std::string name =
		(m_entityPtr && !m_entityPtr->getName().empty())
			? m_entityPtr->getName()
			: "Entity";
	m_description = "Delete " + name;
}

void DeleteEntityCommand::execute() {
	if (!m_parent || !m_entityPtr || m_detached) return;
	m_detached = m_parent->detachChild(m_entityPtr);
	if (m_onChanged) m_onChanged(nullptr);
}

void DeleteEntityCommand::undo() {
	if (!m_parent || !m_detached) return;
	m_parent->addChild(std::move(m_detached));
	if (m_onChanged) m_onChanged(m_entityPtr);
}

// ------------------------------------------------------------ UndoStack

UndoStack::UndoStack(size_t maxHistory)
	: m_maxHistory(std::max<size_t>(1, maxHistory)) {}

void UndoStack::push(std::unique_ptr<EditorCommand> command, bool executeNow) {
	if (!command) return;
	if (executeNow) {
		command->execute();
	}
	// Discard redo tail when branching from an earlier point in history.
	if (m_cursor < m_commands.size()) {
		m_commands.erase(
			m_commands.begin() + static_cast<std::ptrdiff_t>(m_cursor),
			m_commands.end());
	}
	// Attempt to coalesce with the top command when supported.
	if (!m_commands.empty() && m_commands.back()->mergeWith(*command)) {
		notify();
		return;
	}
	m_commands.push_back(std::move(command));
	m_cursor = m_commands.size();

	if (m_commands.size() > m_maxHistory) {
		const size_t excess = m_commands.size() - m_maxHistory;
		m_commands.erase(
			m_commands.begin(),
			m_commands.begin() + static_cast<std::ptrdiff_t>(excess));
		m_cursor = m_commands.size();
	}
	notify();
}

bool UndoStack::undo() {
	if (!canUndo()) return false;
	--m_cursor;
	m_commands[m_cursor]->undo();
	notify();
	return true;
}

bool UndoStack::redo() {
	if (!canRedo()) return false;
	m_commands[m_cursor]->execute();
	++m_cursor;
	notify();
	return true;
}

std::string UndoStack::undoDescription() const {
	if (!canUndo()) return {};
	return m_commands[m_cursor - 1]->description();
}

std::string UndoStack::redoDescription() const {
	if (!canRedo()) return {};
	return m_commands[m_cursor]->description();
}

void UndoStack::clear() {
	if (m_commands.empty() && m_cursor == 0) return;
	m_commands.clear();
	m_cursor = 0;
	notify();
}

void UndoStack::setOnChanged(std::function<void()> callback) {
	m_onChanged = std::move(callback);
}

void UndoStack::notify() {
	if (m_onChanged) m_onChanged();
}

}  // namespace bird::editor
