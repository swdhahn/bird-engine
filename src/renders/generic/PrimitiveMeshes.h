#ifndef BIRD_PRIMITIVE_MESHES_H
#define BIRD_PRIMITIVE_MESHES_H

#include <cstdint>
#include <memory>
#include <vector>

namespace bird {

class Mesh;

/** Position-only triangle soup used by light volumes. */
struct PrimitiveGeometry {
	std::vector<float> positions;  // xyz triplets
	std::vector<uint32_t> indices;
};

/**
 * UV sphere circumscribing the unit sphere, so the faceted hull never cuts
 * inside the light's true radius.
 * @param segments Longitudinal divisions (>= 3).
 * @param rings Latitudinal divisions (>= 2).
 */
PrimitiveGeometry generateLightSphere(uint32_t segments, uint32_t rings);

/**
 * Cone with apex at the origin opening along -Z to a base of radius 1 at
 * z = -1, circumscribing the true circular cone.
 * @param segments Base divisions (>= 3).
 */
PrimitiveGeometry generateLightCone(uint32_t segments);

/** Uploads geometry as a position-only mesh. Requires a graphics context. */
std::shared_ptr<Mesh> buildPositionMesh(const PrimitiveGeometry& geometry);

}  // namespace bird

#endif	// BIRD_PRIMITIVE_MESHES_H
