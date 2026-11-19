#include "ShadowMath.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace bird::shadow {

namespace {

/** Returns an up vector never parallel to the given forward direction. */
Vector3 stableUp(const Vector3& forward) {
	return std::abs(forward.y) > 0.99f ? Vector3(0.0f, 0.0f, 1.0f)
									   : Vector3(0.0f, 1.0f, 0.0f);
}

}  // namespace

std::vector<float> computeCascadeSplits(float nearPlane, float farPlane,
										uint32_t cascadeCount, float lambda) {
	if (!(nearPlane > 0.0f) || !(farPlane > nearPlane) || cascadeCount == 0) {
		throw std::invalid_argument("computeCascadeSplits: invalid range.");
	}
	lambda = std::clamp(lambda, 0.0f, 1.0f);
	std::vector<float> splits(cascadeCount + 1);
	splits.front() = nearPlane;
	for (uint32_t i = 1; i < cascadeCount; i++) {
		const float p = static_cast<float>(i) / cascadeCount;
		const float logSplit = nearPlane * std::pow(farPlane / nearPlane, p);
		const float uniformSplit = nearPlane + (farPlane - nearPlane) * p;
		splits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
	}
	splits.back() = farPlane;
	return splits;
}

CascadeProjection fitCascade(const Matrix4& inverseView, float fovY,
							 float aspect, float sliceNear, float sliceFar,
							 const Vector3& lightDirection, uint32_t resolution,
							 float casterPullback) {
	// Bounding sphere of the slice in view space. The sphere is invariant
	// under camera rotation, so the projection size never changes, which is
	// the precondition for texel snapping to remove shimmer.
	const float tanY = std::tan(fovY * 0.5f);
	const float tanX = tanY * aspect;
	const float diagonalSq = tanX * tanX + tanY * tanY;
	// Sphere center lies on the view axis at distance z minimizing radius.
	float centerDistance =
		0.5f * (sliceNear + sliceFar) * (1.0f + diagonalSq);
	centerDistance = std::min(centerDistance, sliceFar);
	const float farCornerSq =
		sliceFar * sliceFar * diagonalSq +
		(sliceFar - centerDistance) * (sliceFar - centerDistance);
	const float nearCornerSq =
		sliceNear * sliceNear * diagonalSq +
		(centerDistance - sliceNear) * (centerDistance - sliceNear);
	float radius = std::sqrt(std::max(farCornerSq, nearCornerSq));
	// Quantize so floating noise cannot alter the projection scale.
	radius = std::ceil(radius * 16.0f) / 16.0f;

	const Vector3 center =
		Vector3(inverseView * Vector4(0.0f, 0.0f, -centerDistance, 1.0f));

	const Vector3 direction = glm::normalize(lightDirection);
	const Vector3 eye = center - direction * (radius + casterPullback);
	const Matrix4 view = glm::lookAt(eye, center, stableUp(direction));
	const float depthRange = 2.0f * radius + casterPullback;
	Matrix4 projection =
		glm::ortho(-radius, radius, -radius, radius, 0.0f, depthRange);

	// Snap the projected world origin to the texel grid.
	const Matrix4 viewProjection = projection * view;
	const float halfResolution = static_cast<float>(resolution) * 0.5f;
	const Vector4 origin = viewProjection * Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	const Vector2 texelOrigin = Vector2(origin) * halfResolution;
	const Vector2 rounded = glm::round(texelOrigin);
	const Vector2 offset = (rounded - texelOrigin) / halfResolution;
	projection[3][0] += offset.x;
	projection[3][1] += offset.y;

	return {projection * view, (2.0f * radius) / static_cast<float>(resolution)};
}

float guardedFieldOfView(float baseFov, uint32_t tileSize, float guardTexels) {
	const float scale =
		1.0f + 2.0f * guardTexels / std::max(1.0f, static_cast<float>(tileSize));
	return 2.0f * std::atan(std::tan(baseFov * 0.5f) * scale);
}

float localShadowNearPlane(float range) {
	return std::max(0.05f, range * 0.005f);
}

Matrix4 spotViewProjection(const Vector3& position, const Vector3& direction,
						   float outerAngle, float range, uint32_t tileSize) {
	const Vector3 forward = glm::normalize(direction);
	const float fov =
		std::min(guardedFieldOfView(2.0f * outerAngle, tileSize, 2.0f), 3.1f);
	const Matrix4 projection =
		glm::perspective(fov, 1.0f, localShadowNearPlane(range), range);
	return projection *
		   glm::lookAt(position, position + forward, stableUp(forward));
}

std::array<Matrix4, CUBE_FACE_COUNT> pointFaceViewProjections(
	const Vector3& position, float range, uint32_t tileSize) {
	static const Vector3 forwards[CUBE_FACE_COUNT] = {
		{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
	static const Vector3 ups[CUBE_FACE_COUNT] = {
		{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
	const float fov =
		guardedFieldOfView(glm::radians(90.0f), tileSize, 2.0f);
	const Matrix4 projection =
		glm::perspective(fov, 1.0f, localShadowNearPlane(range), range);
	std::array<Matrix4, CUBE_FACE_COUNT> result;
	for (uint32_t face = 0; face < CUBE_FACE_COUNT; face++) {
		result[face] = projection * glm::lookAt(position,
												position + forwards[face],
												ups[face]);
	}
	return result;
}

uint32_t selectCubeFace(const Vector3& v) {
	const Vector3 a = glm::abs(v);
	if (a.x >= a.y && a.x >= a.z) return v.x >= 0.0f ? 0u : 1u;
	if (a.y >= a.z) return v.y >= 0.0f ? 2u : 3u;
	return v.z >= 0.0f ? 4u : 5u;
}

}  // namespace bird::shadow
