#ifndef BIRD_ANIMATION_POSEOPS_H
#define BIRD_ANIMATION_POSEOPS_H

#include <vector>

#include "../../util/Math.h"
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

}  // namespace bird

#endif	// BIRD_ANIMATION_POSEOPS_H
