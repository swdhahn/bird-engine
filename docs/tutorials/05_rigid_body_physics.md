# Tutorial 5: Rigid-Body Physics

This tutorial adds physics to a scene:
1. a static floor and falling boxes;
2. pushing bodies with impulses from a script;
3. reacting to contacts and trigger zones;
4. picking objects with a raycast;
5. a walking character controller.

The design behind these APIs is described in [Physics](../architecture/physics.md).

---

## 1. Bodies Are Components

Physics needs no setup. The `Application` creates the `PhysicsServer` before your scene, and the root scene creates its world the first time a body joins. Add a `RigidBodyComponent` to an object before adding the object to a scene:

```cpp
#include <physics/CollisionShape.h>
#include <physics/RigidBodyComponent.h>
#include <scene/Scene.h>
#include <scene/components/MeshComponent.h>

using namespace bird;

// cube.obj spans -1..1, so a unit box (half extents 1) matches the mesh.
void addFloorAndBoxes(Scene* scene) {
    auto cube = RESOURCE_MANAGER->loadMesh("assets/models/cube.obj");
    const CollisionShape unitBox = CollisionShape::box(Vector3(1.0f));

    RigidBodySettings still;
    still.motionType = MotionType::Static;

    auto floor = _own_mk<WorldObject>();
    floor->addComponent(_own_mk<MeshComponent>(cube));
    floor->addComponent(_own_mk<RigidBodyComponent>(unitBox, still));
    floor->setWorldPosition({0, -2.5f, -15});
    floor->setScale({10, 0.5f, 10});  // the collider follows the object's scale
    scene->addChild(std::move(floor));

    for (int i = 0; i < 10; i++) {
        auto box = _own_mk<WorldObject>();
        box->addComponent(_own_mk<MeshComponent>(cube));
        box->addComponent(_own_mk<RigidBodyComponent>(unitBox));  // dynamic by default
        box->setWorldPosition({0.3f * (i % 3), 2.0f + 1.2f * i, -15});
        box->setScale(Vector3(0.5f));
        scene->addChild(std::move(box));
    }
}
```

Each step, dynamic bodies write their pose back to their objects, so rendering needs nothing extra. Moving an object yourself (`setWorldPosition`, `translate`, `rotate`) teleports its body at the next step.

### Motion types
| Type | Moved by | Typical use |
|------|----------|-------------|
| `Static` | Never simulated (teleport only) | Floors, walls, level geometry |
| `Kinematic` | Your code; pushes dynamic bodies | Moving platforms, doors |
| `Dynamic` | Gravity, forces, collisions | Crates, debris, balls |

Level geometry can use its render meshes directly. For a whole level, split it into a few static chunks that share the level's transform:

```cpp
void addLevelCollision(Scene* scene) {
    auto meshes = RESOURCE_MANAGER->loadMesh("assets/models/maps/Map1.glb");
    const Vector3 levelPosition(0, -5, 30);  // the rendered map's transform

    RigidBodySettings level;
    level.motionType = MotionType::Static;
    for (const CollisionShape& chunk : CollisionShape::triangleMeshChunks(meshes, 3, 3)) {
        auto collider = _own_mk<WorldObject>();
        collider->addComponent(_own_mk<RigidBodyComponent>(chunk, level));
        collider->setWorldPosition(levelPosition);
        scene->addChild(std::move(collider));
    }
}
```

Triangle meshes must be static or kinematic; give moving bodies a `convexHull` instead.

---

## 2. Forces Belong in `physicsProcess`

`ProcessScript::physicsProcess(fixedDelta)` runs just before every fixed physics step (60 times per second by default, regardless of frame rate). Forces applied there are consistent at any frame rate:

```cpp
class Thruster : public ProcessScript<WorldObject> {
   public:
    void init() override {
        for (auto& component : components()) {
            if (auto* body = dynamic_cast<RigidBodyComponent*>(component.get())) m_body = body;
        }
    }
    void deinit() override {}

    void physicsProcess(float fixedDelta) override {
        if (input()->getActionValue("thrust") > 0.5f) {
            m_body->applyForce({0, 30.0f * m_body->getMass(), 0});  // 3 g upwards
        }
    }

    void process(float delta) override {}  // per-frame visuals, input edges

   private:
    RigidBodyComponent* m_body = nullptr;
};
```

For a one-off kick, use `applyImpulse` (an instant change of momentum) from either hook.

---

## 3. Contacts and Trigger Zones

A **sensor** body detects overlaps without pushing anything. Scripts on the same object receive the events through virtual hooks:

```cpp
class CoinZone : public Script<WorldObject> {
   public:
    void init() override {}
    void deinit() override {}

    void onTriggerEnter(const ContactEvent& event) override {
        std::cout << "Something entered the zone" << std::endl;
    }
    void onTriggerExit(const ContactEvent& event) override {}
};

void addZone(Scene* scene) {
    RigidBodySettings sensor;
    sensor.motionType = MotionType::Static;
    sensor.isSensor = true;

    auto zone = _own_mk<WorldObject>();
    zone->addComponent(_own_mk<RigidBodyComponent>(CollisionShape::box({3, 1, 2}), sensor));
    zone->addComponent(_own_mk<CoinZone>());
    zone->setWorldPosition({0, -1, -10});
    scene->addChild(std::move(zone));
}
```

Solid bodies report `onContactBegin` / `onContactEnd` instead. A lambda works as well as a script:

```cpp
body->setOnContactBegin([](const ContactEvent& e) {
    // e.self is this body, e.other the one it hit; e.normal points from self to other.
});
```

Events arrive on the main thread after the step. Begin and end fire **once per pair of bodies**, and a resting body that falls asleep keeps its contact. Removing objects inside a callback is safe; the partner of a destroyed body receives its end event.

---

## 4. Picking with a Raycast

Cast from the camera along its view direction; the camera looks down its local -Z:

```cpp
void pushWhatTheCameraSees(Scene* scene, Camera* camera) {
    const Quaternion rotation = camera->getWorldRotation() * camera->getLocalRotation();
    const Vector3 forward = rotation * Vector3(0, 0, -1);

    auto hit = scene->getPhysicsWorld()->raycast(camera->getWorldPosition(), forward, 200.0f);
    if (hit && hit->body->getMotionType() == MotionType::Dynamic) {
        hit->body->applyImpulseAt(forward * 10.0f * hit->body->getMass(), hit->point);
    }
}
```

`raycastAll`, `shapeCast` and `overlap` follow the same pattern. Each takes an optional layer mask and one body to ignore.

---

## 5. Layers

Put bodies on layers and choose what each layer collides with:

```cpp
RigidBodySettings ghost;
ghost.layers = physicsLayer(1);  // lives on layer 1
ghost.mask = physicsLayer(1);    // only collides with layer 1 (passes through the floor on layer 0)
```

Both bodies must agree: A's layers must intersect B's mask, and B's layers A's mask.

---

## 6. A Walking Character

A `CharacterControllerComponent` gives a player or NPC capsule movement: it walks up stairs and slopes, stops at walls and steep slopes, and pushes light dynamic bodies. The object's position is the character's feet.

```cpp
#include <physics/CharacterControllerComponent.h>

class PlayerController : public ProcessScript<WorldObject> {
   public:
    explicit PlayerController(Camera* camera) : m_camera(camera) {}
    void init() override {
        for (auto& component : components()) {
            if (auto* c = dynamic_cast<CharacterControllerComponent*>(component.get())) m_character = c;
        }
    }
    void deinit() override {}

    void physicsProcess(float fixedDelta) override {
        // Steer in the camera's horizontal frame.
        Vector3 forward = m_camera->getWorldRotation() * Vector3(0, 0, -1);
        forward.y = 0;
        forward = glm::normalize(forward);
        const Vector3 right(-forward.z, 0, forward.x);
        const float ahead = input()->getActionValue("move_forward") -
                            input()->getActionValue("move_backward");
        const float side = input()->getActionValue("move_right") -
                           input()->getActionValue("move_left");
        m_character->setMoveVelocity((forward * ahead + right * side) * 4.5f);  // persists
        if (input()->getActionValue("jump") > 0.5f) m_character->jump(5.0f);   // only from the ground
    }

    void process(float delta) override {
        // First-person camera at eye height.
        m_camera->setWorldPosition(object()->getWorldPosition() + Vector3(0, 1.6f, 0));
    }

   private:
    Camera* m_camera;
    CharacterControllerComponent* m_character = nullptr;
};

void addPlayer(Scene* scene, Camera* camera) {
    CharacterSettings settings;  // 1.8 m tall, 0.3 m radius, 0.4 m steps, 50 degree slopes
    settings.pushStrength = 600.0f;

    auto player = _own_mk<WorldObject>();
    player->addComponent(_own_mk<CharacterControllerComponent>(settings));
    player->addComponent(_own_mk<PlayerController>(camera));
    player->setWorldPosition({0, 0, -8});
    scene->addChild(std::move(player));
}
```

`getGroundState()` reports `OnGround`, `OnSteepGround`, `NotSupported` or `InAir`, and `getTouchingBodies()` lists what the character bumped into. Characters also send contact events, and trigger zones hear characters walking in:

```cpp
void watchDoorway(RigidBodyComponent* doorwaySensor, CharacterControllerComponent* player) {
    doorwaySensor->setOnTriggerEnter([](const ContactEvent& e) {
        if (e.otherCharacter != nullptr) std::cout << "The player came in" << std::endl;
    });
    player->setOnContactBegin([](const ContactEvent& e) {
        // e.selfCharacter == player; e.other is the rigid body bumped (or nullptr
        // when e.otherCharacter is another character).
    });
}
``` Remember that pushing is force-limited: give crates a modest `mass` (the default density makes a 1 m cube weigh a tonne).

---

## 7. Seeing the Colliders

Turn on wireframes of every collider (static level chunks are cached, so this stays fast):

```cpp
APPLICATION->setPhysicsDebugDraw(true);
APPLICATION->getPhysicsDebugSettings().velocities = true;  // optional extras
```

---

## 8. Try It

`test/main.cpp` contains a playground:
- 80 boxes drop onto a slab;
- a trigger zone counts the boxes inside it;
- **F** pushes the box under the crosshair, and **R** drops the boxes again;
- **C** switches between flying and walking as a first-person character (WASD, **Space** jumps) that can shove the boxes and walk through the village;
- hold the **right mouse button** to look around (always on while walking);
- **G** toggles collider wireframes; `BIRD_PHYSICS_DEBUG=1` starts with them on.

The HUD shows the body count, the awake count and the last event.

Build and run it with:

```bash
./run_test.sh run
```
