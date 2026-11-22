#ifndef BIRD_ANIMATION_POSEOPS_H
#define BIRD_ANIMATION_POSEOPS_H

#include <vector>

#include "../../renders/generic/Bounds.h"
#include "../../util/Math.h"
#include "BoneMask.h"
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
 * Fast pose blend using LERP for translation/scale and shortest-path NLERP
 * (normalized linear interpolation, flipping hemisphere when dot < 0) for
 * rotation. O(joints).
 *
 * @param from Pose at weight 0.
 * @param to Pose at weight 1.
 * @param weight Blend factor (typically in [0, 1]).
 * @param out Result; may alias from or to.
 * @throws std::invalid_argument when the poses differ in size.
 */
void blendPosesNlerp(const Pose& from, const Pose& to, float weight, Pose& out);

/**
 * Blends overlay over base per joint using effective weight
 * `mask.getJointWeight(i) * weight`. If mask is empty, behaves like
 * blendPoses(base, overlay, weight, out). O(joints).
 *
 * @param base Underlying pose (kept where effective weight is 0).
 * @param overlay Layered pose (applied where effective weight is 1).
 * @param mask Per-joint weights in [0, 1], or empty for uniform 1.0.
 * @param weight Global layer weight in [0, 1].
 * @param out Result; may alias base or overlay.
 * @throws std::invalid_argument when pose or non-empty mask sizes differ.
 */
void blendPosesMasked(const Pose& base, const Pose& overlay,
					  const BoneMask& mask, float weight, Pose& out);

/**
 * Computes an N-way weighted blend of poses (used by 2D blend trees).
 * Weights are normalized internally to sum to 1. Translation and scale use
 * weighted linear sums; rotations use shortest-path hemisphere-aligned NLERP
 * relative to the dominant (first positive-weight) pose. O(poses * joints).
 *
 * @param poses Array of pose pointers (all must have the same joint count).
 * @param weights Matching non-negative blend weights (at least one > 0).
 * @param count Number of poses.
 * @param out Result buffer; must not alias any input pose.
 * @throws std::invalid_argument on empty input, size mismatch, or zero total weight.
 */
void blendPosesWeighted(const Pose* const* poses, const float* weights,
						size_t count, Pose& out);

/**
 * Computes an additive delta pose representing `target - reference`.
 *
 * Per joint:
 * - `delta.translation = target.translation - reference.translation`
 * - `delta.rotation = normalize(target.rotation * inverse(reference.rotation))`
 * - `delta.scale = target.scale / reference.scale` (1.0 when reference axis is 0)
 *
 * @param target Authored additive pose.
 * @param reference Base/reference pose (e.g. rest pose or first frame of clip).
 * @param out Resulting delta pose; may alias target or reference.
 * @throws std::invalid_argument when pose sizes differ.
 */
void makeAdditivePose(const Pose& target, const Pose& reference, Pose& out);

/**
 * Applies an additive delta pose on top of a base pose with uniform weight.
 *
 * Per joint (with effective weight w):
 * - `out.translation = base.translation + additive.translation * w`
 * - `out.rotation = normalize(slerp(identity, additive.rotation, w) * base.rotation)`
 * - `out.scale = base.scale * lerp(1, additive.scale, w)`
 *
 * @param base Base pose.
 * @param additive Delta pose produced by makeAdditivePose().
 * @param weight Additive strength (typically in [0, 1]).
 * @param out Result; may alias base or additive.
 * @throws std::invalid_argument when pose sizes differ.
 */
void applyAdditivePose(const Pose& base, const Pose& additive, float weight,
					   Pose& out);

/**
 * Applies an additive delta pose on top of a base pose with per-joint masking.
 * Effective weight per joint is `mask.getJointWeight(i) * weight` (or `weight`
 * when mask is empty).
 *
 * @param base Base pose.
 * @param additive Delta pose produced by makeAdditivePose().
 * @param mask Per-joint weights in [0, 1], or empty for uniform 1.0.
 * @param weight Global layer weight.
 * @param out Result; may alias base or additive.
 * @throws std::invalid_argument when pose or non-empty mask sizes differ.
 */
void applyAdditivePoseMasked(const Pose& base, const Pose& additive,
							 const BoneMask& mask, float weight, Pose& out);

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
