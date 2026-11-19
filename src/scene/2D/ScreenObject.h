#ifndef BIRD_SCREEN_H
#define BIRD_SCREEN_H

#include "../../util/Math.h"

namespace bird {

/**
 * 2D spatial screen entity base class.
 *
 * Encapsulates 2D position, rotation, scale, and a cached 3x3 affine
 * transformation matrix with dirty-flag optimization (m_needsMatrixUpdate).
 */
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

	/**
	 * Recomputes the 3x3 2D affine transformation matrix and clears dirty flag.
	 */
	void updateTransformationMatrix();

	/**
	 * Returns true if position, rotation, or scale changed since the last update.
	 */
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
