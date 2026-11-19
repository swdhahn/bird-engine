# Building Bird Engine from Source

This guide covers everything you need to build Bird Engine and its asset baking tools on Linux, Windows, and macOS.

---

## System Requirements

- **C++ Compiler**: Supporting C++17 or later (GCC 9+, Clang 10+, or MSVC 2019+).
- **CMake**: Version 3.23 or newer.
- **Git**: For version control.
- **Python 3**: (Optional, for test automation scripts).

### Platform Dependencies

#### Linux (Ubuntu / Debian / Arch / Fedora)
On Linux, Bird Engine requires X11 development headers and OpenGL:
```bash
# Ubuntu / Debian
sudo apt-get install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxxf86vm-dev

# Arch Linux
sudo pacman -S base-devel cmake libx11 libxrandr libxinerama libxcursor libxi mesa
```

#### macOS
Requires Xcode Command Line Tools:
```bash
xcode-select --install
```

#### Windows
Requires Visual Studio 2019 or 2022 with the "Desktop development with C++" workload installed.

---

## Cloning the Repository

Clone the repository recursively:
```bash
git clone https://github.com/swdhahn/bird-engine.git
cd bird-engine
```

> [!NOTE]
> All primary third-party libraries (Assimp, GLFW, GLEW, GLM, Shaderc, SPIRV-Cross, Volk, Jolt) are installed or downloaded through CMake into `libs/install`.

---

## Build Steps

### 1. Configure and Build the Engine
Run CMake to configure the build tree and compile:
```bash
# Configure in Debug mode (or Release)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Compile using all available CPU cores
cmake --build build -j$(nproc)
```

### 2. Install to Local Prefix
Installing copies headers, binaries, and assets into the build staging directory for client projects to find:
```bash
cmake --install build --prefix build
```

### 3. Bake Engine Assets (BuildTools)
Run the `BuildTools` executable to bake GLSL shaders and TrueType fonts into `.bird` binaries:
```bash
./build/BuildTools
```

### 4. Build and Run the Unit Tests
Bird Engine includes an extensive GoogleTest test suite covering 2D UI layout geometry, text layout, event bubbling, and shader reflection:
```bash
ctest --test-dir build --output-on-failure
```

---

## All-in-One Helper Script (`run_test.sh`)

Bird Engine includes a helper script that configures, builds, bakes, installs, and builds the sample project:
```bash
# Build in Debug mode
./run_test.sh

# Build in Release mode
./run_test.sh release

# Build and immediately launch the test project
./run_test.sh run
```

---

## Next Steps

Now that the engine is compiled, proceed to **[Project Setup](project_setup.md)** to learn how to create your own game project.
