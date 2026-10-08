#include "EntityManager.h"
#include <stdexcept>
#include <utility>

Entity& EntityManager::Add(std::unique_ptr<Entity> entity)
{
    if (!entity) throw std::invalid_argument("Cannot add a null entity");
    entities.push_back(std::move(entity));
    return *entities.back();
}

void EntityManager::Update(float dt)
{
    for (const auto& entity : entities)
        if (entity->active) entity->Update(dt);
}

void EntityManager::Render()
{
    for (const auto& entity : entities)
        if (entity->active) entity->Render();
}

void EntityManager::Shutdown()
{
    entities.clear();
}
