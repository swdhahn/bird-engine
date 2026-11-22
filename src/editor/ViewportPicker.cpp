#include "ViewportPicker.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../renders/generic/Mesh.h"
#include "../scene/components/MeshComponent.h"

namespace bird::editor {

namespace {

void pickRecursive(const Ray& ray, Scene* scene,
				   const WorldObject* ignoreEntity, PickHit& bestHit) {
	if (!scene) return;
	for (auto& childPtr : scene->getChildren()) {
		WorldObject* child = childPtr.get();
		if (!child || child == ignoreEntity) continue;

		const AABB bounds = ViewportPicker::computeEntityBounds(child);
		float dist = 0.0f;
		if (ViewportPicker::intersectRayAABB(ray, bounds, dist)) {
			if (!bestHit.entity || dist < bestHit.distance) {
				bestHit.entity = child;
				bestHit.distance = dist;
				bestHit.point = ray.at(dist);
			}
		}

		if (child->getType() == SCENE) {
			pickRecursive(ray, static_cast<Scene*>(child), ignoreEntity,
						  bestHit);
		}
	}
}

}  // namespace

Ray ViewportPicker::screenToRay(Vector2 screenPos, Vector2 viewportSize,
								const Camera& camera) {
	Ray ray;
	if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
		ray.origin = camera.getWorldPosition();
		ray.direction = glm::normalize(camera.getWorldRotation() *
									   Vector3(0.0f, 0.0f, -1.0f));
		return ray;
	}

	const float ndcX = (2.0f * screenPos.x) / viewportSize.x - 1.0f;
	const float ndcY = 1.0f - (2.0f * screenPos.y) / viewportSize.y;

	const Matrix4 invVP =
		glm::inverse(camera.getPerspectiveMatrix() * camera.getViewMatrix());

	Vector4 nearH = invVP * Vector4(ndcX, ndcY, -1.0f, 1.0f);
	Vector4 farH = invVP * Vector4(ndcX, ndcY, 1.0f, 1.0f);

	if (std::abs(nearH.w) > 1e-6f) nearH /= nearH.w;
	if (std::abs(farH.w) > 1e-6f) farH /= farH.w;

	ray.origin = Vector3(nearH);
	const Vector3 dir = Vector3(farH) - Vector3(nearH);
	const float len = glm::length(dir);
	ray.direction = (len > 1e-6f) ? (dir / len) : Vector3(0.0f, 0.0f, -1.0f);
	return ray;
}

bool ViewportPicker::intersectRayAABB(const Ray& ray, const AABB& box,
									  float& outDistance) {
	if (box.isEmpty()) return false;

	float tmin = 0.0f;
	float tmax = std::numeric_limits<float>::max();

	for (int axis = 0; axis < 3; ++axis) {
		const float orig = ray.origin[axis];
		const float dir = ray.direction[axis];
		const float bmin = box.min[axis];
		const float bmax = box.max[axis];

		if (std::abs(dir) < 1e-8f) {
			if (orig < bmin || orig > bmax) return false;
		} else {
			const float invD = 1.0f / dir;
			float t0 = (bmin - orig) * invD;
			float t1 = (bmax - orig) * invD;
			if (t0 > t1) std::swap(t0, t1);
			tmin = std::max(tmin, t0);
			tmax = std::min(tmax, t1);
			if (tmin > tmax) return false;
		}
	}

	outDistance = tmin;
	return true;
}

AABB ViewportPicker::computeEntityBounds(WorldObject* entity,
										 float defaultHalfExtent) {
	AABB worldBox;
	if (!entity) return worldBox;

	const Matrix4 model = entity->needsMatrixUpdate()
							  ? createTransformMatrix(entity)
							  : entity->getTransformMatrix();

	for (auto& comp : entity->getComponents()) {
		if (auto* meshComp = dynamic_cast<MeshComponent*>(comp.get())) {
			for (const auto& mesh : meshComp->getMeshes()) {
				if (!mesh) continue;
				const AABB& local = mesh->getLocalBounds();
				if (local.isEmpty()) continue;
				const AABB transformed = local.transformed(model);
				worldBox.expand(transformed.min);
				worldBox.expand(transformed.max);
			}
		}
	}

	if (worldBox.isEmpty()) {
		const Vector3 center =
			entity->getWorldPosition() + entity->getLocalPosition();
		const Vector3 ext(defaultHalfExtent);
		worldBox.min = center - ext;
		worldBox.max = center + ext;
	}
	return worldBox;
}

PickHit ViewportPicker::pick(const Ray& ray, Scene* rootScene,
							 const WorldObject* ignoreEntity) {
	PickHit bestHit;
	pickRecursive(ray, rootScene, ignoreEntity, bestHit);
	return bestHit;
}

}  // namespace bird::editor
