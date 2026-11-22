# Architecture: Frustum Culling

Bird Engine performs per-frame camera **frustum culling** in [`GraphicsPipeline::buildRenderQueue`](../../src/renders/GraphicsPipeline.cpp) via [`FrustumCuller`](../../src/renders/generic/FrustumCuller.h) before submitting draw calls to the **deferred G-buffer pass** or **forward pass** (including forward opaque meshes, transparent meshes, and CPU billboard particle emitters).

---

## 1. Plane Extraction (Gribb / Hartmann Method)

At the start of each frame, [`Frustum::fromViewProjection`](../../src/renders/generic/Bounds.h) extracts the 6 inward-facing frustum planes from the camera's combined $\mathbf{M} = \mathbf{P} \cdot \mathbf{V}$ matrix for OpenGL's $[-1, 1]$ clip-space depth convention:

| Plane | Enum (`FrustumPlane`) | Unnormalized Equation (Rows of $\mathbf{M}$) |
|---|---|---|
| **Left** | `FrustumPlane::Left` | $\mathbf{r}_4 + \mathbf{r}_1$ |
| **Right** | `FrustumPlane::Right` | $\mathbf{r}_4 - \mathbf{r}_1$ |
| **Bottom** | `FrustumPlane::Bottom` | $\mathbf{r}_4 + \mathbf{r}_2$ |
| **Top** | `FrustumPlane::Top` | $\mathbf{r}_4 - \mathbf{r}_2$ |
| **Near** | `FrustumPlane::Near` | $\mathbf{r}_4 + \mathbf{r}_3$ |
| **Far** | `FrustumPlane::Far` | $\mathbf{r}_4 - \mathbf{r}_3$ |

Each plane $\mathbf{p} = (n_x, n_y, n_z, d)$ is normalized by $\|\mathbf{n}\|$ so signed point distance is $\mathbf{n} \cdot \mathbf{x} + d$.

---

## 2. Local & World Bounding Volumes (`AABB` and `BoundingSphere`)

When a [`Mesh`](../../src/renders/generic/Mesh.h) is constructed (or when `Mesh::recomputeBounds()` is called after mutating vertex data), it computes and caches both:

- **Local AABB (`Mesh::getLocalBounds()`):** Tight axis-aligned box `AABB::fromPositions(positions, vertexCount)`.
- **Local Bounding Sphere (`Mesh::getLocalBoundingSphere()`):** Rotation-invariant sphere `BoundingSphere::fromPositions(positions, vertexCount)` centered at the AABB midpoint.

During `GraphicsPipeline::buildRenderQueue`:
1. Rigid meshes transform their local AABB into world space in $O(1)$ using **Arvo's method** (`AABB::transformed(model)`), avoiding 8-corner enumeration.
2. Skinned meshes with a `SkeletalAnimationComponent` compute animated joint-sphere bounds (`skinnedBounds`) before transforming to world space.
3. Active [`ParticleEmitter`](../../src/scene/components/ParticleEmitter.h) components maintain a dynamic world-space `AABB` (`ParticleEmitter::getWorldBounds()`) enclosing all live billboard quads.

---

## 3. Pass Integration & Profiling (`CullingStats`)

Every mesh sub-draw is classified into its target pass bucket (`RenderBucket::Deferred`, `RenderBucket::ForwardOpaque`, or `RenderBucket::Transparent`) via `classifyMaterial(*material)` and tested with `FrustumCuller::testMesh(worldBounds, bucket)`. Off-screen draws are rejected before entering `RenderQueue`.

> [!NOTE]
> Shadow-casting meshes (`material->castsShadows()`) are still gathered into `m_shadowCasters` before camera-frustum rejection because off-screen casters can cast shadows into the visible view frustum; the shadow pass performs its own per-cascade / per-light culling.

### Inspecting Culling Statistics at Runtime

Query [`GraphicsPipeline::getCullingStats()`](../../src/renders/GraphicsPipeline.h) to inspect culled vs. rendered counts for overlays, tests, or performance profiling:

```cpp
const bird::CullingStats& stats =
    bird::APPLICATION->getGraphicsPipeline()->getCullingStats();

// Aggregate mesh sub-draw counts:
uint32_t tested   = stats.totalTested;
uint32_t rendered = stats.rendered;
uint32_t culled   = stats.culled;
float ratio       = stats.cullRatio(); // [0.0, 1.0]

// Per-pass breakdown:
uint32_t defDrawn = stats.deferredRendered;
uint32_t defCull  = stats.deferredCulled;
uint32_t fwdDrawn = stats.forwardRendered(); // ForwardOpaque + Transparent
uint32_t fwdCull  = stats.forwardCulled();

// Particle emitter culling breakdown:
uint32_t vfxDrawn = stats.emittersRendered;
uint32_t vfxCull  = stats.emittersCulled;
```

To toggle frustum culling on or off at runtime for A/B profiling comparisons:

```cpp
bird::APPLICATION->getGraphicsPipeline()->setFrustumCullingEnabled(false);
```
