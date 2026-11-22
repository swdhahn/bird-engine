#include "TransformGizmo.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bird::editor {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

const Vector4 kColorX{0.95f, 0.24f, 0.24f, 1.0f};
const Vector4 kColorY{0.25f, 0.90f, 0.35f, 1.0f};
const Vector4 kColorZ{0.24f, 0.55f, 0.98f, 1.0f};
const Vector4 kColorHighlight{1.00f, 0.92f, 0.20f, 1.0f};
const Vector4 kColorUniform{0.92f, 0.92f, 0.92f, 1.0f};

void drawWireCube(DebugDrawList& draw, const Vector3& center,
				  const Vector3& ax, const Vector3& ay, const Vector3& az,
				  float halfSize, const Vector4& color) {
	const Vector3 hx = ax * halfSize;
	const Vector3 hy = ay * halfSize;
	const Vector3 hz = az * halfSize;
	const Vector3 c[8] = {
		center - hx - hy - hz, center + hx - hy - hz, center + hx + hy - hz,
		center - hx + hy - hz, center - hx - hy + hz, center + hx - hy + hz,
		center + hx + hy + hz, center - hx + hy + hz};
	for (int i = 0; i < 4; ++i) {
		draw.overlayLine(c[i], c[(i + 1) % 4], color);
		draw.overlayLine(c[i + 4], c[(i + 1) % 4 + 4], color);
		draw.overlayLine(c[i], c[i + 4], color);
	}
}

void drawArrowHead(DebugDrawList& draw, const Vector3& tip,
				   const Vector3& dir, const Vector3& perp1,
				   const Vector3& perp2, float length, float radius,
				   const Vector4& color) {
	const Vector3 base = tip - dir * length;
	const Vector3 p[4] = {base + perp1 * radius, base + perp2 * radius,
						  base - perp1 * radius, base - perp2 * radius};
	for (int i = 0; i < 4; ++i) {
		draw.overlayLine(tip, p[i], color);
		draw.overlayLine(p[i], p[(i + 1) % 4], color);
	}
}

void drawCircle(DebugDrawList& draw, const Vector3& center, const Vector3& u,
				const Vector3& v, float radius, const Vector4& color,
				int segments = 36) {
	Vector3 prev = center + u * radius;
	for (int i = 1; i <= segments; ++i) {
		const float angle = (kTwoPi * static_cast<float>(i)) /
							static_cast<float>(segments);
		const Vector3 next =
			center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
		draw.overlayLine(prev, next, color);
		prev = next;
	}
}

}  // namespace

void TransformGizmo::setMode(GizmoMode mode) {
	if (m_mode == mode) return;
	cancelDrag();
	m_mode = mode;
	m_hoveredAxis = GizmoAxis::None;
}

void TransformGizmo::setSpace(GizmoSpace space) {
	if (m_space == space) return;
	cancelDrag();
	m_space = space;
}

Vector3 TransformGizmo::entityOrigin(const WorldObject& target) const {
	return target.getWorldPosition() + target.getLocalPosition();
}

float TransformGizmo::computeScale(const Vector3& origin,
								   const Camera& camera) const {
	const float dist = glm::length(camera.getWorldPosition() - origin);
	return std::max(kMinWorldScale, dist * kScreenSizeFactor);
}

void TransformGizmo::getAxes(const WorldObject& target, Vector3& axisX,
							 Vector3& axisY, Vector3& axisZ) const {
	if (m_space == GizmoSpace::Local || m_mode == GizmoMode::Scale) {
		// Scale always operates along the entity's local axes.
		const Quaternion rot =
			target.getWorldRotation() * target.getLocalRotation();
		axisX = glm::normalize(rot * Vector3(1.0f, 0.0f, 0.0f));
		axisY = glm::normalize(rot * Vector3(0.0f, 1.0f, 0.0f));
		axisZ = glm::normalize(rot * Vector3(0.0f, 0.0f, 1.0f));
	} else {
		axisX = Vector3(1.0f, 0.0f, 0.0f);
		axisY = Vector3(0.0f, 1.0f, 0.0f);
		axisZ = Vector3(0.0f, 0.0f, 1.0f);
	}
}

Vector3 TransformGizmo::axisDirection(GizmoAxis axis) const {
	switch (axis) {
		case GizmoAxis::X: return m_dragAxisX;
		case GizmoAxis::Y: return m_dragAxisY;
		case GizmoAxis::Z: return m_dragAxisZ;
		default: return Vector3(0.0f, 1.0f, 0.0f);
	}
}

Vector3 TransformGizmo::planeNormal(GizmoAxis planeAxis) const {
	switch (planeAxis) {
		case GizmoAxis::XY: return m_dragAxisZ;
		case GizmoAxis::XZ: return m_dragAxisY;
		case GizmoAxis::YZ: return m_dragAxisX;
		default: return Vector3(0.0f, 1.0f, 0.0f);
	}
}

bool TransformGizmo::intersectRayPlane(const Ray& ray,
									   const Vector3& planeOrigin,
									   const Vector3& planeNormal,
									   Vector3& outHit) {
	const float denom = glm::dot(ray.direction, planeNormal);
	if (std::abs(denom) < 1e-6f) return false;
	const float t = glm::dot(planeOrigin - ray.origin, planeNormal) / denom;
	if (t < 0.0f) return false;
	outHit = ray.at(t);
	return true;
}

float TransformGizmo::closestParamOnAxis(const Ray& ray,
										 const Vector3& axisOrigin,
										 const Vector3& axisDir) {
	// Closest point on line L(s) = axisOrigin + s * axisDir to ray R(t).
	const Vector3 w0 = axisOrigin - ray.origin;
	const float a = glm::dot(axisDir, axisDir);
	const float b = glm::dot(axisDir, ray.direction);
	const float c = glm::dot(ray.direction, ray.direction);
	const float d = glm::dot(axisDir, w0);
	const float e = glm::dot(ray.direction, w0);
	const float denom = a * c - b * b;
	if (std::abs(denom) < 1e-6f) return 0.0f;
	return (b * e - c * d) / denom;
}

float TransformGizmo::distanceRayToSegment(const Ray& ray, const Vector3& segA,
										   const Vector3& segB,
										   float& outRayT) {
	const Vector3 u = ray.direction;
	const Vector3 v = segB - segA;
	const Vector3 w = ray.origin - segA;
	const float a = glm::dot(u, u);
	const float b = glm::dot(u, v);
	const float c = glm::dot(v, v);
	const float d = glm::dot(u, w);
	const float e = glm::dot(v, w);
	const float denom = a * c - b * b;

	float sc = 0.0f;
	float tc = 0.0f;
	if (denom < 1e-6f) {
		tc = (c > 1e-6f) ? std::clamp(e / c, 0.0f, 1.0f) : 0.0f;
		sc = std::max(0.0f, b * tc - d);
	} else {
		sc = (b * e - c * d) / denom;
		tc = (a * e - b * d) / denom;
		if (tc < 0.0f) {
			tc = 0.0f;
			sc = std::max(0.0f, -d / a);
		} else if (tc > 1.0f) {
			tc = 1.0f;
			sc = std::max(0.0f, (b - d) / a);
		}
		if (sc < 0.0f) {
			sc = 0.0f;
			tc = (c > 1e-6f) ? std::clamp(e / c, 0.0f, 1.0f) : 0.0f;
		}
	}

	outRayT = sc;
	const Vector3 diff = (ray.origin + u * sc) - (segA + v * tc);
	return glm::length(diff);
}

GizmoAxis TransformGizmo::hitTest(const Ray& ray, const WorldObject& target,
								  const Camera& camera) const {
	const Vector3 origin = entityOrigin(target);
	const float s = computeScale(origin, camera);
	Vector3 ax, ay, az;
	getAxes(target, ax, ay, az);

	if (m_mode == GizmoMode::Translate || m_mode == GizmoMode::Scale) {
		// In Scale mode, check the center uniform-scale cube first.
		if (m_mode == GizmoMode::Scale) {
			const float half = s * 0.14f;
			AABB centerBox{origin - Vector3(half), origin + Vector3(half)};
			float hitDist = 0.0f;
			if (ViewportPicker::intersectRayAABB(ray, centerBox, hitDist)) {
				return GizmoAxis::Uniform;
			}
		}

		// In Translate mode, check planar drag squares (XY, XZ, YZ).
		if (m_mode == GizmoMode::Translate) {
			const float pMin = s * 0.22f;
			const float pMax = s * 0.44f;
			struct PlaneDef {
				GizmoAxis axis;
				Vector3 u;
				Vector3 v;
				Vector3 n;
			};
			const PlaneDef planes[3] = {
				{GizmoAxis::XY, ax, ay, az},
				{GizmoAxis::XZ, ax, az, ay},
				{GizmoAxis::YZ, ay, az, ax},
			};
			float bestPlaneDist = std::numeric_limits<float>::max();
			GizmoAxis bestPlane = GizmoAxis::None;
			for (const PlaneDef& p : planes) {
				Vector3 hit;
				if (!intersectRayPlane(ray, origin, p.n, hit)) continue;
				const Vector3 rel = hit - origin;
				const float cu = glm::dot(rel, p.u);
				const float cv = glm::dot(rel, p.v);
				if (cu >= pMin && cu <= pMax && cv >= pMin && cv <= pMax) {
					const float rayDist = glm::length(hit - ray.origin);
					if (rayDist < bestPlaneDist) {
						bestPlaneDist = rayDist;
						bestPlane = p.axis;
					}
				}
			}
			if (bestPlane != GizmoAxis::None) return bestPlane;
		}

		// Test X, Y, Z axis shafts.
		const float threshold = s * 0.12f;
		const std::pair<GizmoAxis, Vector3> axes[3] = {
			{GizmoAxis::X, ax}, {GizmoAxis::Y, ay}, {GizmoAxis::Z, az}};
		GizmoAxis bestAxis = GizmoAxis::None;
		float bestScore = std::numeric_limits<float>::max();
		for (const auto& [axis, dir] : axes) {
			float rayT = 0.0f;
			const float dist =
				distanceRayToSegment(ray, origin + dir * (s * 0.1f),
									 origin + dir * s, rayT);
			if (dist <= threshold && dist < bestScore) {
				bestScore = dist;
				bestAxis = axis;
			}
		}
		return bestAxis;
	}

	// Rotate mode: intersect each orthogonal circle plane and check radius band.
	const float ringTolerance = s * 0.14f;
	const std::pair<GizmoAxis, Vector3> rings[3] = {
		{GizmoAxis::X, ax}, {GizmoAxis::Y, ay}, {GizmoAxis::Z, az}};
	GizmoAxis bestRing = GizmoAxis::None;
	float bestRayDist = std::numeric_limits<float>::max();
	for (const auto& [axis, normal] : rings) {
		Vector3 hit;
		if (!intersectRayPlane(ray, origin, normal, hit)) continue;
		const float radius = glm::length(hit - origin);
		if (std::abs(radius - s) <= ringTolerance) {
			const float rayDist = glm::length(hit - ray.origin);
			if (rayDist < bestRayDist) {
				bestRayDist = rayDist;
				bestRing = axis;
			}
		}
	}
	return bestRing;
}

void TransformGizmo::updateHover(const Ray& ray, const WorldObject* target,
								 const Camera& camera) {
	if (isDragging()) return;
	if (!target) {
		m_hoveredAxis = GizmoAxis::None;
		return;
	}
	m_hoveredAxis = hitTest(ray, *target, camera);
}

bool TransformGizmo::beginDrag(const Ray& ray, WorldObject* target,
							   const Camera& camera) {
	if (!target) return false;
	const GizmoAxis hit = hitTest(ray, *target, camera);
	if (hit == GizmoAxis::None) return false;

	m_activeAxis = hit;
	m_hoveredAxis = hit;
	m_dragTarget = target;
	m_beforeSnapshot = TransformSnapshot::capture(*target);
	m_dragOrigin = entityOrigin(*target);
	m_dragScale = computeScale(m_dragOrigin, camera);
	getAxes(*target, m_dragAxisX, m_dragAxisY, m_dragAxisZ);

	if (m_mode == GizmoMode::Translate) {
		if (hit == GizmoAxis::X || hit == GizmoAxis::Y || hit == GizmoAxis::Z) {
			m_dragStartParam =
				closestParamOnAxis(ray, m_dragOrigin, axisDirection(hit));
		} else {
			intersectRayPlane(ray, m_dragOrigin, planeNormal(hit),
							  m_dragStartPlaneHit);
		}
	} else if (m_mode == GizmoMode::Rotate) {
		Vector3 hitPt;
		const Vector3 normal = axisDirection(hit);
		if (intersectRayPlane(ray, m_dragOrigin, normal, hitPt)) {
			const Vector3 v = hitPt - m_dragOrigin;
			m_dragStartRotVec =
				(glm::length(v) > 1e-5f) ? glm::normalize(v) : m_dragAxisX;
		}
	} else if (m_mode == GizmoMode::Scale) {
		if (hit == GizmoAxis::Uniform) {
			const Vector3 camFwd =
				glm::normalize(camera.getWorldRotation() *
							   Vector3(0.0f, 0.0f, -1.0f));
			intersectRayPlane(ray, m_dragOrigin, camFwd, m_dragStartPlaneHit);
		} else {
			m_dragStartParam =
				closestParamOnAxis(ray, m_dragOrigin, axisDirection(hit));
		}
	}
	return true;
}

void TransformGizmo::updateDrag(const Ray& ray, const Camera& camera) {
	if (!isDragging() || !m_dragTarget) return;

	if (m_mode == GizmoMode::Translate) {
		if (m_activeAxis == GizmoAxis::X || m_activeAxis == GizmoAxis::Y ||
			m_activeAxis == GizmoAxis::Z) {
			const Vector3 dir = axisDirection(m_activeAxis);
			const float param = closestParamOnAxis(ray, m_dragOrigin, dir);
			const float delta = param - m_dragStartParam;
			m_dragTarget->setWorldPosition(m_beforeSnapshot.worldPosition +
										   dir * delta);
		} else {
			Vector3 hitPt;
			if (intersectRayPlane(ray, m_dragOrigin, planeNormal(m_activeAxis),
								  hitPt)) {
				const Vector3 delta = hitPt - m_dragStartPlaneHit;
				m_dragTarget->setWorldPosition(m_beforeSnapshot.worldPosition +
											   delta);
			}
		}
	} else if (m_mode == GizmoMode::Rotate) {
		const Vector3 normal = axisDirection(m_activeAxis);
		Vector3 hitPt;
		if (intersectRayPlane(ray, m_dragOrigin, normal, hitPt)) {
			const Vector3 curr = hitPt - m_dragOrigin;
			const float len = glm::length(curr);
			if (len > 1e-5f) {
				const Vector3 currDir = curr / len;
				const float cosA =
					std::clamp(glm::dot(m_dragStartRotVec, currDir), -1.0f,
							   1.0f);
				const float sinA =
					glm::dot(glm::cross(m_dragStartRotVec, currDir), normal);
				const float angle = std::atan2(sinA, cosA);
				const Quaternion deltaQ = glm::angleAxis(angle, normal);
				m_dragTarget->setWorldRotation(
					glm::normalize(deltaQ * m_beforeSnapshot.worldRotation));
			}
		}
	} else if (m_mode == GizmoMode::Scale) {
		if (m_activeAxis == GizmoAxis::Uniform) {
			const Vector3 camFwd =
				glm::normalize(camera.getWorldRotation() *
							   Vector3(0.0f, 0.0f, -1.0f));
			const Vector3 camRight =
				glm::normalize(camera.getWorldRotation() *
							   Vector3(1.0f, 0.0f, 0.0f));
			Vector3 hitPt;
			if (intersectRayPlane(ray, m_dragOrigin, camFwd, hitPt)) {
				const float delta =
					glm::dot(hitPt - m_dragStartPlaneHit, camRight);
				const float factor =
					std::max(0.05f, 1.0f + delta / std::max(m_dragScale, 0.1f));
				m_dragTarget->setScale(m_beforeSnapshot.scale * factor);
			}
		} else {
			const Vector3 dir = axisDirection(m_activeAxis);
			const float param = closestParamOnAxis(ray, m_dragOrigin, dir);
			const float delta = param - m_dragStartParam;
			const float factor =
				std::max(0.05f, 1.0f + delta / std::max(m_dragScale, 0.1f));
			Vector3 newScale = m_beforeSnapshot.scale;
			if (m_activeAxis == GizmoAxis::X) newScale.x *= factor;
			if (m_activeAxis == GizmoAxis::Y) newScale.y *= factor;
			if (m_activeAxis == GizmoAxis::Z) newScale.z *= factor;
			m_dragTarget->setScale(newScale);
		}
	}
}

std::unique_ptr<TransformCommand> TransformGizmo::endDrag() {
	if (!isDragging() || !m_dragTarget) {
		m_activeAxis = GizmoAxis::None;
		m_dragTarget = nullptr;
		return nullptr;
	}

	WorldObject* target = m_dragTarget;
	const TransformSnapshot after = TransformSnapshot::capture(*target);
	const TransformSnapshot before = m_beforeSnapshot;

	std::string label = "Transform Entity";
	if (m_mode == GizmoMode::Translate) label = "Translate Entity";
	else if (m_mode == GizmoMode::Rotate) label = "Rotate Entity";
	else if (m_mode == GizmoMode::Scale) label = "Scale Entity";

	m_activeAxis = GizmoAxis::None;
	m_dragTarget = nullptr;

	if (before == after) return nullptr;
	return std::make_unique<TransformCommand>(target, before, after,
											  std::move(label));
}

void TransformGizmo::cancelDrag() {
	if (isDragging() && m_dragTarget) {
		m_beforeSnapshot.apply(*m_dragTarget);
	}
	m_activeAxis = GizmoAxis::None;
	m_dragTarget = nullptr;
}

void TransformGizmo::draw(DebugDrawList& debugDraw, const WorldObject* target,
						  const Camera& camera) const {
	if (!target) return;

	const Vector3 origin = entityOrigin(*target);
	const float s = computeScale(origin, camera);
	Vector3 ax, ay, az;
	getAxes(*target, ax, ay, az);

	const GizmoAxis lit =
		(m_activeAxis != GizmoAxis::None) ? m_activeAxis : m_hoveredAxis;

	const Vector4 colX = (lit == GizmoAxis::X) ? kColorHighlight : kColorX;
	const Vector4 colY = (lit == GizmoAxis::Y) ? kColorHighlight : kColorY;
	const Vector4 colZ = (lit == GizmoAxis::Z) ? kColorHighlight : kColorZ;

	if (m_mode == GizmoMode::Translate) {
		const float headLen = s * 0.20f;
		const float headRad = s * 0.06f;

		debugDraw.overlayLine(origin, origin + ax * s, colX);
		drawArrowHead(debugDraw, origin + ax * s, ax, ay, az, headLen, headRad,
					  colX);

		debugDraw.overlayLine(origin, origin + ay * s, colY);
		drawArrowHead(debugDraw, origin + ay * s, ay, az, ax, headLen, headRad,
					  colY);

		debugDraw.overlayLine(origin, origin + az * s, colZ);
		drawArrowHead(debugDraw, origin + az * s, az, ax, ay, headLen, headRad,
					  colZ);

		// Planar handle squares (XY, XZ, YZ).
		const float p0 = s * 0.22f;
		const float p1 = s * 0.44f;
		auto drawPlaneSquare = [&](const Vector3& u, const Vector3& v,
								   const Vector4& c) {
			const Vector3 a = origin + u * p0 + v * p0;
			const Vector3 b = origin + u * p1 + v * p0;
			const Vector3 cPt = origin + u * p1 + v * p1;
			const Vector3 d = origin + u * p0 + v * p1;
			debugDraw.overlayLine(a, b, c);
			debugDraw.overlayLine(b, cPt, c);
			debugDraw.overlayLine(cPt, d, c);
			debugDraw.overlayLine(d, a, c);
		};
		drawPlaneSquare(ax, ay,
						(lit == GizmoAxis::XY) ? kColorHighlight : kColorZ);
		drawPlaneSquare(ax, az,
						(lit == GizmoAxis::XZ) ? kColorHighlight : kColorY);
		drawPlaneSquare(ay, az,
						(lit == GizmoAxis::YZ) ? kColorHighlight : kColorX);
	} else if (m_mode == GizmoMode::Rotate) {
		drawCircle(debugDraw, origin, ay, az, s, colX);
		drawCircle(debugDraw, origin, az, ax, s, colY);
		drawCircle(debugDraw, origin, ax, ay, s, colZ);
	} else if (m_mode == GizmoMode::Scale) {
		const float tipHalf = s * 0.06f;
		debugDraw.overlayLine(origin, origin + ax * s, colX);
		drawWireCube(debugDraw, origin + ax * s, ax, ay, az, tipHalf, colX);

		debugDraw.overlayLine(origin, origin + ay * s, colY);
		drawWireCube(debugDraw, origin + ay * s, ax, ay, az, tipHalf, colY);

		debugDraw.overlayLine(origin, origin + az * s, colZ);
		drawWireCube(debugDraw, origin + az * s, ax, ay, az, tipHalf, colZ);

		const Vector4 colU =
			(lit == GizmoAxis::Uniform) ? kColorHighlight : kColorUniform;
		drawWireCube(debugDraw, origin, ax, ay, az, s * 0.10f, colU);
	}
}

}  // namespace bird::editor
