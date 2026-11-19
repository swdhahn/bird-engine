# Setting Up a Game Project

In Bird Engine, your game or application is built as a **shared library** (plugin). The Bird runner executable (`./build/Bird`) loads your library at runtime.

---

## Directory Layout

A standard Bird Engine project has the following layout:
```
my_game/
├── CMakeLists.txt
└── src/
    └── main.cpp
```

---

## 1. Configuring `CMakeLists.txt`

Your game project uses CMake to link against `Bird::Bird_core`:

```cmake
cmake_minimum_required(VERSION 3.23)
project(MyGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Create a shared library for your game
add_library(MyGame SHARED src/main.cpp)

# 2. Locate the installed Bird_Engine package
# Point to your bird-engine build install directory
set(Bird_Engine_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bird-engine/build/lib/cmake/Bird_Engine")
find_package(Bird_Engine CONFIG REQUIRED)

# 3. Link against Bird_core
target_link_libraries(MyGame PUBLIC Bird::Bird_core)
```

---

## 2. Implementing the Entry Point (`main.cpp`)

Your library must export a C-linkage function named `app_entry()` that returns a `bird::Script<bird::Scene>*`:

```cpp
#include <Application.h>
#include <scene/Scene.h>
#include <scene/components/Script.h>
#include <iostream>

// Main scene script
class MyGameScript : public bird::Script<bird::Scene> {
public:
    void init() override {
        std::cout << "MyGame initialized!" << std::endl;
        // Setup 3D objects, cameras, lights, and UI here
    }

    void deinit() override {
        std::cout << "MyGame shutting down!" << std::endl;
    }
};

// Exported factory function
extern "C" BIRD_API_EXPORT bird::Script<bird::Scene>* app_entry() {
    return new MyGameScript();
}
```

---

## 3. Compiling Your Game

From your project directory:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```
This produces `build/libMyGame.so` (Linux), `build/MyGame.dll` (Windows), or `build/libMyGame.dylib` (macOS).

---

## 4. Running Your Game

Run the compiled game by providing the path to your library (without the file extension) to the `Bird` binary:

```bash
# Syntax: ./path/to/Bird <path/to/library_without_extension> [optional_resource_directory]
../bird-engine/build/Bird ./build/libMyGame
```

The runner appends `.so` / `.dll` / `.dylib` according to your operating system, loads `app_entry()`, initializes the engine systems, and starts the game loop.

---

## Next Steps

Now you are ready to build scenes and gameplay! Follow the step-by-step guides starting with **[Tutorial 1: Your First 3D Scene](../tutorials/01_first_3d_scene.md)**.
