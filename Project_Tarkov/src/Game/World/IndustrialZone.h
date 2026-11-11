#pragma once
#include "../../Physics/CollisionWorld.h"
#include "../../Graphics/WorldGeometry.h"
#include "../Config/RaidConfig.h"
#include <array>
#include <vector>

// North is -Z. All positions use meters and the ground surface is Y=0.
namespace IndustrialZone
{
    using WorldGeometry::Block;
    using WorldGeometry::Shape;
    struct Exit { const char* name; glm::vec3 position; };
    const std::array<glm::vec3,2>& Spawns();
    const std::array<Exit,2>& Exits();
    const std::array<glm::vec3,5>& Buildings();
    const std::array<glm::vec3,5>& Enemies();

    void Build(CollisionWorld& world, std::vector<Block>& blocks);
}
