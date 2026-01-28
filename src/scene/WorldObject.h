//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_WORLDOBJECT_H
#define BIRD_WORLDOBJECT_H

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <memory>

#include "../util/Input.h"
#include "../util/Math.h"
#include "../util/ResourceManager.h"
#include "components/Component.h"

namespace bird {

extern Input* INPUT;
extern ResourceManager* RESOURCE_MANAGER;
class Application;
extern Application* APPLICATION;
class Component;

class WorldObject {
   public:
	WorldObject();
	virtual ~WorldObject() {};

	virtual void init() = 0;
	virtual void deinit() = 0;
	void _deinit();

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

	void addComponent(std::unique_ptr<Component> component);
	void removeComponent(Component* component);

   protected:
	// Unique id for world objects
	uint64_t m_id;
	Vector3 m_worldPosition;
	Vector3 m_localPosition;
	Vector3 m_scale;
	Quaternion m_worldRotation;
	Quaternion m_localRotation;
	Matrix4 m_transformMatrix;

	std::vector<std::unique_ptr<Component>> m_components;

	/**
	 * Must make this variable true whenever any position or rotation is
	 * changed. This way, the rotation and position changes will occur during
	 * rendering and Matrix related calculations.
	 */
	bool m_needsMatrixUpdate;

   private:
	inline static uint64_t s_uniqueIdentificationIndex = 0;
};

}  // namespace bird

#endif	// BIRD_WORLDOBJECT_H
