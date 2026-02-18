#ifndef BIRD_SCREEN_H
#define BIRD_SCREEN_H

#include "../../util/Math.h"

namespace bird {

// extern Input* INPUT;
// extern ResourceManager* RESOURCE_MANAGER;

class ScreenObject {
   public:
	ScreenObject();
	virtual ~ScreenObject();

	virtual void init() = 0;
	virtual void deinit() = 0;

	const Vector2& getPosition() const;
	void setPosition(Vector2 worldPosition);
	const float& getRotation() const;
	void setRotation(float localRotation);
	const Matrix3& getTransformMatrix() const;
	const Vector2& getScale() const;
	void setScale(Vector2 scale);

	void setTransformMatrix(Matrix3 transformMatrix);

	void updateTransformationMatrix();
	bool needsMatrixUpdate() const;

   protected:
	Vector2 m_position;
	Vector2 m_scale;
	float m_rotation;
	Matrix3 m_transformMatrix;
	bool m_needsMatrixUpdate;
};

}  // namespace bird

#endif	// !BIRD_SCREEN_H
