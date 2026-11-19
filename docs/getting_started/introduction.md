# Introduction to Bird Engine

Bird Engine is a modern C++17 game engine designed with a focus on modularity, fast iteration, and clean API boundaries.

---

## Core Philosophy

### 1. Dynamic Library Architecture (Hot-Reload Friendly)
Unlike engines that require recompiling the entire engine executable with your game code, Bird Engine decouples the engine core (`libBird_core`) and the runner (`Bird`) from your game logic. 

Your application compiles as a shared library (`.so` on Linux, `.dll` on Windows, `.dylib` on macOS) that exports a single factory symbol:
```cpp
extern "C" BIRD_API_EXPORT bird::Script<bird::Scene>* app_entry();
```
The runner executable dynamically loads your library via `dlopen`/`LoadLibrary`, instantiates your root scene script, and starts the game loop. This architecture lays the groundwork for live code reloading and fast iteration.

### 2. Multi-Backend Graphics Abstraction
All graphics APIs are encapsulated behind the abstract `bird::GraphicsPipeline` interface:
- **OpenGL (Primary)**: Fully implemented OpenGL 4.1+/4.6 backend (`GLPipeline`) featuring forward scene passes, post-processing (FXAA/color grading) via offscreen Framebuffer Objects (FBOs), and alpha-blended 2D UI overlay passes.
- **Vulkan**: Comprehensive pipeline harness (`VKPipeline` & `ApplicationVK`) supporting validation layers, swapchains, SPIR-V shaders, and multi-threaded rendering design.
- **Metal & DirectX 12**: Platform extension points designed for native macOS and Windows backends.

As an engine user, you interact strictly with high-level types (`Mesh`, `Texture`, `Shader`, `Camera`, `Material`) in the `bird::` namespace.

### 3. Native Asset Pipeline (.bird)
Bird Engine includes an offline compiler called `BuildTools`. Rather than parsing heavy assets at runtime (e.g. compiling complex GLSL at startup or parsing multi-megabyte glTF/FBX models with Assimp on launch), `BuildTools` bakes all source assets into compact, pre-validated binary formats with the `.bird` extension:
- **Shaders**: Compiles Vulkan-style GLSL to SPIR-V via `shaderc`, reflects uniform bindings and push constants, and generates GLSL 460 and GLSL 410 variants via `SPIRV-Cross`.
- **Fonts**: Generates SDF/bitmap glyph atlases and metrics from `.ttf` and `.otf` fonts.
- **3D Models**: Extracts vertex attributes, normal data, and texture coordinates into binary meshes and materials.

### 4. Decoupled 2D Retained UI Framework
Bird Engine features a dedicated, headless 2D UI system (`bird::UIManager`) with a two-pass layout engine (flexbox-like `UIStack`, anchors, pivots, size rules) and a comprehensive suite of modern desktop controls (windows, tabs, sliders, text inputs, dropdowns, modal dialogs). Because the UI system is graphics-agnostic and relies on `UIDrawList` batching, it is 100% unit-testable without creating an OpenGL or Vulkan context.

---

## High-Level Engine Overview

```
                               +-------------------+
                               |  Bird Executable  |
                               +---------+---------+
                                         | dlopen()
                               +---------v---------+
                               |  Your Game Plugin |
                               |  (app_entry())    |
                               +---------+---------+
                                         |
                       +-----------------v-----------------+
                       |         bird::Application         |
                       +-----------------+-----------------+
                                         |
     +-------------------+---------------+-------------------+-------------------+
     |                   |                                   |                   |
+----v-----+       +-----v----+                        +-----v----+        +-----v----+
|  Input   |       |  Scene   |                        |    UI    |        | Resource |
| Manager  |       | Hierarchy|                        | Manager  |        | Manager  |
+----------+       +-----+----+                        +-----+----+        +----------+
                         |                                   |
                  +------v------+                     +------v------+
                  | WorldObjects|                     |  UIObjects  |
                  | & Components|                     | (2D Widgets)|
                  +------+------+                     +------+------+
                         |                                   |
                         +-----------------+-----------------+
                                           |
                                  +--------v--------+
                                  | GraphicsPipeline|
                                  | (OpenGL/Vulkan) |
                                  +-----------------+
```

---

## Next Steps

To begin building with Bird Engine:
1. Proceed to **[Building from Source](building_from_source.md)** to compile the engine and its tools.
2. Review **[Project Setup](project_setup.md)** to configure your first game library.
