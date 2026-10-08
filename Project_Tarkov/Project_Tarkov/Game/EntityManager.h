#pragma once
#include "Entity.h"
#include <memory>
#include <vector>

class EntityManager
{
public:
    // Ownership is transferred explicitly. The returned reference is valid until Shutdown.
    Entity& Add(std::unique_ptr<Entity> entity);
    void Update(float dt);
    void Render();
    void Shutdown();

private:
    std::vector<std::unique_ptr<Entity>> entities;
};
