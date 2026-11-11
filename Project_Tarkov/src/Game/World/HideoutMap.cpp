#include "../Hideout/Hideout.h"
#include "../../Graphics/WorldGeometry.h"
#include "IndustrialDetails.h"
#include <algorithm>

#include "HideoutMap.h"
#include "HideoutLayout.h"
namespace HideoutMap {
void Build(CollisionWorld& world, std::vector<WorldGeometry::Batch>& batches,
    int availableRoomCount, std::uint32_t rescuedRoomMask, int activeBedRoom) {
    availableRoomCount=std::clamp(availableRoomCount,2,4);
    if (rescuedRoomMask==0 && availableRoomCount>Hideout::InitialRoomCount)
        rescuedRoomMask=(1u<<(availableRoomCount-Hideout::InitialRoomCount))-1u;
    for (int room=Hideout::InitialRoomCount;room<Hideout::HideoutRoomCount;++room)
        if (rescuedRoomMask&(1u<<(room-Hideout::InitialRoomCount)))
            availableRoomCount=(std::max)(availableRoomCount,room+1);
    world.Clear();
    std::vector<WorldGeometry::Block> blocks;
    // The terrace overlooks the raid map below; scenery has no hideout collision.
    CollisionWorld sceneryWorld;
    IndustrialZone::Build(sceneryWorld, blocks);
    IndustrialZone::AddDetails(sceneryWorld, blocks);
    for (auto& block : blocks) block.center += glm::vec3(0,-16,270);
    HideoutLayout::AppendShell(rescuedRoomMask,world,blocks);
    for(int room=0;room<availableRoomCount;++room) {
        if(room>=Hideout::InitialRoomCount && !(rescuedRoomMask&(1u<<(room-Hideout::InitialRoomCount)))) continue;
        HideoutLayout::AppendFurniture(room,room!=activeBedRoom,world,blocks);
    }
    WorldGeometry::Bake(blocks,batches);
}
}
