#ifndef BIRD_DART_RUNTIME_H
#define BIRD_DART_RUNTIME_H

#include <cstdint>
#include <string>

struct _Dart_Isolate;
typedef struct _Dart_Isolate* Dart_Isolate;
struct _Dart_Handle;
typedef struct _Dart_Handle* Dart_Handle;
typedef Dart_Handle Dart_PersistentHandle;

namespace bird {

class Scene;

/**
 * Manages the embedded Dart VM lifecycle and root isolate inside the Bird
 * executable using libdart_dll.
 */
class DartRuntime {
   public:
	DartRuntime() = default;
	~DartRuntime();

	DartRuntime(const DartRuntime&) = delete;
	DartRuntime& operator=(const DartRuntime&) = delete;

	/**
	 * Initializes the embedded Dart VM. Safe to call once per process.
	 */
	bool init(bool startServiceIsolate = false, int servicePort = 5858);

	/**
	 * Loads a Dart script (.dart or .dill) into a new root isolate.
	 * If packageConfigPath is empty, searches upward from scriptPath for
	 * `.dart_tool/package_config.json`.
	 */
	bool loadScript(const std::string& scriptPath,
					const std::string& packageConfigPath = "");

	/**
	 * Runs the `main()` function of the loaded Dart library.
	 */
	bool runMain();

	/**
	 * Invokes a top-level Dart function taking a single int64 argument
	 * (such as a native pointer address). Returns false if the function threw
	 * an error. If optional is true, missing functions return true silently.
	 */
	bool invokeTopLevelPtr(const std::string& fnName, void* ptrArg,
						   bool optional = false);

	/**
	 * Invokes a zero-argument top-level Dart function.
	 */
	bool invokeTopLevelVoid(const std::string& fnName, bool optional = false);

	/**
	 * Drains the isolate's microtask queue.
	 */
	bool drainMicrotasks();

	/**
	 * Shuts down the active isolate and tears down the embedded Dart VM.
	 */
	void shutdown();

	bool isInitialized() const { return m_vmInitialized; }
	bool hasIsolate() const { return m_isolate != nullptr; }
	const std::string& lastError() const { return m_lastError; }

   private:
	bool checkHandleError(Dart_Handle handle, const std::string& context);
	static std::string findPackageConfig(const std::string& scriptPath);

	bool m_vmInitialized = false;
	Dart_Isolate m_isolate = nullptr;
	Dart_PersistentHandle m_rootLibrary = nullptr;
	std::string m_lastError;
};

}  // namespace bird

#endif	// BIRD_DART_RUNTIME_H
