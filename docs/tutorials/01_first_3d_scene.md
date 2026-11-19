# Tutorial 1: Your First 3D Scene

In this tutorial, you will learn how to:
1. Initialize the scene graph.
2. Spawn 3D entities (`WorldObject`).
3. Load 3D meshes using the `ResourceManager`.
4. Attach `MeshComponent` to entities.
5. Transform objects in 3D space (position, rotation, scale).

---

## The Scene Graph Concept

Bird Engine models worlds using a hierarchical tree:
- **`Scene`**: A container node derived from `WorldObject`. It owns child objects via unique pointers (`_own<WorldObject>`).
- **`WorldObject`**: An entity with spatial properties (position, rotation quaternion, scale) and a list of attached `Component` objects.
- **`Component`**: Modular behaviors or data attached to an entity (e.g. `MeshComponent` for 3D visuals, `ProcessScript` for gameplay logic).

```
Scene (Root)
 ├── WorldObject ("Level Ground")
 │    └── MeshComponent (Map1.glb)
 ├── WorldObject ("Floating Cube")
 │    └── MeshComponent (cube.obj)
 └── Camera (Active Viewpoint)
```

---

## Step 1: Loading a 3D Mesh

Inside your `Script<Scene>::init()` hook, access the engine's `ResourceManager` via `resource_manager()`.

To load a mesh:
```cpp
// Returns a vector of shared Mesh pointers (_ref<Mesh>)
auto meshes = resource_manager()->loadMesh("assets/models/cube.obj");
```
The `ResourceManager` automatically caches meshes by path, ensuring identical assets are loaded into GPU memory only once.

---

## Step 2: Spawning Entities and Attaching Components

Create a new `WorldObject` using the engine's exclusive-ownership factory `_own_mk<T>()`:

```cpp
// 1. Create a WorldObject
auto cube = bird::_own_mk<bird::WorldObject>();

// 2. Attach a MeshComponent
cube->addComponent(bird::_own_mk<bird::MeshComponent>(
    resource_manager()->loadMesh("assets/models/cube.obj")
));
```

---

## Step 3: Spatial Transformations

Modify the position, rotation, and scale of your object:

```cpp
// Translate 5 units up on the Y axis
cube->translate(bird::Vector3(0.0f, 5.0f, 0.0f));

// Scale the cube
cube->setScale(bird::Vector3(2.0f, 2.0f, 2.0f));

// Rotate around the Y axis by 45 degrees (converted to radians)
cube->rotate(bird::Vector3(0.0f, 1.0f, 0.0f), glm::radians(45.0f));
```

> [!NOTE]
> When you modify an object's position, rotation, or scale, Bird Engine flags the object's transform dirty (`needsMatrixUpdate() == true`). The transformation matrix is recomputed once per frame before rendering, minimizing matrix multiplication overhead.

---

## Step 4: Adding to the Scene

Add the entity to the active scene by transferring ownership via `object()->addChild()`:

```cpp
// object() returns the Scene* this script is attached to
object()->addChild(std::move(cube));
```

---

## Full Example Code

Here is the complete source for a scene displaying a map terrain and a floating cube:

```cpp
#include <Application.h>
#include <scene/Scene.h>
#include <scene/WorldObject.h>
#include <scene/components/MeshComponent.h>
#include <scene/components/Script.h>
#include <util/Math.h>

class LevelScript : public bird::Script<bird::Scene> {
public:
    void init() override {
        // 1. Spawn ground map
        auto ground = bird::_own_mk<bird::WorldObject>();
        ground->addComponent(bird::_own_mk<bird::MeshComponent>(
            resource_manager()->loadMesh("assets/models/maps/Map1.glb")
        ));
        ground->translate(bird::Vector3(0.0f, -5.0f, 30.0f));
        object()->addChild(std::move(ground));

        // 2. Spawn a floating cube
        auto cube = bird::_own_mk<bird::WorldObject>();
        cube->addComponent(bird::_own_mk<bird::MeshComponent>(
            resource_manager()->loadMesh("assets/models/cube.obj")
        ));
        cube->translate(bird::Vector3(0.0f, 2.0f, 0.0f));
        cube->rotate(bird::Vector3(0.0f, 1.0f, 0.0f), glm::radians(30.0f));
        object()->addChild(std::move(cube));
    }

    void deinit() override {}
};

extern "C" BIRD_API_EXPORT bird::Script<bird::Scene>* app_entry() {
    return new LevelScript();
}
```

---

## Next Steps

To look around and explore the 3D scene, proceed to **[Tutorial 2: Camera & Input Handling](02_camera_and_input.md)**.
