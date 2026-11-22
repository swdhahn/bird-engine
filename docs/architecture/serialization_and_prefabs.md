# Architecture: Scene Serialization, Prefabs & Save Games

Bird Engine provides a modular serialization and archetype subsystem in [`src/serialization/`](../../src/serialization/) that enables full scene persistence, reusable entity prefabs with non-destructive property overrides, and cross-platform save-game storage.

---

## 1. Architecture Overview

```mermaid
flowchart TD
    subgraph Scene & Prefab Layer
        SS["SceneSerializer"]
        PM["PrefabManager / InstantiatePrefab()"]
        PF["Prefab Asset (.prefab.json)"]
        PO["PropertyOverride Deltas"]
    end

    subgraph Reflection & Registry Layer
        CR["ComponentRegistry"]
        CD["ComponentDescriptor & FieldDescriptor"]
        DS["DartScriptRecord / DartScriptComponent"]
    end

    subgraph Format-Agnostic Archive Layer
        AR["Archive (Abstract Interface)"]
        JA["JsonWriteArchive / JsonReadArchive"]
        BA["Future BinaryArchive (.bird)"]
    end

    subgraph Storage Layer
        VFS["platform::VFS (Packaged Assets)"]
        SG["SaveGame (OS User Data & Config)"]
    end

    SS --> CR
    PM --> PF
    PM --> PO
    PM --> SS
    CR --> CD
    CR --> DS
    SS --> AR
    AR <|-- JA
    AR <|-- BA
    PM --> VFS
    SS --> SG
```

### Design Principles
- **Format-Agnostic Archive Interface (`bird::Archive`)**: Scene and component serialization logic is written once against `Archive`. Human-readable JSON (`JsonWriteArchive` / `JsonReadArchive`) is used for development and version control, while a binary archive backend can be added without modifying any component registration.
- **Stable 128-Bit Entity Identity (`bird::UUID` & `bird::EntityRef`)**: While `WorldObject::getID()` assigns sequential runtime counters, `WorldObject::getUUID()` provides a persistent RFC 4122 v4 UUID that survives scene saves and loads. Cross-entity component fields use `EntityRef`, which automatically remaps internal references during prefab instantiation.
- **Zero-Boilerplate Component Reflection (`bird::ComponentRegistry`)**: Any C++ component can register its fields (public members, getter/setter pairs, enums, or dynamic property maps) in a single fluent declaration. Registered fields automatically support scene save/load, prefab override application, and automatic override diffing.

---

## 2. Format-Agnostic Archive & JSON Schema

### The `Archive` Abstraction
`bird::Archive` ([`src/serialization/Archive.h`](../../src/serialization/Archive.h)) exposes a uniform bidirectional interface:

| Method | Description |
|---|---|
| `mode()` / `isWriting()` / `isReading()` | Queries whether the archive is serializing out or deserializing in. |
| `beginObject(name)` / `endObject()` | Opens and closes a named nested object scope. |
| `beginArray(name, count)` / `endArray()` | Opens and closes a named array scope (`count` is a size hint when writing and the element count when reading). |
| `beginArrayElement(index)` / `endArrayElement()` | Steps through elements of the active array scope. |
| `field(name, value)` | Reads or writes `bool`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `float`, `double`, `std::string`, `UUID`, `Vector2`, `Vector3`, `Vector4`, `Quaternion`, `Matrix4`, `EntityRef`, or `SerializedValue`. |
| `fieldEnum(name, enumValue)` | Convenience template serializing any `enum` or `enum class` via its underlying integer value. |

### Scene JSON Schema (`bird_scene`)
A serialized scene document has the following structure:

```json
{
  "format": "bird_scene",
  "schema_version": 1,
  "scene": {
    "uuid": "550e8400-e29b-41d4-a716-446655440000",
    "name": "MainLevel",
    "objectType": "Scene",
    "transform": {
      "worldPosition": [0.0, 0.0, 0.0],
      "localPosition": [0.0, 0.0, 0.0],
      "worldRotation": [0.0, 0.0, 0.0, 1.0],
      "localRotation": [0.0, 0.0, 0.0, 1.0],
      "scale": [1.0, 1.0, 1.0]
    },
    "components": [],
    "children": [
      {
        "uuid": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
        "name": "MainCamera",
        "objectType": "Camera",
        "transform": {
          "worldPosition": [0.0, 5.0, 12.0],
          "localPosition": [0.0, 0.0, 0.0],
          "worldRotation": [0.0, 0.247404, 0.0, 0.968912],
          "localRotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "camera": {
          "clearColor": [0.1, 0.2, 0.35],
          "ambientColor": [0.8, 0.7, 0.6],
          "ambientIntensity": 0.42,
          "exposure": 1.25,
          "debugView": 0
        },
        "components": []
      },
      {
        "uuid": "9b1deb4d-3b7d-4bad-9bdd-2b0d7b3dcb6d",
        "name": "Player",
        "objectType": "WorldObject",
        "transform": {
          "worldPosition": [2.0, 0.0, -3.0],
          "localPosition": [0.0, 0.0, 0.0],
          "worldRotation": [0.0, 0.0, 0.0, 1.0],
          "localRotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "components": [
          {
            "type": "MeshComponent",
            "data": {
              "assetPath": "assets/models/hero.glb"
            }
          },
          {
            "type": "LightComponent",
            "data": {
              "lightType": 1,
              "color": [1.0, 0.9, 0.7],
              "intensity": 2.5,
              "range": 15.0,
              "innerConeAngle": 0.35,
              "outerConeAngle": 0.5,
              "castsShadows": true,
              "shadowDepthBias": 1.0,
              "shadowNormalBias": 1.5
            }
          },
          {
            "type": "DartScript",
            "data": {
              "scriptClass": "PlayerController",
              "scriptPath": "scripts/player_controller.dart",
              "properties": [
                { "name": "moveSpeed", "value": 6.5 },
                {
                  "name": "cameraTarget",
                  "value": {
                    "$type": "entity_ref",
                    "v": "7c9e6679-7425-40de-944b-e07fc1f90ae7"
                  }
                }
              ]
            }
          }
        ]
      }
    ]
  }
}
```

> [!NOTE]
> Quaternions are serialized in JSON as `[x, y, z, w]` arrays and reconstructed via `glm::quat(w, x, y, z)`. Dynamic `SerializedValue` properties use `{ "$type": "<tag>", "v": ... }` wrappers for `vec2`, `vec3`, `vec4`, `quat`, `uuid`, and `entity_ref` so dynamic property bags and prefab overrides preserve exact C++ types across round-trips.

---

## 3. Registering Components for Serialization

All component reflection lives in `bird::ComponentRegistry` ([`src/serialization/ComponentRegistry.h`](../../src/serialization/ComponentRegistry.h)). Built-in components (`LightComponent`, `MeshComponent`, and `DartScript`) are registered automatically by `registerBuiltinComponents()`.

### Registering a Custom C++ Component

```cpp
#include "serialization/ComponentRegistry.h"
#include "serialization/UUID.h"

enum class TurretMode : int32_t { Passive = 0, Guard = 1, Overdrive = 2 };

class TurretComponent : public bird::Component {
   public:
    int32_t ammo = 120;
    float fireRate = 2.5f;
    bird::Vector3 muzzleOffset{0.0f, 1.2f, 0.8f};
    TurretMode mode = TurretMode::Guard;
    bird::EntityRef currentTarget;  // Automatically remapped inside prefabs!

    const std::string& getFaction() const { return m_faction; }
    void setFaction(const std::string& f) { m_faction = f; }

    void init() override {}
    void deinit() override {}

   private:
    std::string m_faction = "Alliance";
};

void registerGameComponents() {
    bird::ComponentRegistry::instance().registerComponent<TurretComponent>(
        "TurretComponent",
        [](bird::ComponentMetaBuilder<TurretComponent>& b) {
            // 1. Direct pointer-to-member registration
            b.field("ammo", &TurretComponent::ammo);
            b.field("fireRate", &TurretComponent::fireRate);
            b.field("muzzleOffset", &TurretComponent::muzzleOffset);
            b.field("currentTarget", &TurretComponent::currentTarget);

            // 2. Getter / setter member function registration
            b.field("faction", &TurretComponent::getFaction,
                    &TurretComponent::setFaction);

            // 3. Enum registration
            b.enumField<TurretMode>(
                "mode",
                [](const TurretComponent& c) { return c.mode; },
                [](TurretComponent& c, TurretMode m) { c.mode = m; });
        });
}
```

### Components with Non-Default Constructors
If a component requires constructor parameters from the archive (such as `LightComponent(LightType)`), supply `factoryFromArchive`:

```cpp
b.factoryFromArchive([](bird::Archive& ar) -> bird::_own<bird::Component> {
    int32_t rawType = 0;
    ar.field("lightType", rawType);
    return bird::_own_mk<bird::LightComponent>(static_cast<bird::LightType>(rawType));
});
```

### Dart Script Components & Exposed Properties
Dart scripts can be serialized in two interchangeable ways:
1. **In Dart (`BirdScript`)**: Override `scriptClassName`, `scriptPath`, and `exposedProperties` on your `BirdScript` subclass. When `createComponent()` is called, those metadata fields and properties are automatically forwarded to the native `DartScriptComponent`.
2. **In C++ / Tooling (`DartScriptRecord`)**: Attach a `bird::DartScriptRecord(scriptClass, scriptPath)` and call `setProperty(name, SerializedValue(...))`. To instantiate live Dart scripts during `SceneSerializer::deserializeScene()`, install a factory hook via `DartScriptRecord::setScriptFactory(...)`.

---

## 4. Prefab / Archetype System

A **Prefab** (`bird::Prefab`, [`src/serialization/Prefab.h`](../../src/serialization/Prefab.h)) is a reusable serialized entity subtree (`"format": "bird_prefab"`) where every entity in the subtree has a stable **local UUID**.

### Creating and Registering a Prefab

```cpp
#include "serialization/PrefabManager.h"

// Option A: Capture a live entity subtree in memory
bird::PrefabManager::get()->registerPrefabFromEntity("prefabs/turret", turretRoot);
bird::PrefabManager::get()->savePrefabToFile("prefabs/turret", "assets/prefabs/turret.prefab.json");

// Option B: Load a prefab asset from disk / VFS
bird::PrefabManager::get()->loadPrefabFromFile("prefabs/turret", "assets/prefabs/turret.prefab.json");
```

### Instantiating a Prefab (`InstantiatePrefab`)
Calling `bird::InstantiatePrefab(assetId, parentScene)` (or `PrefabManager::get()->instantiatePrefab(...)`, exposed to both C++ and Dart):
1. Resolves the `Prefab` by `AssetID` (auto-loading from VFS/disk if not yet cached).
2. Builds a 1-to-1 remap table mapping every **local UUID** in the prefab subtree to a freshly generated **runtime UUID**.
3. Deserializes the entity hierarchy and all attached components, rewriting every `EntityRef` (both C++ reflected `EntityRef` fields and Dart script `EntityRef` properties) that points to a local prefab UUID so it points to the newly spawned sibling/child entity. External `EntityRef` UUIDs pointing outside the prefab remain unchanged.
4. Attaches a `PrefabInstanceComponent` to each spawned node recording its source `prefab_asset_id` and `sourceLocalUuid`.
5. Applies any `PropertyOverride` deltas on top of the base prefab **before** `init()` is called.
6. Attaches the root entity to `parentScene` via `Scene::addChild()`, which triggers `_init()` and `init()` on all components and scripts in the subtree.

### Prefab Instance Overrides
When a scene contains an entity instantiated from a prefab, `SceneSerializer` does **not** duplicate the prefab's hierarchy or components in the scene file. Instead, it diffs the live instance against the base `Prefab` and writes a compact prefab instance entry containing only `prefab_asset_id`, the instance's root `uuid`, and the list of `overrides`:

```json
{
  "prefab_asset_id": "prefabs/goblin",
  "uuid": "d92f2e4a-8c1e-4b3b-9e45-1c2a3b4d5e6f",
  "overrides": [
    {
      "target_local_uuid": "00000000-0000-0000-0000-000000000000",
      "target_kind": "entity",
      "property": "name",
      "value": "GoblinBoss"
    },
    {
      "target_local_uuid": "00000000-0000-0000-0000-000000000000",
      "target_kind": "entity",
      "property": "worldPosition",
      "value": { "$type": "vec3", "v": [12.0, 0.0, -8.0] }
    },
    {
      "target_local_uuid": "00000000-0000-0000-0000-000000000000",
      "target_kind": "component",
      "component_type": "TestAgentComponent",
      "component_index": 0,
      "property": "health",
      "value": 500
    },
    {
      "target_local_uuid": "3f39a1c0-6a91-4422-81a0-9123456789ab",
      "target_kind": "entity",
      "property": "localPosition",
      "value": { "$type": "vec3", "v": [1.25, 2.5, 0.0] }
    }
  ]
}
```

- A nil `target_local_uuid` (`00000000-0000-0000-0000-000000000000`) targets the prefab's root entity; a non-nil UUID targets a specific child entity inside the prefab subtree.
- On scene load, `SceneSerializer` instantiates the base prefab first and applies each `PropertyOverride` before `init()` runs. Any non-overridden fields automatically inherit updates made to the base prefab asset.

---

## 5. Save Game Utility (`bird::SaveGame`)

`bird::SaveGame` ([`src/serialization/SaveGame.h`](../../src/serialization/SaveGame.h)) resolves OS-appropriate directories for player progress and configuration files safely outside the game's install directory:

| Platform | `SaveGame::getUserDataDirectory(appName)` | `SaveGame::getConfigDirectory(appName)` |
|---|---|---|
| **Windows** | `%APPDATA%\<appName>` (fallback: `%USERPROFILE%\AppData\Roaming\<appName>`) | `%APPDATA%\<appName>\config` |
| **Linux / POSIX** | `$XDG_DATA_HOME/<appName>` (fallback: `~/.local/share/<appName>`) | `$XDG_CONFIG_HOME/<appName>` (fallback: `~/.config/<appName>`) |
| **macOS** | `~/Library/Application Support/<appName>` | `~/Library/Application Support/<appName>/config` |

### Safety & Atomic Writes
- **Path Traversal Protection**: `SaveGame` rejects empty application names, absolute relative file paths, and any path containing `..` segments or null bytes with `std::invalid_argument`.
- **Atomic Save Commits**: `SaveGame::writeTextFile()` and `SaveGame::writeBinaryFile()` automatically create parent directories, write to a `<file>.tmp` staging file, flush the stream, and atomically rename the temporary file over the target file so interrupted writes never corrupt an existing save slot.

### Usage Example (C++ & Dart)

```cpp
// Saving a scene or player progress in C++
const std::string json = bird::SceneSerializer::serializeSceneToJson(activeScene);
bird::SaveGame::writeTextFile("MyBirdGame", "saves/slot1.scene.json", json);

// Loading it back
if (bird::SaveGame::fileExists("MyBirdGame", "saves/slot1.scene.json")) {
    const std::string savedJson =
        bird::SaveGame::readTextFile("MyBirdGame", "saves/slot1.scene.json");
    auto loadedScene = bird::SceneSerializer::deserializeSceneFromJson(savedJson);
}
```

```dart
// Spawning a prefab and writing a save file from Dart
final enemy = InstantiatePrefab('assets/prefabs/goblin.prefab.json', scene);
final saveJson = SceneSerializer.serializeSceneToJson(scene, 2);
SaveGame.writeTextFile('MyBirdGame', 'saves/slot1.json', saveJson);
```
