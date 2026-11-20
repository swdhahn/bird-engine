# Tutorial 4: Shaders & Asset Baking

In Bird Engine, shaders are authored in Vulkan-compatible GLSL, validated and compiled offline by `BuildTools`, and loaded into OpenGL (or Vulkan) at runtime via `.bird` binary packages.

In this tutorial, you will learn:
1. The rules for authoring Bird Engine shaders.
2. Standard uniform blocks (`GlobalUBOData`, `ObjectPushConstants`).
3. How to use `#include` in shaders.
4. Baking shaders with `BuildTools`.
5. Loading shaders and binding them to materials at runtime.

---

## 1. Shader Authoring Rules

Bird shaders must follow modern Vulkan-style GLSL conventions:
- **Explicit In/Out Locations**: Every vertex attribute and inter-stage variable must have an explicit `layout(location = N)` qualifier.
- **Explicit Descriptor Bindings**: Uniform blocks and texture samplers must have explicit `layout(binding = N)` qualifiers.
- **Push Constants**: Up to 128 bytes can be declared in a `layout(push_constant) uniform PushConstants` block.
- **No Loose Uniforms**: Uniform variables must reside inside uniform blocks.

---

## 2. Vertex Shader Example (`assets/shaders/custom.vert`)

```glsl
#version 450

// Vertex attributes from the Mesh buffer
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Per-frame global camera matrices (std140, binding 0)
layout(binding = 0) uniform Global {
    mat4 perspective;
    mat4 view;
    vec4 viewPos;
} globals;

// Per-draw model matrix (push constant, std430)
layout(push_constant) uniform ObjectConstants {
    mat4 model;
    mat3 normalMatrix;  // inverse-transpose of mat3(model), computed on the CPU
} pc;

// Outputs to fragment shader
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = pc.normalMatrix * inNormal;  // correct under rotation and scale
    fragTexCoord = inTexCoord;
    gl_Position = globals.perspective * globals.view * worldPos;
}
```

---

## 3. Fragment Shader Example (`assets/shaders/custom.frag`)

```glsl
#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

// Engine-wide blocks live in shared includes; this one declares the
// metallic-roughness Material block (binding 1) and sampler slots 0..4.
#include "include/material.glsl"

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(0.5, 1.0, 0.3)); // Simple directional sun
    float NdotL = max(dot(N, L), 0.2);

    vec4 albedo = baseColor;
    if ((textureMask & TEX_BASE_COLOR) != 0u) {
        albedo *= texture(baseColorMap, fragTexCoord);
    }
    outColor = vec4(albedo.rgb * NdotL, albedo.a);
}
```

---

## 4. Baking with `BuildTools`

Run `BuildTools` from your project or engine root:
```bash
./build/BuildTools
```

### What BuildTools Does:
1. Invokes Google `shaderc` to compile GLSL into SPIR-V bytecodes.
2. Performs reflection on uniform blocks, push constant offsets, and sampler bindings.
3. Translates SPIR-V into GLSL 460, GLSL 410 and GLSL ES 300 using Khronos `SPIRV-Cross`.
4. Writes an optimized binary `assets/shaders/custom.vert.bird` containing all four representations.

---

## 5. Loading Shaders and Configuring Materials in C++

Engine passes select their own shaders: a material describes a surface,
and the renderer chooses the G-buffer or forward program from the
material's domain and blend mode (see
[Rendering Pipeline](../architecture/rendering_pipeline.md)). Custom
programs load through `ResourceManager::loadShader` for passes of your own:

```cpp
#include <renders/generic/Material.h>
#include <renders/generic/Shader.h>
#include <scene/components/MeshComponent.h>
#include <util/ResourceManager.h>

// 1. Load a custom shader program (cached per ordered stage list). Programs
//    with more stages pass a bird::ShaderBuilder to the same function.
auto customShader = resource_manager()->loadShader(
    "assets/shaders/custom.vert", "assets/shaders/custom.frag");

// 2. Create and configure a PBR material. Color textures load as sRGB.
auto wood = bird::Material::create();
wood->setBaseColor(bird::Vector4(1.0f, 0.8f, 0.8f, 1.0f))
    .setRoughness(0.7f)
    .setTexture(bird::TextureSlot::BaseColor,
                resource_manager()->loadColorTexture("assets/textures/wood.png"))
    .setTexture(bird::TextureSlot::Normal,
                resource_manager()->loadTexture("assets/textures/wood_n.png"));

// 3. Override the material on one object; the shared mesh stays untouched.
auto meshComponent = bird::_own_mk<bird::MeshComponent>("assets/models/cube.obj");
meshComponent->setAllMaterials(wood);
```

---

## Next Steps

Explore the architecture deep dives starting with **[Application Lifecycle & Main Loop](../architecture/lifecycle_and_main_loop.md)**.
