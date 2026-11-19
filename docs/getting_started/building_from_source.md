# Building Bird Engine from Source

This guide covers everything you need to build Bird Engine and its asset baking tools on Linux, Windows, and macOS.

---

## System Requirements

- **C++ Compiler**: Supporting C++17 or later (GCC 9+, Clang 10+, or MSVC 2019+).
- **CMake**: Version 3.23 or newer.
- **Git**: For version control.
- **Python 3**: (Optional, for test automation scripts).

### Platform Dependencies

#### Windows
Requires Visual Studio 2019 or 2022 with the "Desktop development with C++" workload installed (this provides the MSVC compiler, CMake, and the Windows SDK). LLVM/Clang (`libclang`) is needed for the Dart FFI binding generator; the easiest way to get it is via [winget](https://learn.microsoft.com/en-us/windows/package-manager/winget/) or the [official LLVM installer](https://github.com/llvm/llvm-project/releases):
```powershell
winget install LLVM.LLVM
```
If CMake can't locate it automatically, point it at the install with `-DLLVM_DIR=<path>\lib\cmake\llvm -DClang_DIR=<path>\lib\cmake\clang` when configuring.

#### macOS
Requires the Xcode Command Line Tools (for the system compiler, linker, and frameworks), [Homebrew](https://brew.sh/), CMake, and LLVM — Apple's Clang does not ship `libclang`/`libclang-cpp` dev headers, which the Dart FFI binding generator (`BuildTools`) needs:
```bash
xcode-select --install
brew install cmake llvm
```
Homebrew's `llvm` is keg-only (not symlinked onto your `PATH`), but `libs/CMakeLists.txt`/`src/build-tools/CMakeLists.txt` locate it automatically via `brew --prefix llvm`. If you have multiple LLVM installs or Homebrew in a non-standard prefix, you can still override it explicitly:
```bash
cmake -B build -DLLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm" -DClang_DIR="$(brew --prefix llvm)/lib/cmake/clang"
```
> [!NOTE]
> On Apple Silicon (arm64) Macs, `libs/CMakeLists.txt` builds Assimp against the system `zlib` instead of Assimp's bundled `contrib/zlib` snapshot, since that snapshot's legacy `fdopen()` shim collides with the declaration in the macOS SDK's `<stdio.h>` under newer Clang/SDK versions. No action is needed — this is handled automatically based on `CMAKE_SYSTEM_PROCESSOR`.

#### Linux (Debian / Ubuntu)
```bash
sudo apt-get install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxxf86vm-dev \
    libclang-dev llvm-dev
```

#### Linux (Arch)
```bash
sudo pacman -S base-devel cmake libx11 libxrandr libxinerama libxcursor libxi mesa clang llvm
```

#### Linux (Fedora)
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake libX11-devel libXrandr-devel libXinerama-devel \
    libXcursor-devel libXi-devel mesa-libGL-devel mesa-libGLU-devel \
    libXxf86vm-devel clang-devel llvm-devel
```

On all Linux distributions, Bird Engine requires X11 development headers, OpenGL, and LLVM/Clang (`libclang`) for the Dart FFI binding generator, as shown above.

---

## Cloning the Repository

Clone the repository recursively:
```bash
git clone https://github.com/swdhahn/bird-engine.git
cd bird-engine
```

> [!NOTE]
> All primary third-party libraries (Assimp, GLFW, GLEW, GLM, Shaderc, SPIRV-Cross, Volk, Jolt, Dart SDK 3.8.2, and `libdart_dll`) are installed or downloaded through `libs/CMakeLists.txt` into `libs/install`:
> ```bash
> cmake -S libs -B libs && cmake --build libs
> ```
> To build without embedded Dart scripting, pass `-DBIRD_ENABLE_DART=OFF` when configuring the main project.

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
