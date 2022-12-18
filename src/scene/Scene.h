//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_SCENE_H
#define BIRD_SCENE_H

#include <vector>
#include "WorldObject.h"
#include "Entity.h"

namespace bird {

    class OptimizationSet{};

    class Scene : public WorldObject {
    public:
        Scene();

        virtual void init() = 0;
        virtual void deinit() = 0;

        void addEntity(Entity* entity);
        void removeEntity(Entity* entity);

        void addChild(Scene* scene);
        void removeChild(Scene* scene);

        std::vector<Scene*> getChildren() const;
        std::vector<Entity*> getEntities() const;
    protected:
        Scene* m_parent = nullptr;
        std::vector<Scene*> m_children;
        std::vector<Entity*> m_entities;

    private:
        /**
         * This will be implemented in the future: But, it will let you define what specific type of
         * optimization you would want for this scene. i.e. terrain lod, Oct tree, portals and rooms, etc.
         */
        OptimizationSet* m_optimizationSet = nullptr;



    };

} // bird

#endif //BIRD_SCENE_H
