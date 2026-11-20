#ifndef BIRD_BUILDTOOLS_SKELETALIMPORT_H
#define BIRD_BUILDTOOLS_SKELETALIMPORT_H

#include "../animation/skeletal/SkeletalAsset.h"

struct aiScene;

namespace bird {

/** True when any mesh in the scene carries bone weights. */
bool sceneHasSkinning(const aiScene& scene);

/**
 * Converts an Assimp scene into a SkeletalAsset, independent of the
 * source format (glTF, FBX, Collada, ...).
 *
 * The skeleton contains every bone node, every animated node, and all of
 * their ancestors, in depth-first (topological) order, so transforms on
 * container nodes (armature objects, unit-conversion roots) are kept.
 * Joints that no vertex references receive an identity inverse bind.
 *
 * Clip times are converted from ticks to seconds (ticksPerSecond of 0 is
 * treated as Assimp's documented default of 25). Keys use linear
 * interpolation; Assimp 5.3 does not report per-key interpolation.
 *
 * Skin streams keep each vertex's four strongest influences, renormalized
 * to sum to one; unweighted vertices bind fully to the root joint.
 *
 * @throws std::runtime_error when the scene exceeds Skeleton::kMaxJoints
 *         or a bone references a node that does not exist.
 */
SkeletalAsset extractSkeletalAsset(const aiScene& scene);

}  // namespace bird

#endif	// BIRD_BUILDTOOLS_SKELETALIMPORT_H
