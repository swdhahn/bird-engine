# Architecture: Rendering Pipeline

Bird Engine renders through an abstract pipeline interface (`bird::GraphicsPipeline`); the OpenGL backend (`bird::gl::GLPipeline`) is the sole complete implementation. The renderer is **deferred by default**, with a **forward pass** for every surface that deferred shading cannot express. All features target OpenGL 4.1, so macOS remains supported: no compute shaders and no storage buffers are required.

---

## 1. Frame Overview

```
 GraphicsPipeline::buildRenderQueue      frustum culling, bucketing, sorting
 planShadows                             cascades + atlas tiles, CPU only
        |
        v
 1. Shadow pass      caster depth -> directional atlas (4 cascades)
                                  -> local atlas (spot: 1 tile, point: 6 tiles)
 2. G-buffer pass    opaque + masked lit surfaces -> RT0, RT1, HDR (emissive), depth
 3. Depth copy       G-buffer depth -> HDR target depth renderbuffer
 4. Lighting pass    fullscreen: ambient + directional lights (+ cascaded shadows)
                     volumes: point spheres, spot cones, additive into HDR
 5. Forward pass     unlit / force-forward opaque, then transparency back to front
 6. Debug lines      into HDR
 7. Post-process     exposure + ACES tone map (or G-buffer debug view) -> LDR,
                     then FXAA -> default framebuffer
 8. UI               orthographic overlay, untouched by post-processing
```

Each pass occupies its own class under `src/renders/opengl/passes/`; `GLPipeline::renderWorld` only orchestrates them. `GLRenderState` caches fixed-function state, and each pass declares the complete state upon which the pass depends.

---

## 2. Render Targets (`GLRenderTargets`)

| Target | Format | Contents |
|---|---|---|
| RT0 | `SRGB8_ALPHA8` | base color (rgb), ambient occlusion (a) |
| RT1 | `RGBA16F` | octahedral normal (xy), roughness (z), metallic (w) |
| HDR | `RGBA16F` | emissive written by the G-buffer pass; every light accumulates on top |
| Depth | `DEPTH24_STENCIL8` texture | world position is reconstructed from depth and the inverse matrices |
| LDR | `RGBA8` | tone-mapped, sRGB-encoded input to FXAA |

The HDR texture is attached both to the G-buffer framebuffer (as RT2) and to the HDR framebuffer, which owns a separate depth renderbuffer. After the G-buffer pass, depth is blitted into that renderbuffer. Light volumes and forward surfaces can therefore depth-test while the lighting shaders sample the G-buffer depth texture, avoiding any sampling/attachment feedback loop.

---

## 3. Materials (`bird::Material`)

Materials follow the glTF **metallic-roughness** model:

- **Factors:** base color (rgba), metallic, roughness, emissive color and strength, alpha cutoff, normal scale, occlusion strength.
- **Texture slots (`TextureSlot`):** `BaseColor`, `MetallicRoughness` (glTF packing: G roughness, B metal), `Normal`, `Occlusion`, `Emissive`. The slot value equals the sampler binding. Base color and emissive textures load as sRGB through `ResourceManager::loadColorTexture`; data maps load linear through `loadTexture`.
- **Routing (`classifyMaterial`):**

| Material | Bucket | Pass |
|---|---|---|
| `Lit` + `Opaque` / `Masked` | `Deferred` | G-buffer, sorted by material then front to back |
| `Unlit`, or `setForceForward(true)` | `ForwardOpaque` | forward, before transparency |
| `Transparent` / `Additive` | `Transparent` | forward, sorted back to front, no depth writes |

A material owns a small std140 uniform block, re-uploaded only after a setter changes a value. A shader variant per texture combination is avoided: a `textureMask` bitfield tells the shaders which slots hold textures. `MeshComponent::setMaterial` and `setAllMaterials` override materials per component, leaving the shared, cached `Mesh` untouched.

**Baking:** `AssetLoader` writes version 2 material files (`src/build-tools/MaterialFormat.h`) holding PBR factors, the glTF alpha mode, double-sidedness, and texture roles. Meshes gain a tangent stream (xyz plus handedness). Legacy unversioned Blinn-Phong files still load: diffuse becomes base color, shininess maps to roughness, and the first texture becomes the base color map. Models rebake automatically when the source is newer or the material files predate version 2.

---

## 4. Lights (`bird::LightComponent`)

| Type | Geometry | Deferred evaluation | Shadows |
|---|---|---|---|
| `Directional` | host -Z axis | fullscreen pass | 4 cascades, practical split scheme, bounding-sphere fit with texel snapping (no shimmer) |
| `Point` | host position, `range` | sphere volume | six faces in the local atlas; the shader selects the face by major axis |
| `Spot` | host -Z axis, inner/outer cone | cone volume | one perspective tile in the local atlas |

- **Radiance** equals `color * intensity`. Point and spot lights fall off with the inverse square of distance, windowed smoothly to zero at `range`.
- **Volumes** rasterize both faces with depth test `GREATER` and depth clamping, so only surfaces inside the volume are shaded, including when the camera stands inside the light.
- **Forward lighting** uses a uniform block of up to 32 lights: every directional light, then visible local lights nearest the camera first (`selectForwardLights`).
- **Shadow atlases** are single `DEPTH32F` 2D textures with hardware comparison (`sampler2DShadow`) and 3x3 PCF, clamped inside each tile. `ShadowAtlas` is a power-of-two quadtree allocator; local lights are prioritized by projected size, degrade to smaller tiles under pressure, and finally render unshadowed. Acne is controlled by slope-scaled polygon offset in the caster pass, plus per-light depth and normal-offset biases in texel units.
- **Ambient light** and **exposure** are configured per camera through `WorldEnvironment` (`ambientColor`, `ambientIntensity`, `exposure`).

---

## 5. Shader Interfaces (`src/renders/generic/GpuLayouts.h`)

Every block is mirrored by a C++ struct whose std140 offsets are pinned with `static_assert`.

| Binding | Block / sampler | Contents |
|---|---|---|
| UBO 0 | `Global` | projection, view, their inverses, camera position, screen size, ambient, exposure, near/far, debug view |
| UBO 1 | `Material` | PBR factors, texture mask, flags |
| UBO 2 | `Lights` | 32 `GpuLight` entries, counts |
| UBO 3 | `Shadows` | 64 shadow matrices, atlas rectangles, texel sizes, cascade splits |
| push | `ObjectConstants` / `LightVolume` / `ShadowConstants` | per-draw model data (at most 128 bytes) |
| samplers 0..4 | material slots | see `TextureSlot` |
| samplers 5..7 | G-buffer | albedo, normal, depth |
| samplers 8..9 | shadow atlases | directional, local |

Shared GLSL lives in `assets/shaders/include/` (`globals`, `material`, `pbr`, `lights`, `shadows`, `gbuffer`). The BRDF is Cook-Torrance with GGX distribution, height-correlated Smith visibility, and Schlick Fresnel. Vertex inputs use fixed locations for every mesh: position 0, normal 1, uv 2, tangent 3. Meshes without tangents fall back to a derivative-based tangent frame.

---

## 6. Debugging

- **G-buffer views:** set `WorldEnvironment::debugView` to 1 (albedo), 2 (normals), 3 (roughness/metallic), or 4 (linear depth). The demos cycle views with **V** and accept `BIRD_DEBUG_VIEW=n`.
- **Frame capture:** `BIRD_CAPTURE=/path/frame.ppm BIRD_CAPTURE_FRAME=120` writes the finished frame as a binary PPM.
- **Shader variant:** `BIRD_GL_SHADER_TARGET=410`, `460` or `es300` forces the GLSL variant a context uses.
- **Capability fallbacks:** `BIRD_GL_CAPS_DISABLE=depthClamp,srgbWriteControl,linearDepthFiltering` forces the fallback paths that OpenGL ES devices take, on any context.

---

## 7. OpenGL Profiles: Desktop GL, OpenGL ES and WebGL

One GL backend serves every OpenGL family. The CMake option `BIRD_GL_PROFILE` picks the API at build time:

| Profile | API | Function loading | Default for |
|---|---|---|---|
| `DESKTOP` | OpenGL 4.1+ core | GLEW | Linux, Windows, macOS |
| `ES` | OpenGL ES 3.0+ | Linked directly | Android, iOS, Emscripten (WebGL 2) |

`src/renders/opengl/GLApi.h` is the only header that includes GL declarations, and it selects the platform header for the profile. Optional features are detected at run time by `GLCapabilities`, so the passes branch on flags rather than `#ifdef`:

| Capability | Desktop | OpenGL ES | Fallback when absent |
|---|---|---|---|
| `depthClamp` | 3.2+ | `GL_EXT_depth_clamp` | Clamp requests are ignored; geometry past near/far clips |
| `srgbWriteControl` | Core | `GL_EXT_sRGB_write_control` | sRGB attachments always encode, so nothing to toggle |
| `debugOutput` | 4.3+ / `KHR_debug` | 3.2+ / `KHR_debug` | Not enabled |
| `halfFloatRenderTargets` | Core | 3.2+ / `EXT_color_buffer_(half_)float` | `GLRenderTargets` fails fast with a clear error |
| `linearDepthFiltering` | Core | Never | Depth textures without compare mode filter with `NEAREST` |

On ES and WebGL, 8-bit gray, gray-alpha and RGB textures upload as RGBA, since those APIs accept only exact internal-format pairings and WebGL has no swizzles. Uniforms are written with `glUniform*` on the bound program, which every API version supports.

To test the ES profile on a Linux desktop, configure with `-DBIRD_GL_PROFILE=ES` (requires the GLES and EGL development packages, e.g. `libgles-dev libegl-dev`); GLFW then opens an OpenGL ES 3.0 context through EGL.

---

## 8. Backend Status

| Feature | OpenGL (`GLPipeline`) | Vulkan | Metal | DirectX 12 |
|---|---|---|---|---|
| **Status** | Complete (desktop GL and OpenGL ES 3.0) | Not compiled (legacy experiment) | Stub | Stub |
| **Shading** | Deferred + forward, PBR, HDR | None | None | None |
| **Shadows** | Cascaded, spot, point (atlas) | None | None | None |
| **Shader source** | Baked `.bird` (GLSL 410/460, GLSL ES 300) | Baked SPIR-V (planned) | Planned | Planned |

---

## 9. Known Limitations and Next Steps

- Masked (alpha-tested) materials cast fully opaque shadows; the shadow pass does not sample textures.
- Ambient light is uniform; image-based lighting would replace `evaluateAmbient`.
- MSAA is unavailable with the deferred path; FXAA remains the anti-aliasing solution, and TAA is the natural successor.
- Light volumes draw one call per light; instancing becomes worthwhile beyond a few hundred lights.
- On OpenGL ES devices without `GL_EXT_depth_clamp`, shadow casters behind a light's near plane and light volumes past the far plane are clipped.
