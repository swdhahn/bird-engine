# Bird Engine

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

A modular, multi-backend C++ game engine designed for developer ergonomics, fast iteration, and clean architectural boundaries. Bird Engine abstracts low-level rendering APIs (OpenGL, Vulkan, Metal, DirectX) and features a dynamic plugin architecture, an extensible 3D scene graph, a dedicated retained-mode 2D UI framework, and an offline asset baking toolchain.

---

## Documentation

Full Godot-style documentation and guides are available in the **[`docs/`](docs/README.md)** directory:

- 🚀 **[Getting Started Guide](docs/README.md#1-getting-started)**
  - [Introduction & Philosophy](docs/getting_started/introduction.md)
  - [Building from Source](docs/getting_started/building_from_source.md)
  - [Setting Up a Game Project](docs/getting_started/project_setup.md)
- 📖 **[Step-by-Step Tutorials](docs/README.md#2-step-by-step-tutorials)**
  - [Tutorial 1: Your First 3D Scene](docs/tutorials/01_first_3d_scene.md)
  - [Tutorial 2: Camera & Input Handling](docs/tutorials/02_camera_and_input.md)
  - [Tutorial 3: UI Layouts & Widgets](docs/tutorials/03_building_ui_layouts.md)
  - [Tutorial 4: Shaders & Asset Baking](docs/tutorials/04_shaders_and_baking.md)
- 🏗️ **[Architecture Deep Dives](docs/README.md#3-architecture-deep-dives)**
  - [Application Lifecycle & Main Loop](docs/architecture/lifecycle_and_main_loop.md)
  - [Memory & Ownership Model](docs/architecture/memory_and_ownership.md)
  - [Scene Tree & Component System](docs/architecture/scene_tree_and_scripts.md)
  - [Rendering Pipeline & Backends](docs/architecture/rendering_pipeline.md)
  - [2D Retained UI Framework](docs/architecture/ui_framework.md)
  - [Resource & Asset Management](docs/architecture/resource_management.md)
  - [Animation](docs/architecture/animation.md)

---

## Key Features

- **Dynamic Plugin Architecture**: Games and applications compile into standalone dynamic libraries (`.so` / `.dll` / `.dylib`), loaded at runtime by the `Bird` executable.
- **Multi-Backend Rendering**:
  - Production **OpenGL 4.1+/4.6** pipeline with offscreen HDR framebuffers, post-processing (FXAA), and alpha-blended UI passes; the same backend builds for **OpenGL ES 3.0+** (`-DBIRD_GL_PROFILE=ES`) for mobile and WebGL 2.
  - Comprehensive **Vulkan 1.x** reference pipeline and validation harness.
  - Extension points for **Metal** and **DirectX 12**.
- **Offline Asset Compiler (`BuildTools`)**:
  - Compiles Vulkan-style GLSL to SPIR-V via `shaderc`.
  - Generates cross-platform GLSL 460, 410 and ES 300 variants via `SPIRV-Cross`.
  - Bakes TrueType fonts into atlas textures and glyph metric tables.
  - Packages 3D models and textures into high-speed binary `.bird` assets.
- **Headless 2D UI Subsystem**:
  - Two-pass layout engine with `UIStack` (flexbox-like flow), anchors, pivots, and sizing rules (`fixed`, `fill`, `fit`).
  - Rich widget suite: Windows, tabs, sliders, buttons, checkboxes, progress bars, text boxes, and modal dialogs.
  - Retained-mode single-draw-call batching via `UIDrawList`.
- **Action-Based Input**: Flexible action mapping decouples gameplay code from physical hardware keys and mouse buttons.

---

## Quick Start

### 1. Build and Run
Using the all-in-one helper script:
```bash
# Configure, build, bake assets, and launch the demo
./run_test.sh run
```

### 2. Manual Build with CMake
```bash
# 1. Configure and compile engine
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# 2. Install to build directory and bake assets
cmake --install build --prefix build
./build/BuildTools

# 3. Build sample game plugin
cd test && cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc) && cd ..

# 4. Launch engine with sample plugin
./build/Bird test/build/libBird_test
```

### 3. Run Unit Tests
```bash
ctest --test-dir build --output-on-failure
```

---

## License

© 2023 Seth Hahn. Licensed under the [MIT License](LICENSE).
