#include <dlfcn.h>

#include <iostream>

#include "Application.h"
#include "scene/Scene.h"
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
	path +=
#if defined(_WIN32) || defined(_WIN64)
		".dll";
#elif defined(__APPLE__)
			".dylib";
#elif defined(__linux__) || defined(__unix__)
			".so";
#else
					".so";
#endif



	void* app_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
	if (!app_handle) {
		std::cerr << "Could not load app dynamic library: " << path
				  << std::endl;
		std::cerr << "dlopen error: " << dlerror() << std::endl;
		exit(1);
	}
	bird::Scene* (*app_entry)();
	void* app_entry_void = dlsym(app_handle, "app_entry");
	if (app_entry_void != nullptr) {
		app_entry = reinterpret_cast<bird::Scene* (*)()>(app_entry_void);
	} else {
		std::cerr << "Could not locate any 'bird::Scene* app_entry();'"
				  << std::endl;
		std::cerr << "dlsym error: " << dlerror() << std::endl;
		dlclose(app_handle);
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

	dlclose(app_handle);

	return 0;  //*/
}
