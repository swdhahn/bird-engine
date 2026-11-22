#ifndef BIRD_ANIMATION_POSEOPS_H
#define BIRD_ANIMATION_POSEOPS_H

#include <vector>

#include "../../renders/generic/Bounds.h"
#include "../../util/Math.h"
#include "SkeletalAsset.h"
#include "Skeleton.h"

namespace bird {

/*
 * Pose pipeline, each stage a single linear pass over the joints:
 *
 *   sample clip(s) -> blendPoses -> localToModel -> buildSkinningPalette
 *
 * Every function writes into caller-owned buffers so a running animation
 * performs no per-frame allocation once its buffers are warm.
 */

/**
 * Blends two poses joint by joint: lerp for translation and scale,
 * shortest-arc slerp for rotation. O(joints).
 *
 * @param from Pose at weight 0.
 * @param to Pose at weight 1.
 * @param weight Blend factor (unclamped).
 * @param out Result; may alias from or to.
 * @throws std::invalid_argument when the poses differ in size.
 */
void blendPoses(const Pose& from, const Pose& to, float weight, Pose& out);

/**
 * Converts joint-local transforms into model-space matrices in one
 * forward pass (the skeleton is topologically ordered). O(joints).
 *
 * @throws std::invalid_argument when the pose does not match the skeleton.
 */
void localToModel(const Skeleton& skeleton, const Pose& local,
				  std::vector<Matrix4>& model);

/**
 * Builds the GPU skinning palette: model * inverseBind per joint, stored
 * as the affine matrix's three rows (glm::mat3x4 columns), 48 bytes per
 * joint. In GLSL: `vec3 p = vec4(position, 1.0) * palette[joint];`.
 *
 * @throws std::invalid_argument when the matrix count does not match.
 */
void buildSkinningPalette(const Skeleton& skeleton,
						  const std::vector<Matrix4>& model,
						  std::vector<Matrix3x4>& palette);

/** Packs an affine 4x4 matrix into the three-row palette format. */
Matrix3x4 toPaletteEntry(const Matrix4& affine);

/**
 * Per joint, the largest distance from the joint to any vertex it
 * influences, measured in the joint's bind space (inverseBind * vertex).
 * Joints influencing nothing get 0. Computed once per mesh at load.
 * O(vertices).
 *
 * @param positions Bind-pose positions, xyz per vertex.
 * @param vertexCount Number of vertices.
 * @throws std::invalid_argument when the skin and vertex counts differ or
 *         a joint index is outside the skeleton.
 */
std::vector<float> computeJointRadii(const Skeleton& skeleton,
									 const SkinStream& skin,
									 const float* positions,
									 size_t vertexCount);

/**
 * Conservative model-space bounds of a skinned mesh in its current pose:
 * the union of spheres around each influencing joint (radius scaled by the
 * joint's largest axis scale). Linear blend skinning keeps every vertex
 * inside the convex hull of its joints' rigidly moved copies, so the box
 * always contains the mesh. O(joints).
 *
 * @param jointModel Joint model matrices (SkeletalAnimator output).
 * @param jointRadii Radii from computeJointRadii for this mesh.
 * @return The bounds; empty when no joint has a radius.
 */
AABB skinnedBounds(const std::vector<Matrix4>& jointModel,
				   const std::vector<float>& jointRadii);

}  // namespace bird

#endif	// BIRD_ANIMATION_POSEOPS_H
