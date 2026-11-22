# Tutorial 7: 3D NavMesh & AI Pathfinding

In this tutorial, you will:
1. Bake a 3D `NavMesh` from static scene geometry using **Recast** (configuring agent radius, height, max slope, and step height).
2. Save the baked `NavMesh` to a binary `.bnav` file and load it at runtime without rebaking.
3. Query point-to-point paths (`findPath`) and nearest walkable points (`findNearestPoint`) with **Detour**.
4. Attach `NavAgent` components to move NPCs smoothly around obstacles in both **C++** and **Dart**.
5. Visualize the NavMesh polygons and active agent paths with the 3D debug renderer.

For an architectural overview of `NavServer`, `NavWorld`, `NavMesh`, and `NavAgent`, see [Navigation & AI Pathfinding](../architecture/navigation.md).

---

## 1. Baking and Saving a NavMesh in C++

Configure `NavMeshBuildSettings` to match your character dimensions, then bake directly from your `Scene` (or from a list of `Mesh` resources) and save the binary to disk:

```cpp
#include <navigation/NavAgent.h>
#include <navigation/NavMesh.h>
#include <navigation/NavTypes.h>
#include <navigation/NavWorld.h>
#include <scene/Scene.h>
#include <scene/components/MeshComponent.h>

using namespace bird;

_ref<NavMesh> loadOrBakeLevelNavMesh(Scene* scene) {
    const std::string navPath = "assets/models/maps/level01.bnav";

    // 1. Try loading a pre-baked binary NavMesh first (fast runtime path).
    if (auto loaded = NavMesh::loadFromFile(navPath)) {
        return loaded;
    }

    // 2. Otherwise, configure Recast settings and bake from static scene geometry.
    NavMeshBuildSettings settings;
    settings.cellSize = 0.25f;       // XZ voxel size (m)
    settings.cellHeight = 0.15f;     // Y voxel size (m)
    settings.agentRadius = 0.5f;     // Erode walkable area 0.5 m from walls
    settings.agentHeight = 1.8f;     // 1.8 m minimum ceiling clearance
    settings.agentMaxClimb = 0.4f;   // Climb stairs/steps up to 0.4 m
    settings.agentMaxSlope = 45.0f;  // Walkable slopes up to 45 degrees

    auto navMesh = NavMesh::buildFromScene(scene, settings);

    // Cache the baked binary so subsequent launches load instantaneously.
    navMesh->saveToFile(navPath);
    return navMesh;
}
```

Assign the `NavMesh` to your scene's `NavWorld`:

```cpp
auto navMesh = loadOrBakeLevelNavMesh(scene);
scene->getNavWorld()->setNavMesh(navMesh);
```

---

## 2. Point-to-Point Path Queries (`findPath` & `findNearestPoint`)

You can query paths and snap arbitrary world coordinates onto the `NavMesh` without spawning an agent:

```cpp
NavWorld* navWorld = scene->getNavWorld();

// Snap an arbitrary position onto the nearest walkable polygon:
if (std::optional<NavPoint> snapped = navWorld->findNearestPoint({12.0f, 5.0f, -8.0f})) {
    Vector3 onGround = snapped->position;
}

// Compute a funnel-smoothed 3D path from start to goal:
NavPath path = navWorld->findPath({-10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
if (path.isValid()) {
    for (size_t i = 0; i < path.getPointCount(); ++i) {
        Vector3 waypoint = path.getPoint(i);
        // Use waypoint...
    }
}
```

---

## 3. Steering Entities with `NavAgent` in C++

Attach a `NavAgent` component to any `WorldObject`. Each frame, `NavServer` steps the scene's Detour Crowd simulation and updates the entity's world position and yaw orientation:

```cpp
class PatrolScript : public ProcessScript<WorldObject> {
   public:
    PatrolScript(NavAgent* agent, std::vector<Vector3> waypoints)
        : m_agent(agent), m_waypoints(std::move(waypoints)) {}

    void init() override {
        if (!m_waypoints.empty()) {
            m_agent->setDestination(m_waypoints[0]);
        }
    }

    void deinit() override {}

    void process(float delta) override {
        if (m_waypoints.empty()) return;

        // Advance to the next patrol waypoint upon arrival.
        if (m_agent->hasReachedDestination()) {
            m_currentIndex = (m_currentIndex + 1) % m_waypoints.size();
            m_agent->setDestination(m_waypoints[m_currentIndex]);
        }
    }

   private:
    NavAgent* m_agent = nullptr;
    std::vector<Vector3> m_waypoints;
    size_t m_currentIndex = 0;
};

void spawnPatrolGuard(Scene* scene) {
    NavAgentSettings agentCfg;
    agentCfg.radius = 0.5f;
    agentCfg.height = 1.8f;
    agentCfg.maxSpeed = 4.0f;
    agentCfg.maxAcceleration = 10.0f;
    agentCfg.stoppingDistance = 0.35f;
    agentCfg.updateRotation = true;

    auto guard = _own_mk<WorldObject>();
    guard->setWorldPosition({-8.0f, 0.0f, -8.0f});
    guard->addComponent(_own_mk<MeshComponent>("assets/models/Character_Skeleton_Knight.gltf"));

    auto navComp = _own_mk<NavAgent>(agentCfg);
    NavAgent* agentPtr = navComp.get();
    guard->addComponent(std::move(navComp));

    guard->addComponent(_own_mk<PatrolScript>(
        agentPtr, std::vector<Vector3>{{-8, 0, -8}, {8, 0, -8}, {8, 0, 8}, {-8, 0, 8}}));

    scene->addChild(std::move(guard));
}
```

---

## 4. Enabling 3D Debug Visualization

Toggle NavMesh polygon wireframes and active agent paths on `Application`:

```cpp
APPLICATION->setNavDebugDraw(true);

NavDebugSettings& dbg = APPLICATION->getNavDebugSettings();
dbg.drawNavMesh = true;      // Cached cyan/blue NavMesh polygon wireframe
dbg.drawAgentPaths = true;   // Yellow path corridor + destination marker
dbg.drawAgentBounds = true;  // Green agent cylinder + velocity vector
```

---

## 5. Using NavMesh & `NavAgent` from Dart Scripts

All navigation classes (`NavMesh`, `NavMeshBuildSettings`, `NavWorld`, `NavAgent`, `NavAgentSettings`, `NavPath`, `NavPoint`, and `NavDebugSettings`) are automatically exposed to Dart through Bird Engine's FFI binding generator.

### Loading or Baking a NavMesh in Dart

```dart
import 'package:bird_engine/bird_engine.dart';

void setupNavigation(Scene scene) {
  final navWorld = scene.getNavWorld()!;

  // Load a pre-baked binary NavMesh from disk, or bake from the scene:
  var navMesh = NavMesh.loadFromFile('assets/nav/arena.bnav');
  if (navMesh == null) {
    final buildSettings = NavMeshBuildSettings()
      ..cellSize = 0.25
      ..cellHeight = 0.15
      ..agentRadius = 0.5
      ..agentHeight = 1.8
      ..agentMaxClimb = 0.4
      ..agentMaxSlope = 45.0;
    navMesh = NavMesh.buildFromScene(scene, buildSettings);
    buildSettings.dispose();
  }

  navWorld.setNavMesh(navMesh);

  // Enable 3D debug rendering of the NavMesh and agent paths:
  application!.setNavDebugDraw(true);
}
```

### Querying Paths & Controlling a `NavAgent` in Dart

```dart
class GuardAiScript extends BirdScript<WorldObject> {
  final NavAgent agent;
  final List<Vector3> patrolPoints;
  int _index = 0;

  GuardAiScript(this.agent, this.patrolPoints);

  @override
  void init() {
    if (patrolPoints.isNotEmpty) {
      agent.setDestination(patrolPoints[_index]);
    }
  }

  @override
  void process(double delta) {
    // Emergency stop when holding Space
    if (input!.isKeyDown(KEY_SPACE)) {
      if (agent.hasDestination()) {
        agent.stop();
      }
      return;
    }

    // Advance to the next waypoint once the destination is reached
    if (agent.hasReachedDestination() && patrolPoints.isNotEmpty) {
      _index = (_index + 1) % patrolPoints.length;
      agent.setDestination(patrolPoints[_index]);
    }
  }
}

void spawnDartGuard(Scene scene) {
  // Optional: query a path directly on NavWorld
  final navWorld = scene.getNavWorld()!;
  final path = navWorld.findPath(
    Vector3(-10.0, 0.0, 0.0),
    Vector3(10.0, 0.0, 0.0),
    Vector3(2.0, 4.0, 2.0),
  );
  if (path.isValid()) {
    print('Computed path with ${path.getPointCount()} waypoints (${path.getLength()} m)');
  }
  path.dispose();

  // Create an entity with a NavAgent component
  final npc = WorldObject();
  npc.setWorldPosition(Vector3(-10.0, 0.0, 0.0));

  final settings = NavAgentSettings()
    ..radius = 0.5
    ..height = 1.8
    ..maxSpeed = 4.5
    ..maxAcceleration = 10.0
    ..stoppingDistance = 0.35;

  final agent = NavAgent(settings);
  npc.addComponent(agent);

  final ai = GuardAiScript(agent, [
    Vector3(-10.0, 0.0, 0.0),
    Vector3(10.0, 0.0, 0.0),
    Vector3(10.0, 0.0, 10.0),
    Vector3(-10.0, 0.0, 10.0),
  ]);
  npc.addComponent(ai.createComponent());

  scene.addChild(npc);
  settings.dispose();
}
```
