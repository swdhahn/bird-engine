#include <iostream>

#include "Application.h"
#include "scene/Scene.h"

// --- Platform Specific Includes & Macros ---
#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>

	// Windows Definitions
	#define LIB_HANDLE HMODULE
	#define LIB_OPEN(path) LoadLibraryA(path)
	#define LIB_SYM(handle, name) GetProcAddress(handle, name)
	#define LIB_CLOSE(handle) FreeLibrary(handle)

	// Helper to get a readable string from Windows errors
	std::string GetPlatformError() {
		DWORD error = GetLastError();
		if (error == 0) return "Unknown Error";
		LPSTR messageBuffer = nullptr;
		size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
									 NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
		std::string message(messageBuffer, size);
		LocalFree(messageBuffer);
		return message;
	}

// Windows usually needs an extension for LoadLibrary, unlike dlopen which sometimes guesses
const std::string LIB_EXT = ".dll";

#else
#include <dlfcn.h>

// POSIX Definitions (Linux/macOS)
#define LIB_HANDLE void*
#define LIB_OPEN(path) dlopen(path, RTLD_NOW | RTLD_GLOBAL)
#define LIB_SYM(handle, name) dlsym(handle, name)
#define LIB_CLOSE(handle) dlclose(handle)

std::string GetPlatformError() {
	const char* err = dlerror();
	return err ? std::string(err) : "Unknown Error";
}

#if defined(__APPLE__)
const std::string LIB_EXT = ".dylib";
#else
const std::string LIB_EXT = ".so";
#endif
#endif

/*
 *
 * 1. Shader has a list of materials
 * 2. each material has a list of mesh pointers
 * 3. each model has a list of mesh pointers
 *
 * 1. We loop through shaders. loop through materials in each shader.
 * 2. We loop through models. Connect pair shader material and material for each
 * mesh. Loop through meshes.
 *
 * shader -> entity -> mesh -> material
 *
 */

/* TODO:
 *
 * use glBufferSubData for updates and glBufferData for buffer
 * creation/allocation only
 *
 * Finish linear and bicubic/near interpolation for textures
 * Finish transformations
 *
 * /home/swdhahn/Programming/cWorkspace/bird-engine/src/renders/generic/Texture.cpp: In destructor ‘virtual bird::Texture::~Texture()’:
/home/swdhahn/Programming/cWorkspace/bird-engine/src/renders/generic/Texture.cpp:67:16:
warning: deleting ‘void*’ is undefined [-Wdelete-incomplete] 67 |         delete
m_pData; |                ^~~~~~~

 *
 */
int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Please provide a path to your application" << std::endl;
		exit(1);
	}
	std::string path(argv[1]);
	path += LIB_EXT;



	LIB_HANDLE app_handle = LIB_OPEN(path.c_str());
	if (!app_handle) {
		std::cerr << "Could not load app dynamic library: " << path
				  << std::endl;
		std::cerr << "dlopen error: " << GetPlatformError() << std::endl;
		exit(1);
	}
	bird::Scene* (*app_entry)();
	auto app_entry_raw = LIB_SYM(app_handle, "app_entry");
	if (app_entry_raw != nullptr) {
		app_entry = reinterpret_cast<bird::Scene* (*)()>(app_entry_raw);
	} else {
		std::cerr << "Could not locate any 'bird::Scene* app_entry();'"
				  << std::endl;
		std::cerr << "dlsym error: " << GetPlatformError() << std::endl;
		LIB_CLOSE(app_handle);
		exit(1);
	}
	bird::Scene* scene = app_entry();

	bird::Application app(*scene);

	try {
		app.run();
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	LIB_CLOSE(app_handle);

	return 0;  //*/
}
