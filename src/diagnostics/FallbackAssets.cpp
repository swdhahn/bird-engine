#include "FallbackAssets.h"

#include <algorithm>
#include <array>

namespace bird::diag {

FallbackTextureData generateCheckerboardTextureData(uint32_t size,
													uint32_t cellSize) {
	const uint32_t dim = std::max<uint32_t>(2, size);
	const uint32_t cell = std::max<uint32_t>(1, cellSize);

	FallbackTextureData out;
	out.width = dim;
	out.height = dim;
	out.channels = 4;
	out.pixels.resize(static_cast<size_t>(dim) * dim * 4);

	for (uint32_t y = 0; y < dim; ++y) {
		for (uint32_t x = 0; x < dim; ++x) {
			const bool isMagenta = (((x / cell) + (y / cell)) % 2) == 0;
			const size_t idx = (static_cast<size_t>(y) * dim + x) * 4;
			out.pixels[idx + 0] = isMagenta ? 0xFF : 0x00;
			out.pixels[idx + 1] = 0x00;
			out.pixels[idx + 2] = isMagenta ? 0xFF : 0x00;
			out.pixels[idx + 3] = 0xFF;
		}
	}
	return out;
}

FallbackMeshData generateUnitCubeMeshData() {
	struct FaceSpec {
		std::array<std::array<float, 3>, 4> corners;
		std::array<float, 3> normal;
	};

	// Six faces of [-0.5, 0.5]^3, each ordered CCW when viewed from outside.
	constexpr float h = 0.5f;
	const std::array<FaceSpec, 6> faces = {{
		// +Z (front)
		{{{{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}}}, {0.0f, 0.0f, 1.0f}},
		// -Z (back)
		{{{{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}},
		 {0.0f, 0.0f, -1.0f}},
		// +X (right)
		{{{{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}}}, {1.0f, 0.0f, 0.0f}},
		// -X (left)
		{{{{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}}},
		 {-1.0f, 0.0f, 0.0f}},
		// +Y (top)
		{{{{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}}}, {0.0f, 1.0f, 0.0f}},
		// -Y (bottom)
		{{{{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}}},
		 {0.0f, -1.0f, 0.0f}},
	}};

	constexpr std::array<std::array<float, 2>, 4> kFaceUVs = {{
		{0.0f, 0.0f},
		{1.0f, 0.0f},
		{1.0f, 1.0f},
		{0.0f, 1.0f},
	}};

	FallbackMeshData mesh;
	mesh.positions.reserve(24 * 3);
	mesh.normals.reserve(24 * 3);
	mesh.texCoords.reserve(24 * 2);
	mesh.indices.reserve(36);

	for (uint32_t f = 0; f < 6; ++f) {
		const uint32_t base = f * 4;
		for (uint32_t v = 0; v < 4; ++v) {
			const auto& p = faces[f].corners[v];
			const auto& n = faces[f].normal;
			const auto& uv = kFaceUVs[v];
			mesh.positions.insert(mesh.positions.end(), {p[0], p[1], p[2]});
			mesh.normals.insert(mesh.normals.end(), {n[0], n[1], n[2]});
			mesh.texCoords.insert(mesh.texCoords.end(), {uv[0], uv[1]});
		}
		mesh.indices.insert(mesh.indices.end(),
							{base, base + 1, base + 2, base, base + 2,
							 base + 3});
	}

	return mesh;
}

}  // namespace bird::diag
