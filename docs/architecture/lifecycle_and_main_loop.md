# Architecture: Lifecycle & Main Loop

This document describes the initialization sequence, frame loop mechanics, delta timing, and shutdown process in Bird Engine.

---

## 1. Bootstrapping Sequence

When running `./build/Bird libMyGame`, the boot sequence executes as follows:

```mermaid
sequenceDiagram
    participant Main as main.cpp (Bird Runner)
    participant Plugin as libMyGame (Plugin)
    participant App as bird::Application
    participant GP as bird::gl::GLPipeline
    participant RM as bird::ResourceManager
    participant UI as bird::UIManager
    participant Scene as bird::Scene

    Main->>Plugin: dlopen("libMyGame.so")
    Main->>Plugin: dlsym("app_entry")
    Plugin-->>Main: Returns MyGameScript*
    Main->>App: app.init(script)
    App->>GP: new GLPipeline() & init()
    App->>RM: new ResourceManager() & init()
    App->>UI: new UIManager() & init(defaultFont)
    App->>Scene: new Scene() & addComponent(script)
    App->>Scene: scene->init(nullptr)
    Scene->>Plugin: MyGameScript::init()
    App->>App: run() (Enters Frame Loop)
```

---

## 2. The Per-Frame Loop (`Application::run`)

The core loop in [src/Application.cpp](file:///home/swdhahn/Programming/cWorkspace/bird-engine-gemrun/src/Application.cpp) executes continuously until the window is closed:

```mermaid
flowchart TD
    Start["Frame Start"] --> Time["1. Calculate Delta Time (glfwGetTime)"]
    Time --> CamCheck{"2. Camera Transform Dirty?"}
    CamCheck -- Yes --> CamUpdate["Camera::updateTransformationMatrix()"]
    CamCheck -- No --> Physics["3. Fixed steps: physicsProcess(fixedDelta), then PhysicsServer::step()"]
    CamUpdate --> Physics
    Physics --> UITick["3b. UIManager::process(delta)"]
    UITick --> Scripts["4. ProcessScript::process(delta) on all registered scripts"]
    Scripts --> SceneProc["5. Application::processScene() (Recurse dirty WorldObjects)"]
    SceneProc --> Render["6. GraphicsPipeline::renderRootScene()"]
    Render --> Input["7. INPUT->tick()"]
    Render --> Window["8. Window::pollWindow() & handleResize()"]
    Window --> Loop{"Window Should Close?"}
    Loop -- No --> Start
    Loop -- Yes --> Shutdown["deinit() & Exit"]
```

### Key Phases:
1. **Delta Time**:
   Calculated as `delta = Window::getGLFWTime() - lastTime`. Delta time is passed to UI timers and all `ProcessScript::process(delta)` callbacks to ensure framerate-independent physics and animations.
2. **Fixed-Step Physics**:
   A `FixedStepClock` (default 1/60 s) turns the frame delta into zero or more whole steps. Long frames are clamped to 0.25 s and at most 8 steps run per frame; older backlog is dropped, so a slow machine slows the simulation instead of spiralling. Each step first calls `ProcessScript::physicsProcess(fixedDelta)` on every registered script, then `PhysicsServer::step()`, which steps every scene's `PhysicsWorld` (see [Physics](physics.md)). Input is ticked once per frame, outside the physics loop.
3. **Transform Dirty Checking (`processScene`)**:
   Instead of re-computing matrix transformations every time an object moves, `WorldObject` flags `m_needsMatrixUpdate = true`. During `processScene()`, only objects whose position, rotation, or scale changed will recalculate their 4x4 matrix:
   ```cpp
   for (auto& obj : scene->getChildren()) {
       if (obj->needsMatrixUpdate()) {
           obj->updateTransformationMatrix();
       }
   }
   ```
4. **Multi-Pass Rendering**:
   Delegated to `GraphicsPipeline::renderRootScene(m_scene)` which renders the 3D scene, post-processing filters, and 2D UI overlay.
5. **Platform Event Coalescing**:
   `window->pollWindow()` dispatches GLFW key and mouse events. If the window was resized, `pollResize()` coalesces rapid window manager events and triggers `handleResize()`.

---

## 3. Teardown (`Application::deinit`)

When `window->shouldWindowClose()` becomes true:
1. The active `Scene` hierarchy is deinitialized recursively: every component receives `deinit()` followed by `_deinit()`, which unregisters scripts and destroys physics bodies.
2. The root `Scene` is destroyed, taking its `PhysicsWorld`s with it, and then the `PhysicsServer` (worker threads and Jolt's global registry). A server destroyed while a world is alive aborts, since the world would dangle.
3. The `UIManager` and `GraphicsPipeline` tear down GPU resources, buffers, and shaders.
4. The application exits gracefully with exit code 0.
