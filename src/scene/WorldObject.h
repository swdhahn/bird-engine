//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_WORLDOBJECT_H
#define BIRD_WORLDOBJECT_H

#include "../util/Math.h"

namespace bird {

    class WorldObject {
    public:
        WorldObject();
        virtual ~WorldObject() {};

        virtual void init() = 0;
        virtual void deinit() = 0;

        const uint64_t& getID() const;
        bool operator==(const WorldObject& e);

        void move(const Vector3& delta);

        const Vector3 &getWorldPosition() const;
        void setWorldPosition(const Vector3 &worldPosition);
        const Vector3 &getLocalPosition() const;
        void setLocalPosition(const Vector3 &localPosition);
        const Quaternion &getWorldRotation() const;
        void setWorldRotation(const Quaternion &worldRotation);
        const Quaternion &getLocalRotation() const;
        void setLocalRotation(const Quaternion &localRotation);
        const Matrix4 &getTransformMatrix() const;
        const Vector3 &getScale() const;
        void setScale(const Vector3 &scale);

        void setTransformMatrix(const Matrix4 &transformMatrix);

        void updateTransformationMatrix();
        bool needsMatrixUpdate() const;
    protected:
        // Unique id for world objects
        uint64_t m_id;
        Vector3 m_worldPosition;
        Vector3 m_localPosition;
        Vector3 m_scale;
        Quaternion m_worldRotation;
        Quaternion m_localRotation;
        Matrix4 m_transformMatrix;

        /**
         * Must make this variable true whenever any position or rotation is changed.
         * This way, the rotation and position changes will occur during rendering and Matrix
         * related calculations.
         */
        bool m_needsMatrixUpdate;

    private:
        inline static uint64_t s_uniqueIdentificationIndex = 0;
    };

} // bird

#endif //BIRD_WORLDOBJECT_H
