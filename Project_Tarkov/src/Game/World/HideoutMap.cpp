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
    const auto shellStart=blocks.size();
    HideoutLayout::AppendShell(rescuedRoomMask,world,blocks);
    const auto shellEnd=blocks.size();
    // Surround the playable apartment with repeated residential floors.
    // These copies are scenery and do not add inaccessible room interactions.
    for(int floor:{-3,-2,-1,1,2}) {
        for(auto i=shellStart;i<shellEnd;++i) {
            auto block=blocks[i];
            block.center.y+=floor*3.2f;
            blocks.push_back(block);
        }
    }
    for(int room=0;room<availableRoomCount;++room) {
        if(room>=Hideout::InitialRoomCount && !(rescuedRoomMask&(1u<<(room-Hideout::InitialRoomCount)))) continue;
        HideoutLayout::AppendFurniture(room,room!=activeBedRoom,world,blocks);
    }
    // Exposed concrete switchback staircase beside the building's right wall.
    // The corridor entrance stays fixed beside room four.
    const glm::vec3 concrete(.48f,.47f,.41f),metal(.24f,.28f,.26f);
    auto box=[&](glm::vec3 center,glm::vec3 size,glm::vec3 color,float tilt=0.f) {
        WorldGeometry::Block block;
        block.center=center;block.size=size;block.color=color;block.rotation.x=tilt;
        blocks.push_back(block);
    };
    // Preserve the original flight locations; widen treads and their rails only.
    constexpr float flightWidth=1.62f;
    auto landing=[&](float z,float y,bool farEnd) {
        box({28.85f,y-.15f,z},{3.4f,.3f,1.2f},concrete);
        // Only the far exposed edge is fenced; flight mouths stay open.
        if(farEnd) {
            for(int post=0;post<=10;++post)
                box({27.2f+post*.33f,y+.5f,z-.58f},{.045f,1.f,.045f},metal);
            box({28.85f,y+1.02f,z-.58f},{3.4f,.055f,.055f},metal);
        }
        for(float x:{27.2f,30.5f}) {
            box({x,y+1.02f,z},{.055f,.055f,1.2f},metal);
            for(int post=0;post<=3;++post)
                box({x,y+.5f,z-.58f+post*.386f},{.045f,1.f,.045f},metal);
        }
    };
    constexpr int steps=12;
    constexpr float flight=4.2f,halfFloor=1.6f;
    for(int floor=-3;floor<=1;++floor) {
        const float y=floor*3.2f;
        landing(-5.f,y+halfFloor,true);

        for(int lane=0;lane<2;++lane) {
            const float x=lane==0?28.f:29.7f;
            const float direction=lane==0?-1.f:1.f;
            const float startZ=lane==0?-.2f:-4.4f;
            const float startY=lane==0?y:y+halfFloor;
            const float endY=startY+halfFloor;
            const float endZ=startZ+direction*flight;
            const float tilt=lane==0?20.85f:-20.85f;
            box({x,(startY+endY)*.5f-.14f,(startZ+endZ)*.5f},{flightWidth,.16f,4.49f},concrete,tilt);
            for(int step=0;step<steps;++step) {
                const float t=(step+.5f)/steps;
                const float z=startZ+direction*flight*t;
                const float height=startY+halfFloor*(step+1)/steps;
                box({x,height-.08f,z},{flightWidth,.16f,flight/steps+.015f},concrete);
                for(float side:{-flightWidth*.5f+.025f,flightWidth*.5f-.025f})
                    box({x+side,height+.45f,z},{.04f,.9f,.04f},metal);
            }
            for(float side:{-flightWidth*.5f+.025f,flightWidth*.5f-.025f})
                box({x+side,(startY+endY)*.5f+1.f,(startZ+endZ)*.5f},{.055f,.055f,4.49f},metal,tilt);
        }
    }

    // One continuous slab per floor joins the corridor and near landing.
    for(int floor=-3;floor<=2;++floor) {
        const float y=floor*3.2f;
        box({28.275f,y-.15f,1.9f},{4.55f,.3f,4.2f},concrete);
        box({28.275f,y+.3f,4.f},{4.55f,.6f,.18f},concrete);
        box({28.275f,y+1.05f,4.f},{4.55f,.08f,.08f},metal);
        box({30.55f,y+.3f,1.9f},{.18f,.6f,4.2f},concrete);
        box({30.55f,y+1.05f,1.9f},{.08f,.08f,4.2f},metal);
        for(int post=0;post<=6;++post) {
            box({26.f+post*(4.55f/6.f),y+.8f,4.f},{.055f,.5f,.055f},metal);
            box({30.55f,y+.8f,-.2f+post*.7f},{.055f,.5f,.055f},metal);
        }
    }
    world.AddBox({{26.f,-.3f,-.2f},{30.55f,0.f,4.f}});
    world.AddBox({{26.f,0.f,3.95f},{30.65f,6.f,4.1f}});
    world.AddBox({{30.5f,0.f,-.2f},{30.65f,6.f,4.f}});
    // Invisible entrance barrier prevents walking or jumping onto the stairs.
    world.AddBox({{24.f,0.f,-.1f},{30.65f,6.f,1.1f}});
    WorldGeometry::Bake(blocks,batches);
}
}
