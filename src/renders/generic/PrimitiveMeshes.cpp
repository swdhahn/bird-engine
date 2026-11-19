#include "PrimitiveMeshes.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

#include "Mesh.h"

namespace bird {

namespace {
constexpr float kPi = 3.14159265358979f;
}  // namespace

PrimitiveGeometry generateLightSphere(uint32_t segments, uint32_t rings) {
	if (segments < 3 || rings < 2) {
		throw std::invalid_argument("generateLightSphere: too few divisions.");
	}
	// Inflate so face centers, the closest hull points, reach radius 1.
	const float inflate =
		1.0f / (std::cos(kPi / segments) * std::cos(kPi / (2.0f * rings)));
	PrimitiveGeometry g;
	for (uint32_t r = 0; r <= rings; r++) {
		const float phi = kPi * r / rings;
		for (uint32_t s = 0; s <= segments; s++) {
			const float theta = 2.0f * kPi * s / segments;
			g.positions.push_back(inflate * std::sin(phi) * std::cos(theta));
			g.positions.push_back(inflate * std::cos(phi));
			g.positions.push_back(inflate * std::sin(phi) * std::sin(theta));
		}
	}
	const uint32_t stride = segments + 1;
	for (uint32_t r = 0; r < rings; r++) {
		for (uint32_t s = 0; s < segments; s++) {
			const uint32_t a = r * stride + s;
			const uint32_t b = a + stride;
			g.indices.insert(g.indices.end(), {a, a + 1, b, b, a + 1, b + 1});
		}
	}
	return g;
}

PrimitiveGeometry generateLightCone(uint32_t segments) {
	if (segments < 3) {
		throw std::invalid_argument("generateLightCone: too few divisions.");
	}
	const float inflate = 1.0f / std::cos(kPi / segments);
	PrimitiveGeometry g;
	g.positions.insert(g.positions.end(), {0.0f, 0.0f, 0.0f});	  // apex 0
	g.positions.insert(g.positions.end(), {0.0f, 0.0f, -1.0f});	  // base 1
	for (uint32_t s = 0; s < segments; s++) {
		const float theta = 2.0f * kPi * s / segments;
		g.positions.push_back(inflate * std::cos(theta));
		g.positions.push_back(inflate * std::sin(theta));
		g.positions.push_back(-1.0f);
	}
	for (uint32_t s = 0; s < segments; s++) {
		const uint32_t current = 2 + s;
		const uint32_t next = 2 + (s + 1) % segments;
		g.indices.insert(g.indices.end(), {0, current, next});
		g.indices.insert(g.indices.end(), {1, next, current});
	}
	return g;
}

std::shared_ptr<Mesh> buildPositionMesh(const PrimitiveGeometry& geometry) {
	auto positions = std::make_unique<float[]>(geometry.positions.size());
	std::memcpy(positions.get(), geometry.positions.data(),
				geometry.positions.size() * sizeof(float));
	auto indices = std::make_unique<uint32_t[]>(geometry.indices.size());
	std::memcpy(indices.get(), geometry.indices.data(),
				geometry.indices.size() * sizeof(uint32_t));
	return MeshBuilder()
		.setVertexBuffer(
			BufferBuilder<float>(BUFFER_STAGED | BUFFER_ARRAY)
				.data(std::move(positions), geometry.positions.size())
				.build())
		.setIndexBuffer(
			BufferBuilder<uint32_t>(BUFFER_STAGED | BUFFER_ELEMENT_ARRAY)
				.data(std::move(indices), geometry.indices.size())
				.build())
		.build();
}

}  // namespace bird
