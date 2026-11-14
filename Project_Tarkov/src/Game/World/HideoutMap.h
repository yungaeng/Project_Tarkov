#pragma once
#include "../../Graphics/WorldGeometry.h"
#include "../../Physics/CollisionWorld.h"
#include <cstdint>
namespace HideoutMap {
void Build(CollisionWorld& world, std::vector<WorldGeometry::Batch>& batches, int availableRoomCount = 2, std::uint32_t rescuedRoomMask = 0, int activeBedRoom = -1);
inline float TradingStairwellX(int) { return 28.f; }
inline bool NearTradingStairwell(float x,float z,int availableRoomCount) {
    const float stairX=TradingStairwellX(availableRoomCount);
    return x>stairX-1.5f && x<stairX+.9f && z>.8f && z<2.8f;
}
}
