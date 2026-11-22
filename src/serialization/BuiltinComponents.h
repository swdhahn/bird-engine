#ifndef BIRD_SERIALIZATION_BUILTIN_COMPONENTS_H
#define BIRD_SERIALIZATION_BUILTIN_COMPONENTS_H

namespace bird {

/**
 * Registers Bird Engine's built-in components with ComponentRegistry:
 * - `LightComponent` (type, color, intensity, range, cone angles, shadows)
 * - `MeshComponent` (assetPath)
 * - `DartScript` (`DartScriptRecord` and, when enabled, `DartScriptComponent`)
 *
 * Idempotent: safe to call multiple times. Automatically invoked by
 * SceneSerializer and PrefabManager on first use.
 */
void registerBuiltinComponents();

}  // namespace bird

#endif	// BIRD_SERIALIZATION_BUILTIN_COMPONENTS_H
