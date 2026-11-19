# Tutorial 6: Scripting Games in Dart

In this tutorial, you will build and run a Bird Engine game written entirely in **Dart** — using the embedded Dart VM inside the single `Bird` executable, auto-generated FFI bindings, 3D scene nodes, Jolt Physics, and interactive 2D UI.

---

## Prerequisites

Make sure you have:
1. **LLVM / libclang** installed on your host (for `DartApiGenerator`):
   ```bash
   # Ubuntu / Debian
   sudo apt-get install libclang-dev llvm-dev

   # Arch Linux
   sudo pacman -S clang llvm
   ```
2. **Engine dependencies built** (including `DART_SDK` and `DART_SHARED_LIB`):
   ```bash
   cmake -S libs -B libs
   cmake --build libs --target DART_SDK DART_SHARED_LIB
   ```
   *(If you haven't built the other C++ dependencies yet, run `cmake --build libs` to install all of them into `libs/install`.)*

---

## Step 1: Build Bird Engine with Dart Enabled

Dart integration is enabled by default (`-DBIRD_ENABLE_DART=ON`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

During the build, CMake automatically runs all preliminary Dart setup steps if they haven't been run yet:
1. Compiles `build/tools/DartApiGenerator`.
2. Parses all script-facing C++ headers and generates:
   - `build/generated/dart_c/c_wrapper.{h,cpp}` (compiled directly into `Bird_core`/`Bird`)
   - `dart/bird_engine/lib/src/generated/*.dart` (full Dart API + Dartdoc)
3. Runs `dart pub get` for `dart/bird_engine` and `test` if `.dart_tool/package_config.json` is missing.
4. Copies `libdart_dll.{so,dylib,dll}` alongside the `Bird` executable.

Verify the generated bindings and embedded VM with:
```bash
dart analyze dart/bird_engine test
ctest --test-dir build -R DartIntegrationTest --output-on-failure
```

---

## Step 2: Run the Included Sample Dart Game

Bird Engine ships with a full 3D + Physics + 2D UI Dart demo in [`test/main.dart`](../../test/main.dart) that mirrors `test/main.cpp`:

```bash
./build/Bird test/main.dart
```

Controls in `test/main.dart`:
- **Right Mouse Button + WASD / Space / Shift**: Free-fly camera
- **`C`**: Toggle walking character controller (`CharacterControllerComponent`)
- **`F`**: Raycast push dynamic boxes under the crosshair
- **`R`**: Reset the 80-box physics pile
- **`G`**: Toggle physics collider debug wireframes

---

## Step 3: Create Your Own Dart Game Project

Create a folder for your game (e.g., `my_game/`) with a `pubspec.yaml` pointing to `bird_engine`:

### `my_game/pubspec.yaml`
```yaml
name: my_bird_game
version: 0.1.0
publish_to: none

environment:
  sdk: '>=3.8.0 <4.0.0'

dependencies:
  bird_engine:
    path: ../dart/bird_engine
```

Run `dart pub get` inside `my_game/` once (creates `.dart_tool/package_config.json`, which `Bird` discovers automatically when launching your script):
```bash
dart pub get --directory my_game
```

### `my_game/main.dart`
Every Dart game script defines:
- `void main() {}` (required by the Dart VM loader)
- `@pragma('vm:entry-point') void appEntry(int sceneAddr)` (called by `Application::init`)
- `@pragma('vm:entry-point') void appDeinit()` (optional cleanup hook)

```dart
import 'dart:ffi' as ffi;
import 'package:bird_engine/bird_engine.dart';

void main() {}

final List<BirdScript> _scripts = [];

/// Rotates its host entity every frame.
class Spinner extends BirdScript<WorldObject> {
  @override
  void process(double delta) {
    object.rotateLocal(Vector3(0.0, 1.0, 0.0), 1.5 * delta);
  }
}

/// Simple first-person camera controller.
class SimpleCamera extends BirdScript<Camera> {
  @override
  Camera wrapHost(ffi.Pointer<ffi.Void> ptr) =>
      Camera.fromHandle(ptr, owned: false);

  @override
  void init() {
    object.getWorldEnvironment().clearColor = Vector3(0.15, 0.25, 0.4);
  }

  @override
  void process(double delta) {
    final inp = inputManager!;
    final move = Vector3(
      inp.getActionValue('right') - inp.getActionValue('left'),
      0.0,
      inp.getActionValue('back') - inp.getActionValue('forward'),
    );
    if (move.length > 0.0) {
      object.translate(move.normalized() * (8.0 * delta));
    }
  }
}

@pragma('vm:entry-point')
void appEntry(int sceneAddr) {
  final inp = input!;
  inp.addAction(Action('forward')..addKey(KEY_W));
  inp.addAction(Action('back')..addKey(KEY_S));
  inp.addAction(Action('left')..addKey(KEY_A));
  inp.addAction(Action('right')..addKey(KEY_D));

  final root = sceneFromAddress(sceneAddr);
  final cubeMeshes = resourceManager!.loadMesh('assets/models/cube.obj');

  // Spinning 3D cube
  final cube = WorldObject();
  cube.addComponent(MeshComponent.fromMeshRefList(cubeMeshes));
  final spinner = Spinner();
  _scripts.add(spinner);
  cube.addComponent(spinner.createComponent());
  cube.setWorldPosition(Vector3(0.0, 0.0, -5.0));
  root.addChild(cube);

  // Camera
  final cam = Camera(1280, 720);
  final camScript = SimpleCamera();
  _scripts.add(camScript);
  cam.addComponent(camScript.createComponent());
  cam.enable();
  root.addChild(cam);

  // 2D UI Overlay with Dart closure callback
  final ui = application?.getUIManager();
  if (ui != null) {
    final panel = ui.getRoot()!.add(UIStack(UIAxis.Vertical));
    panel.setOffset(Vector2(20.0, 20.0));
    panel.setGap(8.0);
    final label = panel.add(UILabel('Hello from Dart!'));
    var clicks = 0;
    panel.add(UIButton('Click me')).setOnClick(() {
      clicks++;
      label.setText('Clicked $clicks times');
    });
  }
}

@pragma('vm:entry-point')
void appDeinit() {
  _scripts.clear();
  clearNativeCallbacks();
}
```

Launch your game directly with the `Bird` executable:
```bash
./build/Bird my_game/main.dart
```

---

## Step 4: Adding Physics in Dart

Jolt Physics is fully exposed to Dart, including `RigidBodyComponent`, `CharacterControllerComponent`, `CollisionShape`, `Joint`, `raycast()`, and contact/trigger callbacks:

```dart
final floorSettings = RigidBodySettings()..motionType = MotionType.Static;
final floor = WorldObject()..setWorldPosition(Vector3(0.0, -2.0, -5.0));
floor.addComponent(
  RigidBodyComponent(CollisionShape.box(Vector3(10.0, 0.5, 10.0)), floorSettings),
);
root.addChild(floor);

final boxSettings = RigidBodySettings()..mass = 10.0;
final box = WorldObject()..setWorldPosition(Vector3(0.0, 4.0, -5.0));
final body = RigidBodyComponent(
  CollisionShape.box(Vector3(0.5, 0.5, 0.5)),
  boxSettings,
);
body.setOnContactBegin((ContactEvent ev) {
  print('Box collided at ${ev.point}!');
});
box.addComponent(body);
root.addChild(box);
```

For fixed-timestep physics logic (like character movement or applying continuous forces), override `void physicsProcess(double fixedDelta)` in your `BirdScript<T>` subclass.

---

## Further Reading

- **[Dart Scripting & Codegen Architecture](../architecture/dart_scripting_and_codegen.md)**: Deep dive into `DartApiGenerator`, type marshalling, ownership rules, exposing new headers, and troubleshooting.
- **[`test/main.dart`](../../test/main.dart)**: Complete reference implementation combining 3D rendering, Jolt Physics, character controller, and 2D UI in Dart.
