#ifndef BIRD_EDITOR_UNDO_STACK_H
#define BIRD_EDITOR_UNDO_STACK_H

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../scene/Scene.h"
#include "../scene/WorldObject.h"
#include "../util/Math.h"

namespace bird::editor {

/**
 * Value snapshot of a WorldObject's complete spatial transform.
 *
 * Used by TransformCommand for Undo/Redo and by LevelEditor to restore scene
 * state when stopping Play Mode.
 */
struct TransformSnapshot {
	Vector3 worldPosition{0.0f};
	Vector3 localPosition{0.0f};
	Quaternion worldRotation{1.0f, 0.0f, 0.0f, 0.0f};
	Quaternion localRotation{1.0f, 0.0f, 0.0f, 0.0f};
	Vector3 scale{1.0f};

	/**
	 * Captures the current transform fields of an entity.
	 * @param obj Entity to snapshot.
	 * @return Snapshot containing world/local position, rotation, and scale.
	 */
	static TransformSnapshot capture(const WorldObject& obj);

	/**
	 * Applies this snapshot back onto an entity, marking its matrix dirty.
	 * @param obj Target entity to mutate.
	 */
	void apply(WorldObject& obj) const;

	bool operator==(const TransformSnapshot& other) const;
	bool operator!=(const TransformSnapshot& other) const {
		return !(*this == other);
	}
};

/**
 * Abstract base interface for reversible editor actions managed by UndoStack.
 *
 * Concrete subclasses encapsulate both the forward mutation (execute) and its
 * exact inverse (undo), along with a human-readable description shown in UI
 * menus and status bars.
 */
class EditorCommand {
   public:
	virtual ~EditorCommand() = default;

	/** Applies (or re-applies) the command's mutation. */
	virtual void execute() = 0;

	/** Reverts the mutation performed by execute(). */
	virtual void undo() = 0;

	/** Returns a short human-readable label (e.g. "Translate Entity"). */
	virtual std::string description() const = 0;

	/**
	 * Optional hook to coalesce consecutive edits into a single undo step.
	 * @param other Newly pushed command of potentially matching kind.
	 * @return True if @p other was absorbed into this command.
	 */
	virtual bool mergeWith(const EditorCommand& other) {
		(void)other;
		return false;
	}
};

/**
 * Reversible command that applies a TransformSnapshot to a WorldObject.
 */
class TransformCommand : public EditorCommand {
   public:
	/**
	 * Constructs a transform edit command.
	 * @param target Entity whose transform was edited (must outlive command).
	 * @param before Transform state prior to the edit.
	 * @param after Transform state after the edit.
	 * @param description Human-readable label for the undo menu.
	 */
	TransformCommand(WorldObject* target, const TransformSnapshot& before,
					 const TransformSnapshot& after,
					 std::string description = "Transform Entity");

	void execute() override;
	void undo() override;
	std::string description() const override { return m_description; }

   private:
	WorldObject* m_target;
	TransformSnapshot m_before;
	TransformSnapshot m_after;
	std::string m_description;
};

/**
 * Generic closure-based command for Inspector property edits and renames.
 */
class LambdaCommand : public EditorCommand {
   public:
	/**
	 * Constructs a command from execute and undo callbacks.
	 * @param description Human-readable label for the undo menu.
	 * @param doFn Callback executed on apply/redo.
	 * @param undoFn Callback executed on undo.
	 */
	LambdaCommand(std::string description, std::function<void()> doFn,
				  std::function<void()> undoFn);

	void execute() override;
	void undo() override;
	std::string description() const override { return m_description; }

   private:
	std::string m_description;
	std::function<void()> m_doFn;
	std::function<void()> m_undoFn;
};

/**
 * Reversible command that adds a new WorldObject to a parent Scene.
 *
 * Undoing detaches the entity via Scene::detachChild() and holds ownership
 * inside the command so redoing re-attaches the exact same instance.
 */
class CreateEntityCommand : public EditorCommand {
   public:
	/**
	 * Constructs a create-entity command.
	 * @param parent Scene that will receive the entity.
	 * @param entity Newly constructed entity to add on execute().
	 * @param onChanged Optional callback fired after execute() or undo().
	 */
	CreateEntityCommand(Scene* parent, _own<WorldObject> entity,
						std::function<void(WorldObject*)> onChanged = {});

	void execute() override;
	void undo() override;
	std::string description() const override { return m_description; }

	/** Returns a non-owning pointer to the created entity. */
	WorldObject* getEntity() const { return m_entityPtr; }

   private:
	Scene* m_parent;
	_own<WorldObject> m_owned;
	WorldObject* m_entityPtr;
	std::string m_description;
	std::function<void(WorldObject*)> m_onChanged;
};

/**
 * Reversible command that removes a WorldObject from its parent Scene.
 *
 * Uses Scene::detachChild() so the removed entity stays alive inside the
 * command for the lifetime of the undo entry, enabling full restoration on
 * undo().
 */
class DeleteEntityCommand : public EditorCommand {
   public:
	/**
	 * Constructs a delete-entity command.
	 * @param parent Direct parent Scene owning @p entity.
	 * @param entity Direct child of @p parent to detach on execute().
	 * @param onChanged Optional callback fired after execute() or undo().
	 */
	DeleteEntityCommand(Scene* parent, WorldObject* entity,
						std::function<void(WorldObject*)> onChanged = {});

	void execute() override;
	void undo() override;
	std::string description() const override { return m_description; }

   private:
	Scene* m_parent;
	WorldObject* m_entityPtr;
	_own<WorldObject> m_detached;
	std::string m_description;
	std::function<void(WorldObject*)> m_onChanged;
};

/**
 * Bounded linear Undo/Redo history stack for the Level Editor.
 *
 * Pushing a new command while undone commands exist discards the redo tail.
 * When the stack exceeds maxHistory, the oldest entries are dropped.
 */
class UndoStack {
   public:
	/**
	 * Creates an UndoStack with a maximum history capacity.
	 * @param maxHistory Maximum number of retained commands (must be >= 1).
	 */
	explicit UndoStack(size_t maxHistory = 200);

	/**
	 * Pushes a command onto the stack, discarding any redo tail.
	 * @param command Command to record (ignored if null).
	 * @param executeNow If true, invokes command->execute() before recording.
	 */
	void push(std::unique_ptr<EditorCommand> command, bool executeNow = false);

	/**
	 * Undoes the most recently executed command.
	 * @return True if a command was undone, false if the stack is at the start.
	 */
	bool undo();

	/**
	 * Re-applies the most recently undone command.
	 * @return True if a command was redone, false if there is nothing to redo.
	 */
	bool redo();

	/** @return True when at least one command can be undone. */
	bool canUndo() const { return m_cursor > 0; }

	/** @return True when at least one undone command can be redone. */
	bool canRedo() const { return m_cursor < m_commands.size(); }

	/** @return Description of the next command to undo, or empty string. */
	std::string undoDescription() const;

	/** @return Description of the next command to redo, or empty string. */
	std::string redoDescription() const;

	/** @return Total number of commands currently stored (undo + redo). */
	size_t size() const { return m_commands.size(); }

	/** @return Number of executed commands available to undo. */
	size_t cursor() const { return m_cursor; }

	/** Clears all stored commands and notifies listeners. */
	void clear();

	/** Registers a callback invoked whenever push(), undo(), redo(), or clear() mutates the stack. */
	void setOnChanged(std::function<void()> callback);

   private:
	void notify();

	std::vector<std::unique_ptr<EditorCommand>> m_commands;
	size_t m_cursor = 0;
	size_t m_maxHistory;
	std::function<void()> m_onChanged;
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_UNDO_STACK_H
