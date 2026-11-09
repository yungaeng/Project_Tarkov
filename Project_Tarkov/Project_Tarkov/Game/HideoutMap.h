#pragma once
#include "Hideout.h"
#include "IndustrialGeometry.h"
#include "IndustrialDetails.h"
#include <algorithm>

namespace HideoutMap {
inline void Build(CollisionWorld& world, std::vector<IndustrialZone::Batch>& batches,
    int availableRoomCount = 2, std::uint32_t rescuedRoomMask = 0) {
    availableRoomCount=std::clamp(availableRoomCount,2,4);
    if (rescuedRoomMask==0 && availableRoomCount>Hideout::InitialRoomCount)
        rescuedRoomMask=(1u<<(availableRoomCount-Hideout::InitialRoomCount))-1u;
    for (int room=Hideout::InitialRoomCount;room<Hideout::HideoutRoomCount;++room)
        if (rescuedRoomMask&(1u<<(room-Hideout::InitialRoomCount)))
            availableRoomCount=(std::max)(availableRoomCount,room+1);
    world.Clear();
    std::vector<IndustrialZone::Block> blocks;
    // The terrace overlooks the raid map below; scenery has no hideout collision.
    CollisionWorld sceneryWorld;
    IndustrialZone::Build(sceneryWorld, blocks);
    IndustrialZone::AddDetails(sceneryWorld, blocks);
    for (auto& block : blocks) block.center += glm::vec3(0,-16,270);
    auto box = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c) {
        blocks.push_back({p,s,c});
        world.AddBox({p-s*.5f,p+s*.5f});
    };
    auto detail = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c,
        IndustrialZone::Shape shape=IndustrialZone::Shape::Box, glm::vec3 rotation=glm::vec3(0)) {
        blocks.push_back({p,s,c,shape,rotation});
    };
    auto propBox = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c) {
        box(p,s*.9f,c);
    };
    auto propDetail = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c,
        IndustrialZone::Shape shape=IndustrialZone::Shape::Box, glm::vec3 rotation=glm::vec3(0)) {
        detail(p,s*.9f,c,shape,rotation);
    };
    const glm::vec3 wall(.65f,.62f,.53f), trim(.25f,.29f,.28f);
    // Separate room and terrace slabs eliminate the side passage beside room 1.
    // No ceiling: the entire apartment opens to the sky.
    box({12,-.15f,-4},{24,.3f,8},{.32f,.31f,.28f});
    box({12,-.15f,2},{28,.3f,4},{.42f,.41f,.37f});
    // Each room gets a high frosted window with a warm patch of daylight on the floor.
    for (int room=0;room<4;++room) {
        const float center=room*6.f+3.f;
        box({center,.775f,-8},{6,1.55f,.2f},wall);
        box({center,2.8f,-8},{6,.8f,.2f},wall);
        box({center-2.f,2.05f,-8},{2,1.3f,.2f},wall);
        box({center+2.f,2.05f,-8},{2,1.3f,.2f},wall);
        box({center,2.05f,-7.86f},{1.86f,1.16f,.035f},{.78f,.72f,.55f});
        detail({center,2.65f,-7.82f},{2.04f,.08f,.12f},{.28f,.30f,.27f});
        detail({center,1.45f,-7.82f},{2.04f,.08f,.12f},{.28f,.30f,.27f});
        for(float x:{center-1.f,center+1.f})
            detail({x,2.05f,-7.82f},{.08f,1.28f,.12f},{.28f,.30f,.27f});
        detail({center,2.05f,-7.80f},{.035f,1.16f,.13f},{.34f,.35f,.31f});
        detail({center-.05f,.012f,-6.45f},{1.45f,.018f,1.8f},{.58f,.52f,.37f});
        detail({center+.62f,.014f,-5.95f},{.72f,.02f,1.2f},{.49f,.45f,.34f});
    }
    // Open terrace with a low parapet and thin metal rails, not full-height walls.
    box({12,.3f,4},{28,.6f,.18f},wall);
    box({12,1.05f,4},{28,.08f,.08f},trim);
    for(float x=-2; x<=26; x+=2)
        box({x,.8f,4},{.08f,.5f,.08f},trim);
    for(float x : {-2.f,26.f}) {
        box({x,.3f,2},{.18f,.6f,4},wall);
        box({x,1.05f,2},{.08f,.08f,4},trim);
        for(float z : {0.f,2.f,4.f}) box({x,.8f,z},{.08f,.5f,.08f},trim);
        // Close the short end behind the terrace without creating an L-shaped route.
        box({x<0 ? -1.f : 25.f,1.6f,0},{2,3.2f,.2f},wall);
    }
    // Keep the player on the terrace even when jumping over the visual railing.
    world.AddBox({{-2.1f,0,3.95f},{26.1f,6,4.1f}});
    world.AddBox({{-2.1f,0,0},{-1.95f,6,4}});
    world.AddBox({{25.95f,0,0},{26.1f,6,4}});
    for(int i=0;i<4;++i) {
        float x=i*6.f;
        box({x,1.6f,-4},{.2f,3.2f,8},wall);
        box({x+1,1.6f,0},{2,3.2f,.2f},wall);
        box({x+5,1.6f,0},{2,3.2f,.2f},wall);
        box({x+3,2.85f,0},{2,.7f,.2f},wall);
        const bool roomUnlocked=i<Hideout::InitialRoomCount ||
            (rescuedRoomMask&(1u<<(i-Hideout::InitialRoomCount)));
        if(!roomUnlocked) {
            box({x+3.f,1.25f,0},{1.86f,2.5f,.16f},{.30f,.24f,.18f});
            detail({x+2.18f,1.25f,-.1f},{.12f,2.58f,.08f},trim);
            detail({x+3.82f,1.25f,-.1f},{.12f,2.58f,.08f},trim);
            detail({x+3.f,2.52f,-.1f},{1.76f,.12f,.08f},trim);
            detail({x+3.68f,1.28f,-.12f},{.08f,.18f,.08f},{.78f,.68f,.40f},
                IndustrialZone::Shape::Cylinder);
        }
        // Raised room-number tally marks above each doorway.
        for(int n=0;n<=i;++n)
            box({x+2.65f+n*.22f,2.85f,.13f},{.08f,.3f,.05f},{.85f,.75f,.45f});
    }
    box({24,1.6f,-4},{.2f,3.2f,8},wall);
    const glm::vec3 cloth(.27f,.35f,.33f), metal(.18f,.20f,.19f), stock(.34f,.26f,.19f);
    auto medicalTable=[&](float x,float z) {
        propBox({x,.85f,z},{1.5f,.15f,1},{.72f,.74f,.7f});
        for(float leg:{x-.55f,x+.55f}) propBox({leg,.4f,z},{.12f,.8f,.8f},trim);
        propBox({x,1.05f,z},{.5f,.25f,.4f},{.85f,.85f,.8f});
        propDetail({x-.35f,1.15f,z-.28f},{.18f,.18f,.18f},{.84f,.82f,.72f});
        propDetail({x,1.15f,z-.3f},{.16f,.18f,.16f},{.63f,.72f,.61f},IndustrialZone::Shape::Cylinder);
        propDetail({x+.34f,1.15f,z-.28f},{.22f,.16f,.16f},{.75f,.70f,.57f});
        propDetail({x,2.12f,-7.82f},{.72f,.72f,.07f},{.83f,.79f,.67f});
        propDetail({x,2.12f,-7.77f},{.16f,.52f,.035f},{.76f,.08f,.06f});
        propDetail({x,2.12f,-7.75f},{.52f,.16f,.04f},{.76f,.08f,.06f});
    };
    auto wardrobe=[&](float x,float z) {
        propBox({x,.12f,z},{1.35f,.24f,.65f},{.30f,.25f,.20f});
        for(float side:{x-.58f,x+.58f}) {
            propDetail({side,1.02f,z},{.08f,1.8f,.08f},{.23f,.25f,.24f});
            propDetail({side,1.94f,z},{.12f,.10f,.12f},{.34f,.35f,.31f});
        }
        propDetail({x,1.86f,z},{1.2f,.12f,.12f},{.24f,.26f,.24f});
        propDetail({x,1.38f,z},{.28f,.05f,.07f},{.62f,.58f,.45f});
        propDetail({x,1.02f,z},{.48f,.72f,.12f},cloth);
        propDetail({x-.28f,1.14f,z},{.20f,.42f,.11f},cloth,IndustrialZone::Shape::Box,{0,0,-12});
        propDetail({x+.28f,1.14f,z},{.20f,.42f,.11f},cloth,IndustrialZone::Shape::Box,{0,0,12});
    };
    auto weaponRack=[&](float x,float z) {
        for(float side:{x-.8f,x+.8f})
            propDetail({side,1.08f,z},{.12f,2.16f,.22f},{.27f,.30f,.28f});
        propDetail({x,1.45f,z},{1.5f,.09f,.24f},{.22f,.25f,.23f});
        propDetail({x,1.45f,z+.18f},{.52f,.11f,.10f},metal);
        propDetail({x+.40f,1.465f,z+.18f},{.52f,.045f,.045f},metal);
        propDetail({x-.38f,1.45f,z+.18f},{.28f,.13f,.11f},stock);
        propDetail({x-.06f,1.34f,z+.18f},{.11f,.18f,.09f},stock);
        propDetail({x+.10f,1.31f,z+.18f},{.12f,.22f,.08f},metal);
        propDetail({x+.50f,1.485f,z+.18f},{.12f,.06f,.07f},{.30f,.31f,.29f});
        propDetail({x,.43f,z+.3f},{1.35f,.06f,.5f},{.30f,.31f,.28f});
    };
    auto refrigerator=[&](float x,float z) {
        propBox({x,.88f,z},{.95f,1.58f,.92f},{.57f,.58f,.53f});
        propDetail({x,.92f,z+.47f},{.82f,1.38f,.08f},{.48f,.51f,.49f});
        propDetail({x-.42f,.92f,z+.52f},{.05f,1.24f,.06f},{.28f,.30f,.28f});
        propDetail({x+.38f,.92f,z+.54f},{.06f,.28f,.07f},{.74f,.74f,.66f});
        propDetail({x,.3f,z},{.85f,.12f,.82f},{.42f,.43f,.39f});
    };
    auto bed=[&](float x,float z) {
        propBox({x,.25f,z},{1.45f,.5f,2.15f},{.29f,.27f,.23f});
        propBox({x,.58f,z},{1.38f,.20f,2.05f},{.54f,.50f,.40f});
        propDetail({x,.72f,z-.12f},{1.22f,.16f,1.1f},{.43f,.40f,.34f});
        propDetail({x,.78f,z-.78f},{.52f,.20f,.38f},{.70f,.67f,.57f});
    };
    auto chair=[&](float x,float z) {
        for(float side:{x-.2f,x+.2f}) propDetail({side,.28f,z},{.07f,.56f,.07f},trim);
        propDetail({x,.55f,z},{.55f,.12f,.5f},{.35f,.31f,.25f});
        propDetail({x,.94f,z-.2f},{.55f,.72f,.08f},{.32f,.29f,.24f});
    };
    for(int room=0;room<availableRoomCount;++room) {
        if(room>=Hideout::InitialRoomCount &&
            !(rescuedRoomMask&(1u<<(room-Hideout::InitialRoomCount))) ) continue;
        const float left=room*6.f;
        medicalTable(left+1.2f,-2.5f);
        weaponRack(left+2.8f,-7.f);
        wardrobe(left+4.9f,-6.8f);
        refrigerator(left+5.1f,-4.8f);
        bed(left+1.1f,-4.8f);
        chair(left+3.3f,-3.1f);
    }

    // Torn wallpaper and dust collect along the wall joints and beneath the furniture.
    for(int room=0;room<availableRoomCount;++room) {
        if(room>=Hideout::InitialRoomCount &&
            !(rescuedRoomMask&(1u<<(room-Hideout::InitialRoomCount))) ) continue;
        const float left=room*6.f;
        for(int strip=0;strip<3;++strip) {
            const float x=left+.45f+strip*.48f;
            detail({x,1.03f,-7.77f},{.38f,.72f,.025f},
                strip==1?glm::vec3(.39f,.34f,.27f):glm::vec3(.50f,.43f,.32f),
                IndustrialZone::Shape::Box,{0,0,(strip%2?14.f:-11.f)});
            detail({x+.18f,1.35f,-7.74f},{.22f,.12f,.018f},{.60f,.52f,.40f},
                IndustrialZone::Shape::Box,{0,0,-18.f});
        }
        for(int speck=0;speck<5;++speck) {
            const float x=left+.45f+speck*.88f;
            detail({x,.035f,-7.45f+(speck%2)*.35f},{.22f,.06f,.16f},
                {.37f,.34f,.28f},IndustrialZone::Shape::Rock);
        }
        detail({left+5.88f,1.02f,-5.3f},{.025f,.62f,.72f},{.46f,.40f,.31f},
            IndustrialZone::Shape::Box,{0,12.f,0});
    }
    box({-1.85f,1.25f,2},{.1f,2.5f,1.8f},{.2f,.43f,.34f});
    IndustrialZone::Bake(blocks,batches);
}
}
