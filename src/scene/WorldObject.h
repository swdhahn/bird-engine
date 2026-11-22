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

#include "../serialization/UUID.h"
#include "../util/Constants.h"
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
class AnimationSystem;
/** Engine-wide animation system for world animation (null when headless). */
extern BIRD_API AnimationSystem* ANIMATION_SYSTEM;
class EventBus;
/** Engine-wide decoupled event bus shared by native systems and Dart scripts. */
extern BIRD_API EventBus* EVENT_BUS;
class Component;
class Scene;

/**
 * 3D spatial entity in the scene graph.
 *
 * Encapsulates position, rotation (quaternion), and scale in both world and
 * local coordinate spaces. Manages a cached 4x4 model matrix with dirty-flag
 * optimization (m_needsMatrixUpdate). Hosts a collection of owned Component
 * instances and delegates lifecycle events (init/deinit) to them.
 */
class WorldObject : public Object {
	BIRD_OBJECT(WORLDOBJECT)
   public:
	BIRD_NO_EXPORT WorldObject(uint16_t type);
	WorldObject();
	/** Cancels any world animations still targeting this object. */
	virtual ~WorldObject();

	/**
	 * Associates this entity with a parent Scene and initializes its components.
	 *
	 * Calls _init() and init() on all currently attached components.
	 * @param parent Scene containing this object (or nullptr if root).
	 */
	virtual void init(Scene* parent);

	/**
	 * Deinitializes components and breaks association with the parent Scene.
	 */
	virtual void deinit();

	/** True between init() and deinit(). */
	bool isInitiated() const;

	const uint64_t& getID() const;
	bool operator==(const WorldObject& e);

	/** Returns the stable 128-bit UUID identifying this entity across saves. */
	BIRD_NO_EXPORT const UUID& getUUID() const;

	/** Overrides the entity's stable UUID (used during deserialization and prefab remapping). */
	BIRD_NO_EXPORT void setUUID(const UUID& uuid);

	/** Returns the canonical hyphenated UUID string for this entity. */
	std::string getUUIDString() const;

	/** Sets the entity's UUID from a canonical hyphenated UUID string. */
	void setUUIDString(const std::string& uuid);

	/** Returns the human-readable name assigned to this entity (may be empty). */
	const std::string& getName() const;

	/** Assigns a human-readable name to this entity. */
	void setName(const std::string& name);

	/**
	 * Translates the object along world axes and marks the transform dirty.
	 * @param delta Translation vector in world coordinates.
	 */
	void translate(const Vector3& delta);

	/**
	 * Translates the object relative to its current orientation and marks dirty.
	 * @param delta Translation vector in local object coordinates.
	 */
	void translateLocal(const Vector3& delta);

	/**
	 * Rotates the object around a world-space axis and marks the transform dirty.
	 * @param axis World axis to rotate around.
	 * @param angle Rotation angle in radians.
	 */
	void rotate(const Vector3& axis, const float& angle);

	/**
	 * Rotates the object around a local axis and marks the transform dirty.
	 * @param axis Local object axis to rotate around.
	 * @param angle Rotation angle in radians.
	 */
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

	/**
	 * Recomputes the 4x4 transformation matrix from translation, rotation,
	 * and scale, and clears the dirty flag (m_needsMatrixUpdate = false).
	 */
	void updateTransformationMatrix();

	/**
	 * Returns true if position, rotation, or scale changed since the last update.
	 */
	bool needsMatrixUpdate() const;

	/**
	 * Counter incremented by every position, rotation or scale change.
	 * Systems mirroring the transform (such as physics bodies) compare it
	 * against the value they last synchronized to detect user edits.
	 */
	uint64_t getTransformRevision() const;

	std::vector<_own<Component>>& getComponents();

	/**
	 * Attaches a component, transferring ownership to this WorldObject.
	 *
	 * If this object is already initialized, the component's _init() and
	 * init() hooks are invoked immediately.
	 */
	void addComponent(_own<Component> component);

	/**
	 * Deinitializes and removes an attached component.
	 */
	void removeComponent(Component* component);

	Scene* getParent();

   protected:
	// Unique identifier assigned sequentially upon construction
	uint64_t m_id;
	// Stable persistent identifier used by scene serialization and prefabs
	UUID m_uuid;
	// Optional human-readable entity name
	std::string m_name;
	Vector3 m_worldPosition;
	Vector3 m_localPosition;
	Vector3 m_scale;
	Quaternion m_worldRotation;
	Quaternion m_localRotation;
	Matrix4 m_transformMatrix;

	Scene* m_parent;

	std::vector<_own<Component>> m_components;

	/**
	 * True whenever position, rotation, or scale changes. Checked during
	 * Application::processScene() before submitting draw calls.
	 */
	bool m_needsMatrixUpdate;
	bool m_isInitiated;
	uint64_t m_transformRevision = 0;

	/** Marks the matrix dirty and bumps the transform revision. */
	void markTransformChanged();

   private:
	inline static uint64_t s_uniqueIdentificationIndex = 0;
};

}  // namespace bird

#endif	// BIRD_WORLDOBJECT_H
