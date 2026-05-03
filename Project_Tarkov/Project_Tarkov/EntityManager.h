#pragma once
#include "pch.h"
#include "Entity.h"

class EntityManager
{
private:
    std::vector<Entity*> entities;

public:
    void Add(Entity* e)
    {
        entities.push_back(e);
    }

    void Update(float dt)
    {
        for (auto e : entities)
        {
            if (e->active)
                e->Update(dt);
        }
    }

    void Render()
    {
        for (auto e : entities)
        {
            if (e->active)
                e->Render();
        }
    }

    void Shutdown()
    {
        for (auto e : entities)
            delete e;

        entities.clear();
    }
};