#include "DartRuntime.h"

#include <dart_api.h>
#include <dart_dll.h>

#include <filesystem>
#include <iostream>

#include "DartEventBridge.h"

namespace fs = std::filesystem;

namespace bird {

DartRuntime::~DartRuntime() {
	shutdown();
}

bool DartRuntime::init(bool startServiceIsolate, int servicePort) {
	if (m_vmInitialized) return true;

	DartDllConfig config;
	config.start_service_isolate = startServiceIsolate;
	config.service_port = servicePort;

	if (!DartDll_Initialize(config)) {
		m_lastError = "DartDll_Initialize failed";
		std::cerr << "[DartRuntime] " << m_lastError << std::endl;
		return false;
	}
	m_vmInitialized = true;
	return true;
}

std::string DartRuntime::findPackageConfig(const std::string& scriptPath) {
	std::error_code ec;
	fs::path dir = fs::weakly_canonical(fs::path(scriptPath), ec).parent_path();
	while (!dir.empty()) {
		fs::path candidate = dir / ".dart_tool" / "package_config.json";
		if (fs::exists(candidate, ec)) {
			return candidate.string();
		}
		fs::path parent = dir.parent_path();
		if (parent == dir) break;
		dir = parent;
	}
	return "";
}

bool DartRuntime::checkHandleError(Dart_Handle handle,
								   const std::string& context) {
	if (Dart_IsError(handle)) {
		const char* err = Dart_GetError(handle);
		m_lastError = context + ": " + (err ? err : "unknown Dart error");
		std::cerr << "[DartRuntime] " << m_lastError << std::endl;
		return false;
	}
	return true;
}

bool DartRuntime::loadScript(const std::string& scriptPath,
							 const std::string& packageConfigPath) {
	if (!m_vmInitialized && !init()) return false;

	if (m_isolate != nullptr) {
		bird_events_unregister_callbacks();
		Dart_EnterIsolate(m_isolate);
		if (m_rootLibrary != nullptr) {
			Dart_DeletePersistentHandle(m_rootLibrary);
			m_rootLibrary = nullptr;
		}
		Dart_ShutdownIsolate();
		m_isolate = nullptr;
	}

	std::string pkgCfg = packageConfigPath.empty()
							 ? findPackageConfig(scriptPath)
							 : packageConfigPath;
	const char* pkgPtr = pkgCfg.empty() ? nullptr : pkgCfg.c_str();

	m_isolate = DartDll_LoadScript(scriptPath.c_str(), pkgPtr, nullptr);
	if (m_isolate == nullptr) {
		m_lastError = "DartDll_LoadScript failed for: " + scriptPath;
		std::cerr << "[DartRuntime] " << m_lastError << std::endl;
		return false;
	}

	Dart_EnterIsolate(m_isolate);
	Dart_EnterScope();
	Dart_Handle rootLib = Dart_RootLibrary();
	bool ok = checkHandleError(rootLib, "Dart_RootLibrary");
	if (ok) {
		m_rootLibrary = Dart_NewPersistentHandle(rootLib);
	}
	Dart_ExitScope();
	Dart_ExitIsolate();
	return ok;
}

bool DartRuntime::runMain() {
	if (!m_isolate || !m_rootLibrary) {
		m_lastError = "No Dart script loaded";
		return false;
	}

	Dart_EnterIsolate(m_isolate);
	Dart_EnterScope();
	Dart_Handle rootLib = Dart_HandleFromPersistent(m_rootLibrary);
	Dart_Handle res = DartDll_RunMain(rootLib);
	bool ok = checkHandleError(res, "DartDll_RunMain");
	if (ok) {
		Dart_Handle drainRes = DartDll_DrainMicrotaskQueue();
		ok = checkHandleError(drainRes, "DartDll_DrainMicrotaskQueue");
	}
	Dart_ExitScope();
	Dart_ExitIsolate();
	return ok;
}

bool DartRuntime::invokeTopLevelPtr(const std::string& fnName, void* ptrArg,
									bool optional) {
	if (!m_isolate || !m_rootLibrary) {
		m_lastError = "No Dart script loaded";
		return false;
	}

	Dart_EnterIsolate(m_isolate);
	Dart_EnterScope();
	Dart_Handle rootLib = Dart_HandleFromPersistent(m_rootLibrary);
	Dart_Handle name = Dart_NewStringFromCString(fnName.c_str());

	// Check if the function exists when marked optional.
	if (optional) {
		Dart_Handle lookup = Dart_GetField(rootLib, name);
		if (Dart_IsError(lookup)) {
			Dart_ExitScope();
			Dart_ExitIsolate();
			return true;
		}
	}

	Dart_Handle args[1] = {
		Dart_NewInteger(reinterpret_cast<intptr_t>(ptrArg))};
	Dart_Handle res = Dart_Invoke(rootLib, name, 1, args);
	bool ok = checkHandleError(res, "Dart_Invoke(" + fnName + ")");
	if (ok) {
		Dart_Handle drainRes = DartDll_DrainMicrotaskQueue();
		ok = checkHandleError(drainRes, "DartDll_DrainMicrotaskQueue");
	}
	Dart_ExitScope();
	Dart_ExitIsolate();
	return ok;
}

bool DartRuntime::invokeTopLevelVoid(const std::string& fnName, bool optional) {
	if (!m_isolate || !m_rootLibrary) {
		m_lastError = "No Dart script loaded";
		return false;
	}

	Dart_EnterIsolate(m_isolate);
	Dart_EnterScope();
	Dart_Handle rootLib = Dart_HandleFromPersistent(m_rootLibrary);
	Dart_Handle name = Dart_NewStringFromCString(fnName.c_str());

	if (optional) {
		Dart_Handle lookup = Dart_GetField(rootLib, name);
		if (Dart_IsError(lookup)) {
			Dart_ExitScope();
			Dart_ExitIsolate();
			return true;
		}
	}

	Dart_Handle res = Dart_Invoke(rootLib, name, 0, nullptr);
	bool ok = checkHandleError(res, "Dart_Invoke(" + fnName + ")");
	if (ok) {
		Dart_Handle drainRes = DartDll_DrainMicrotaskQueue();
		ok = checkHandleError(drainRes, "DartDll_DrainMicrotaskQueue");
	}
	Dart_ExitScope();
	Dart_ExitIsolate();
	return ok;
}

bool DartRuntime::drainMicrotasks() {
	if (!m_isolate) return false;
	Dart_EnterIsolate(m_isolate);
	Dart_EnterScope();
	Dart_Handle res = DartDll_DrainMicrotaskQueue();
	bool ok = checkHandleError(res, "DartDll_DrainMicrotaskQueue");
	Dart_ExitScope();
	Dart_ExitIsolate();
	return ok;
}

void DartRuntime::shutdown() {
	if (m_isolate != nullptr) {
		bird_events_unregister_callbacks();
		Dart_EnterIsolate(m_isolate);
		if (m_rootLibrary != nullptr) {
			Dart_DeletePersistentHandle(m_rootLibrary);
			m_rootLibrary = nullptr;
		}
		Dart_ShutdownIsolate();
		m_isolate = nullptr;
	}
	if (m_vmInitialized) {
		DartDll_Shutdown();
		m_vmInitialized = false;
	}
}

}  // namespace bird
