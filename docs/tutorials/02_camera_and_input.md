# Tutorial 2: Camera & Input Handling

In this tutorial, you will learn how to:
1. Set up an active `Camera` entity.
2. Bind logical input actions to keyboard keys and mouse buttons.
3. Write a per-frame `ProcessScript` to control a 3D camera.
4. Implement smooth, delta-timed flight movement and rotation.

---

## 1. The Camera Entity

`bird::Camera` is a 3D `WorldObject` that produces a perspective projection matrix. To spawn a camera and register it as the active viewpoint in the engine:

```cpp
// Create camera with width and height matching viewport resolution
auto camera = bird::_own_mk<bird::Camera>(1280, 720);

// Set clear color (sky blue)
camera->getWorldEnvironment().clearColor = bird::Vector3(0.2f, 0.6f, 0.85f);

// Enable this camera as the active viewpoint in the renderer
camera->enable();

// Add to scene
object()->addChild(std::move(camera));
```

---

## 2. Binding Input Actions

Bird Engine uses an **action-based** input system. Rather than hard-coding physical keys throughout your gameplay scripts, you define named `Action` instances and bind physical keys or mouse buttons to them.

In your main scene script's `init()`:
```cpp
bird::INPUT->addAction(bird::Action("move_forward").addKey(KEY_W));
bird::INPUT->addAction(bird::Action("move_backward").addKey(KEY_S));
bird::INPUT->addAction(bird::Action("move_left").addKey(KEY_A));
bird::INPUT->addAction(bird::Action("move_right").addKey(KEY_D));
bird::INPUT->addAction(bird::Action("move_up").addKey(KEY_SPACE));
bird::INPUT->addAction(bird::Action("move_down").addKey(KEY_LEFT_SHIFT));

// Hold the right mouse button to look around.
bird::INPUT->addAction(bird::Action("look").addMouseButton(MOUSE_BUTTON_2));
```

Mouse movement is not an action: read it with `Input::getMouseDelta()` (pixels since the last frame, +x right, +y down). `Input::setMouseCaptured(true)` hides and locks the cursor (raw motion where supported) and stops the UI from receiving pointer events until released, which is what a mouse-look camera wants.

---

## 3. Creating a Controller with `ProcessScript<T>`

To execute logic every frame, derive from `bird::ProcessScript<T>`. When attached to a `WorldObject`, the `Application` automatically calls `process(float delta)` on your script before rendering.

Inside `ProcessScript<Camera>`:
- `object()` returns a typed pointer to the host `Camera`.
- `input()` returns a pointer to the global `Input` manager.
- `delta` is the frame time in seconds (e.g. `0.0166f` at 60 FPS).

### Implementing Free-Fly Controls

```cpp
#include <scene/Camera.h>
#include <scene/components/ProcessScript.h>
#include <util/Math.h>

class FlyCameraController : public bird::ProcessScript<bird::Camera> {
public:
    void init() override {
        // Set sky color on camera
        object()->getWorldEnvironment().clearColor = bird::Vector3(0.15f, 0.4f, 0.65f);
    }

    void deinit() override {}

    void process(float delta) override {
        const float moveSpeed = 12.0f;
        const float sensitivity = 0.0025f;  // radians per pixel

        // 1. Mouse look while the right button is held (cursor captured).
        const bool looking = input()->getActionValue("look") > 0.5f;
        input()->setMouseCaptured(looking);
        if (looking) {
            const bird::Vector2 mouse = input()->getMouseDelta();
            m_yaw -= mouse.x * sensitivity;
            m_pitch = glm::clamp(m_pitch - mouse.y * sensitivity,
                                 glm::radians(-89.0f), glm::radians(89.0f));
        }
        // Yaw about world up, then pitch about the camera's right axis. The
        // camera looks down its local -Z.
        object()->setWorldRotation(glm::angleAxis(m_yaw, bird::Vector3(0, 1, 0)) *
                                   glm::angleAxis(m_pitch, bird::Vector3(1, 0, 0)));

        // 2. Calculate movement direction
        float forward = input()->getActionValue("move_forward") - 
                        input()->getActionValue("move_backward");
        float strafe  = input()->getActionValue("move_right") - 
                        input()->getActionValue("move_left");
        float vertical = input()->getActionValue("move_up") - 
                         input()->getActionValue("move_down");

        // Camera-local direction: +X right, +Y up, -Z forward.
        bird::Vector3 moveDir(strafe, vertical, -forward);

        // 3. Rotate into world space and move the eye
        object()->translate(object()->getWorldRotation() *
                            object()->getLocalRotation() *
                            moveDir * moveSpeed * delta);
    }

private:
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};
```

---

## 4. Attaching the Controller to the Camera

In your scene initialization:
```cpp
auto cam = bird::_own_mk<bird::Camera>(1280, 720);

// Attach the ProcessScript controller
cam->addComponent(bird::_own_mk<FlyCameraController>());

cam->enable();
object()->addChild(std::move(cam));
```

---

## Next Steps

Now that you have full 3D navigation, proceed to **[Tutorial 3: UI Layouts & Widgets](03_building_ui_layouts.md)** to build interactive 2D user interfaces.
