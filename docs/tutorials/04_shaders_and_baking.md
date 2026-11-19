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
} pc;

// Outputs to fragment shader
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(pc.model) * inNormal;
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

// Material uniform block (binding 1)
layout(binding = 1) uniform MaterialBlock {
    vec4 color;
    vec3 specularColor;
    vec3 ambientColor;
    float shininess;
} material;

// Diffuse texture sampler (binding 2)
layout(binding = 2) uniform sampler2D diffuseMap;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(0.5, 1.0, 0.3)); // Simple directional sun
    float NdotL = max(dot(N, L), 0.2);

    vec4 texColor = texture(diffuseMap, fragTexCoord);
    outColor = vec4(texColor.rgb * material.color.rgb * NdotL, material.color.a);
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
3. Translates SPIR-V into GLSL 460 and GLSL 410 using Khronos `SPIRV-Cross`.
4. Writes an optimized binary `assets/shaders/custom.vert.bird` containing all three representations.

---

## 5. Loading and Using Shaders in C++

Load your custom shader using `ShaderBuilder` and assign it to a `Material`:

```cpp
#include <renders/generic/Shader.h>
#include <renders/generic/Material.h>
#include <util/ResourceManager.h>

// 1. Build and load the shader pipeline
auto customShader = resource_manager()->loadShader(
    bird::ShaderBuilder()
        .attachShaderFile("assets/shaders/custom.vert", bird::VERTEX)
        .attachShaderFile("assets/shaders/custom.frag", bird::FRAGMENT)
);

// 2. Create and configure a material
auto mat = std::make_shared<bird::Material>();
mat->setShader(customShader);
mat->setDiffuseColor(bird::Vector3(1.0f, 0.8f, 0.8f));
mat->addTexture(resource_manager()->loadTexture("assets/textures/wood.png"));

// 3. Assign material to loaded mesh
auto meshes = resource_manager()->loadMesh("assets/models/cube.obj");
for (auto& mesh : meshes) {
    mesh->setMaterial(mat);
}
```

---

## Next Steps

Explore the architecture deep dives starting with **[Application Lifecycle & Main Loop](../architecture/lifecycle_and_main_loop.md)**.
