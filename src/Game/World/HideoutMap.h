#pragma once
#include "../../Graphics/WorldGeometry.h"
#include "../../Physics/CollisionWorld.h"
#include <cstdint>
namespace HideoutMap {
void Build(CollisionWorld& world, std::vector<WorldGeometry::Batch>& batches, int availableRoomCount = 2, std::uint32_t rescuedRoomMask = 0, int activeBedRoom = -1);
}
