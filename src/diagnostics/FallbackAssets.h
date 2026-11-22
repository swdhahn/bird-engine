#ifndef BIRD_DIAGNOSTICS_FALLBACK_ASSETS_H
#define BIRD_DIAGNOSTICS_FALLBACK_ASSETS_H

#include <cstdint>
#include <memory>
#include <vector>

namespace bird {
class Material;
class Mesh;
class Texture;
}  // namespace bird

namespace bird::diag {

/**
 * CPU-side RGBA8 pixel buffer for a generated fallback texture.
 *
 * Kept independent of any graphics API so it can be verified in headless
 * unit tests without a GPU context.
 */
struct FallbackTextureData {
	/** Width of the generated image in pixels. */
	uint32_t width = 0;
	/** Height of the generated image in pixels. */
	uint32_t height = 0;
	/** Number of 8-bit channels per pixel (always 4 = RGBA). */
	uint32_t channels = 4;
	/** Tightly packed RGBA8 pixel bytes (`width * height * channels`). */
	std::vector<uint8_t> pixels;
};

/**
 * CPU-side vertex and index streams for a generated fallback 3D mesh.
 *
 * Kept independent of any graphics API so geometry invariants (vertex count,
 * normals, UVs, winding) can be verified in headless unit tests.
 */
struct FallbackMeshData {
	/** Interleaved XYZ positions (`vertexCount * 3` floats). */
	std::vector<float> positions;
	/** Interleaved XYZ outward unit normals (`vertexCount * 3` floats). */
	std::vector<float> normals;
	/** Interleaved UV texture coordinates (`vertexCount * 2` floats). */
	std::vector<float> texCoords;
	/** Counter-clockwise triangle indices (`triangleCount * 3` uint32s). */
	std::vector<uint32_t> indices;
};

/**
 * Generates a high-contrast magenta (`#FF00FF`) and black (`#000000`) RGBA8
 * checkerboard texture to make missing texture assets immediately visible.
 *
 * @param size     Width and height of the square texture in pixels (default 64,
 *                 clamped to >= 2).
 * @param cellSize Width and height of each checkerboard square in pixels
 *                 (default 8, clamped to >= 1).
 * @return Populated CPU-side RGBA8 pixel data.
 */
FallbackTextureData generateCheckerboardTextureData(uint32_t size = 64,
													uint32_t cellSize = 8);

/**
 * Generates a 1x1x1 unit cube centered at the origin (`[-0.5, 0.5]^3`) with
 * 24 vertices (4 per face for crisp flat normals), outward unit normals,
 * `[0, 1]` UV coordinates per face, and 36 CCW triangle indices.
 *
 * @return Populated CPU-side mesh geometry.
 */
FallbackMeshData generateUnitCubeMeshData();

/**
 * Uploads the magenta/black checkerboard fallback texture to the active
 * graphics backend. Requires an active graphics context.
 *
 * @param srgb When true, configures the GPU texture with sRGB color storage.
 * @return Shared pointer to the uploaded fallback GPU Texture.
 */
std::shared_ptr<bird::Texture> createFallbackTexture(bool srgb = false);

/**
 * Creates a fallback Material displaying the provided fallback texture
 * (or a solid magenta base color if `fallbackTexture` is null).
 *
 * @param fallbackTexture Optional checkerboard texture to bind to BaseColor.
 * @return Shared pointer to the fallback Material.
 */
std::shared_ptr<bird::Material> createFallbackMaterial(
	const std::shared_ptr<bird::Texture>& fallbackTexture);

/**
 * Uploads the fallback unit cube mesh to the active graphics backend and
 * attaches `fallbackMaterial`. Requires an active graphics context.
 *
 * @param fallbackMaterial Material attached to the generated mesh (if null,
 *                         a default magenta material is created).
 * @return Shared pointer to the uploaded fallback GPU Mesh.
 */
std::shared_ptr<bird::Mesh> createFallbackUnitCubeMesh(
	const std::shared_ptr<bird::Material>& fallbackMaterial);

}  // namespace bird::diag

#endif	// BIRD_DIAGNOSTICS_FALLBACK_ASSETS_H
