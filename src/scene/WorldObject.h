#if defined(_WIN32) || defined(_MSC_VER)
#ifdef BIRD_CORE_EXPORTS
#define BIRD_API __declspec(dllexport)
#else
#define BIRD_API __declspec(dllimport)
#endif
#define BIRD_API_EXPORT __declspec(dllexport)
#else
// For GCC/Linux, BIRD_API does nothing (default visibility)
#define BIRD_API
#define BIRD_API_EXPORT
#endif

#ifndef BIRD_WORLDOBJECT_H
#define BIRD_WORLDOBJECT_H

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../util/Input.h"
#include "../util/Math.h"
#include "../util/ResourceManager.h"
#include "Object.h"
#include "components/Component.h"

namespace bird {

extern BIRD_API Input* INPUT;
extern BIRD_API ResourceManager* RESOURCE_MANAGER;
class Application;
extern BIRD_API Application* APPLICATION;
class Component;
class Scene;

class WorldObject : public Object {
	BIRD_OBJECT(WORLDOBJECT)
   public:
	WorldObject(uint16_t type);
	WorldObject();
	virtual ~WorldObject() {};

	void init(Scene* parent);
	void deinit();

	const uint64_t& getID() const;
	bool operator==(const WorldObject& e);

	void translate(const Vector3& delta);
	void translateLocal(const Vector3& delta);
	void rotate(const Vector3& axis, const float& angle);
	void rotateLocal(const Vector3& axis, const float& angle);

	const Vector3& getWorldPosition() const;
	void setWorldPosition(Vector3 worldPosition);
	const Vector3& getLocalPosition() const;
	void setLocalPosition(Vector3 localPosition);
	const Quaternion& getWorldRotation() const;
	void setWorldRotation(Quaternion worldRotation);
	const Quaternion& getLocalRotation() const;
	void setLocalRotation(Quaternion localRotation);
	const Matrix4& getTransformMatrix() const;
	const Vector3& getScale() const;
	void setScale(Vector3 scale);

	void setTransformMatrix(Matrix4 transformMatrix);

	void updateTransformationMatrix();
	bool needsMatrixUpdate() const;

	std::vector<_own<Component>>& getComponents();
	void addComponent(_own<Component> component);
	void removeComponent(Component* component);

	Scene* getParent();

   protected:
	// Unique id for world objects
	uint64_t m_id;
	Vector3 m_worldPosition;
	Vector3 m_localPosition;
	Vector3 m_scale;
	Quaternion m_worldRotation;
	Quaternion m_localRotation;
	Matrix4 m_transformMatrix;

	Scene* m_parent;

	std::vector<_own<Component>> m_components;

	/**
	 * Must make this variable true whenever any position or rotation is
	 * changed. This way, the rotation and position changes will occur during
	 * rendering and Matrix related calculations.
	 */
	bool m_needsMatrixUpdate;
	bool m_isInitiated;

   private:
	inline static uint64_t s_uniqueIdentificationIndex = 0;
};

}  // namespace bird

#endif	// BIRD_WORLDOBJECT_H
