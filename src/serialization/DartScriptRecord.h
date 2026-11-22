#ifndef BIRD_SERIALIZATION_DART_SCRIPT_RECORD_H
#define BIRD_SERIALIZATION_DART_SCRIPT_RECORD_H

#include <functional>
#include <map>
#include <optional>
#include <string>

#include "../scene/components/Component.h"
#include "../util/Memory.hpp"
#include "SerializedValue.h"

namespace bird {

/**
 * Serializable representation of a Dart script component reference and its
 * exposed properties.
 *
 * Allows scenes and prefabs to author, serialize, deserialize, and override
 * Dart script components (class name, package/file path, and typed exposed
 * properties including EntityRef) identically across C++ tooling, headless
 * unit tests, and live Dart VM runs.
 *
 * When a runtime DartScriptFactory is registered via setScriptFactory(),
 * deserializing a "DartScript" component invokes the factory so the live
 * Dart BirdScript subclass is instantiated and bound to the host WorldObject.
 * When no factory is registered, a DartScriptRecord is attached directly,
 * preserving all metadata and exposed properties losslessly.
 */
class DartScriptRecord : public Component {
   public:
	/**
	 * Factory callback signature invoked when deserializing a "DartScript"
	 * component in a live Dart runtime.
	 *
	 * @param scriptClass Dart class name (e.g. "PlayerController").
	 * @param scriptPath Optional Dart library/file path.
	 * @param properties Exposed properties (already remapped for prefabs).
	 * @return A live Component (typically DartScriptComponent), or nullptr to
	 *         fall back to attaching DartScriptRecord directly.
	 */
	using ScriptFactoryFn = std::function<_own<Component>(
		const std::string& scriptClass, const std::string& scriptPath,
		const std::map<std::string, SerializedValue>& properties)>;

	/** Optional C++ lifecycle callback used by tests or bridged scripts. */
	using LifecycleCallback = std::function<void(DartScriptRecord&)>;

	DartScriptRecord() = default;

	/**
	 * Constructs a DartScriptRecord referencing a Dart class and optional path.
	 * @param scriptClass Dart script class name.
	 * @param scriptPath Optional Dart source or package URI.
	 */
	explicit DartScriptRecord(std::string scriptClass,
							  std::string scriptPath = "");

	~DartScriptRecord() override = default;

	void init() override;
	void deinit() override;

	/** Returns the Dart class name (e.g. "EnemyAI"). */
	const std::string& getScriptClass() const noexcept { return m_scriptClass; }

	/** Sets the Dart class name. */
	void setScriptClass(std::string scriptClass) {
		m_scriptClass = std::move(scriptClass);
	}

	/** Returns the optional Dart file or package path. */
	const std::string& getScriptPath() const noexcept { return m_scriptPath; }

	/** Sets the optional Dart file or package path. */
	void setScriptPath(std::string scriptPath) {
		m_scriptPath = std::move(scriptPath);
	}

	/** Sets or updates an exposed property by name. */
	void setProperty(const std::string& name, SerializedValue value);

	/** Removes an exposed property by name. */
	void removeProperty(const std::string& name);

	/** Returns true if an exposed property with `name` exists. */
	bool hasProperty(const std::string& name) const;

	/** Looks up an exposed property by name, or std::nullopt if absent. */
	std::optional<SerializedValue> getProperty(const std::string& name) const;

	/** Returns the ordered map of all exposed properties. */
	const std::map<std::string, SerializedValue>& getProperties() const noexcept {
		return m_properties;
	}

	std::map<std::string, SerializedValue>& getProperties() noexcept {
		return m_properties;
	}

	/** Number of times init() has been invoked on this record. */
	int getInitCount() const noexcept { return m_initCount; }

	/** Registers optional per-instance init/deinit observers. */
	void setOnInit(LifecycleCallback cb) { m_onInit = std::move(cb); }
	void setOnDeinit(LifecycleCallback cb) { m_onDeinit = std::move(cb); }

	/**
	 * Installs or clears the global Dart script factory invoked during
	 * scene/prefab deserialization. Pass nullptr to clear.
	 */
	static void setScriptFactory(ScriptFactoryFn factory);

	/** Returns the currently registered global script factory (may be empty). */
	static const ScriptFactoryFn& getScriptFactory();

   private:
	std::string m_scriptClass;
	std::string m_scriptPath;
	std::map<std::string, SerializedValue> m_properties;
	int m_initCount = 0;
	LifecycleCallback m_onInit;
	LifecycleCallback m_onDeinit;
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_DART_SCRIPT_RECORD_H
