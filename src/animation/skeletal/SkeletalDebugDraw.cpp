#include "SkeletalDebugDraw.h"

#include <cstddef>

namespace bird {

namespace {

void drawCross(DebugDrawList& draw, const Vector3& center, float halfSize,
			   const Vector4& color) {
	draw.line(center - Vector3(halfSize, 0.0f, 0.0f),
			  center + Vector3(halfSize, 0.0f, 0.0f), color);
	draw.line(center - Vector3(0.0f, halfSize, 0.0f),
			  center + Vector3(0.0f, halfSize, 0.0f), color);
	draw.line(center - Vector3(0.0f, 0.0f, halfSize),
			  center + Vector3(0.0f, 0.0f, halfSize), color);
}

}  // namespace

void drawSkeletonDebug(DebugDrawList& draw, const Skeleton& skeleton,
					   const std::vector<Matrix4>& modelMatrices,
					   const Matrix4& worldTransform,
					   const std::vector<TwoBoneIKConstraint>& ikConstraints,
					   const SkeletalDebugSettings& settings) {
	const int32_t count = skeleton.getJointCount();
	if (static_cast<int32_t>(modelMatrices.size()) != count) return;

	const std::vector<int32_t>& parents = skeleton.getParents();
	const Vector4 axisXColor(1.0f, 0.2f, 0.2f, 1.0f);
	const Vector4 axisYColor(0.2f, 1.0f, 0.2f, 1.0f);
	const Vector4 axisZColor(0.2f, 0.4f, 1.0f, 1.0f);

	for (int32_t i = 0; i < count; ++i) {
		const size_t idx = static_cast<size_t>(i);
		const Matrix4 jointWorld = worldTransform * modelMatrices[idx];
		const Vector3 jointPos(jointWorld[3]);

		if (settings.drawBones) {
			const int32_t parent = parents[idx];
			if (parent >= 0) {
				const Matrix4 parentWorld =
					worldTransform * modelMatrices[static_cast<size_t>(parent)];
				const Vector3 parentPos(parentWorld[3]);
				draw.line(parentPos, jointPos, settings.boneColor);
			}
		}

		if (settings.drawJointAxes && settings.jointAxisLength > 0.0f) {
			const float len = settings.jointAxisLength;
			const Vector3 xDir = glm::normalize(Vector3(jointWorld[0])) * len;
			const Vector3 yDir = glm::normalize(Vector3(jointWorld[1])) * len;
			const Vector3 zDir = glm::normalize(Vector3(jointWorld[2])) * len;
			draw.line(jointPos, jointPos + xDir, axisXColor);
			draw.line(jointPos, jointPos + yDir, axisYColor);
			draw.line(jointPos, jointPos + zDir, axisZColor);
		}
	}

	if (settings.drawIKTargets) {
		const float sz = settings.ikTargetSize;
		const Vector3 halfExtent(sz, sz, sz);
		for (const TwoBoneIKConstraint& ik : ikConstraints) {
			if (!ik.enabled) continue;
			const Vector3 targetWorld =
				Vector3(worldTransform * Vector4(ik.targetPosition, 1.0f));
			draw.box(targetWorld - halfExtent, targetWorld + halfExtent,
					 settings.ikTargetColor);
			drawCross(draw, targetWorld, sz * 1.5f, settings.ikTargetColor);

			if (ik.usePolePosition) {
				const Vector3 poleWorld =
					Vector3(worldTransform * Vector4(ik.polePosition, 1.0f));
				drawCross(draw, poleWorld, sz, settings.ikPoleColor);
				if (ik.midJoint >= 0 && ik.midJoint < count) {
					const Vector3 midWorld = Vector3(
						(worldTransform *
						 modelMatrices[static_cast<size_t>(ik.midJoint)])[3]);
					draw.line(midWorld, poleWorld, settings.ikPoleColor);
				}
			}
		}
	}
}

}  // namespace bird
