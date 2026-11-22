#ifndef BIRD_EDITOR_COMPONENT_INSPECTOR_H
#define BIRD_EDITOR_COMPONENT_INSPECTOR_H

#include <array>
#include <functional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "../scene/2D/UICheckBox.h"
#include "../scene/2D/UIColorPicker.h"
#include "../scene/2D/UIDropdown.h"
#include "../scene/2D/UILabel.h"
#include "../scene/2D/UINumberField.h"
#include "../scene/2D/UIStack.h"
#include "../scene/2D/UITextBox.h"
#include "../scene/components/Component.h"
#include "../util/Math.h"
#include "UndoStack.h"

namespace bird::editor {

/**
 * Helper passed to component inspector callbacks to construct labeled,
 * Undo/Redo-aware property rows using Bird Engine's custom UI widgets.
 *
 * Every editable helper automatically captures the previous value, applies
 * the new value through the supplied setter, and pushes a reversible
 * LambdaCommand onto the active UndoStack (when present).
 */
class InspectorBuilder {
   public:
	/**
	 * Constructs a builder targeting a vertical UIStack container.
	 * @param container Parent stack receiving property rows (must not be null).
	 * @param undoStack Optional UndoStack for recording reversible edits.
	 * @param onMutated Optional callback fired after any undo/redo mutation so
	 *                  the InspectorPanel can refresh widget values.
	 */
	InspectorBuilder(UIStack* container, UndoStack* undoStack = nullptr,
					 std::function<void()> onMutated = {});

	/** Adds a non-editable "Label: Value" row. */
	UILabel* addReadOnly(const std::string& label, const std::string& value);

	/**
	 * Adds a live-editable floating-point property backed by UINumberField.
	 * @param label Field caption shown on the left.
	 * @param getter Reads the current value from the component.
	 * @param setter Writes a new value to the component.
	 * @param min Minimum allowed value.
	 * @param max Maximum allowed value.
	 * @param step Increment for arrows, scroll wheel, and scrubbing.
	 * @param precision Decimal places displayed.
	 */
	UINumberField* addFloat(const std::string& label,
							std::function<float()> getter,
							std::function<void(float)> setter,
							double min = -100000.0, double max = 100000.0,
							double step = 0.1, int precision = 2);

	/**
	 * Adds a live-editable integer property backed by UINumberField.
	 */
	UINumberField* addInt(const std::string& label,
						  std::function<int()> getter,
						  std::function<void(int)> setter, int min = -100000,
						  int max = 100000, int step = 1);

	/**
	 * Adds a live-editable boolean property backed by UICheckBox.
	 */
	UICheckBox* addBool(const std::string& label, std::function<bool()> getter,
						std::function<void(bool)> setter);

	/**
	 * Adds a live-editable single-line string property backed by UITextBox.
	 */
	UITextBox* addString(const std::string& label,
						 std::function<std::string()> getter,
						 std::function<void(const std::string&)> setter);

	/**
	 * Adds a 3-component vector row (X, Y, Z UINumberFields).
	 */
	std::array<UINumberField*, 3> addVec3(
		const std::string& label, std::function<Vector3()> getter,
		std::function<void(const Vector3&)> setter, double min = -100000.0,
		double max = 100000.0, double step = 0.1, int precision = 2);

	/**
	 * Adds an RGB color picker button (UIColorButton) for linear Vector3 colors.
	 */
	UIColorButton* addColorRGB(const std::string& label,
							   std::function<Vector3()> getter,
							   std::function<void(const Vector3&)> setter);

	/**
	 * Adds an RGBA color picker button (UIColorButton) for Vector4 colors.
	 */
	UIColorButton* addColorRGBA(const std::string& label,
								std::function<Vector4()> getter,
								std::function<void(const Vector4&)> setter);

	/**
	 * Adds an enum/choice selector backed by UIDropdown.
	 */
	UIDropdown* addDropdown(const std::string& label,
							std::vector<std::string> items,
							std::function<size_t()> getter,
							std::function<void(size_t)> setter);

	/** Returns the underlying container stack for custom widget layouts. */
	UIStack* getContainer() const { return m_container; }

   private:
	UIStack* makeRow(const std::string& label);

	UIStack* m_container;
	UndoStack* m_undoStack;
	std::function<void()> m_onMutated;
};

/**
 * Global registry mapping concrete Component types to Inspector UI builders.
 *
 * Built-in engine components (LightComponent, MeshComponent, RigidBodyComponent)
 * are registered automatically on first access. Game code can expose custom
 * components with a single call:
 *
 * @code
 * bird::editor::ComponentInspectorRegistry::instance()
 *     .registerInspector<HealthComponent>(
 *         "Health", [](HealthComponent* c, bird::editor::InspectorBuilder& ui) {
 *             ui.addFloat("Max HP", [c] { return c->maxHp; },
 *                         [c](float v) { c->maxHp = v; }, 1.0, 1000.0, 1.0, 0);
 *         });
 * @endcode
 */
class ComponentInspectorRegistry {
   public:
	using RawInspectFn = std::function<void(Component*, InspectorBuilder&)>;

	/** Returns the process-wide registry (lazily registering built-ins). */
	static ComponentInspectorRegistry& instance();

	/**
	 * Registers (or replaces) an inspector builder for Component subclass @p T.
	 * @tparam T Concrete subclass of bird::Component.
	 * @param displayName Title shown in the Inspector's collapsible section.
	 * @param builderFn Callback that populates @p InspectorBuilder for an instance.
	 */
	template <typename T>
	void registerInspector(std::string displayName,
						   std::function<void(T*, InspectorBuilder&)> builderFn) {
		Entry entry;
		entry.displayName = std::move(displayName);
		entry.inspect = [fn = std::move(builderFn)](Component* comp,
													InspectorBuilder& builder) {
			if (auto* typed = dynamic_cast<T*>(comp)) {
				fn(typed, builder);
			}
		};
		m_entries[std::type_index(typeid(T))] = std::move(entry);
	}

	/**
	 * Returns true if an inspector is registered for @p type.
	 */
	bool hasInspector(std::type_index type) const;

	/**
	 * Returns the registered display name for @p component, or a cleaned
	 * fallback name derived from typeid when unregistered.
	 */
	std::string displayNameFor(const Component& component) const;

	/**
	 * Populates @p builder with property widgets for @p component.
	 * @return True if a registered inspector ran, false if unregistered.
	 */
	bool buildInspector(Component* component, InspectorBuilder& builder) const;

   private:
	struct Entry {
		std::string displayName;
		RawInspectFn inspect;
	};

	ComponentInspectorRegistry();
	void registerBuiltins();

	std::unordered_map<std::type_index, Entry> m_entries;
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_COMPONENT_INSPECTOR_H
