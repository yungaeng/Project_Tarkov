#pragma once
#include "IndustrialGeometry.h"
#include "IndustrialDetails.h"

namespace HideoutMap {
inline void Build(CollisionWorld& world, std::vector<IndustrialZone::Batch>& batches) {
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
        if(i>=2) detail({x+2.2f,1.25f,.2f},{1.8f,2.5f,.06f},trim,
            IndustrialZone::Shape::Box,{0,68.f,0});
        // Raised room-number tally marks above each doorway.
        for(int n=0;n<=i;++n)
            box({x+2.65f+n*.22f,2.85f,.13f},{.08f,.3f,.05f},{.85f,.75f,.45f});
    }
    box({24,1.6f,-4},{.2f,3.2f,8},wall);
    // Room 1: a medical table and a clearly marked first-aid cross.
    box({1.2f,.85f,-6},{1.5f,.15f,1},{.72f,.74f,.7f});
    for(float x:{.65f,1.75f}) box({x,.4f,-6},{.12f,.8f,.8f},trim);
    box({1.2f,1.05f,-6},{.5f,.25f,.4f},{.85f,.85f,.8f});
    detail({1.2f,1.19f,-6},{.34f,.035f,.14f},{.78f,.12f,.10f});
    detail({.84f,1.15f,-6.28f},{.18f,.18f,.18f},{.84f,.82f,.72f});
    detail({1.16f,1.15f,-6.30f},{.16f,.18f,.16f},{.63f,.72f,.61f},IndustrialZone::Shape::Cylinder);
    detail({1.48f,1.15f,-6.28f},{.22f,.16f,.16f},{.75f,.70f,.57f});
    detail({1.2f,2.12f,-7.82f},{.72f,.72f,.07f},{.83f,.79f,.67f});
    detail({1.2f,2.12f,-7.77f},{.16f,.52f,.035f},{.76f,.08f,.06f});
    detail({1.2f,2.12f,-7.75f},{.52f,.16f,.04f},{.76f,.08f,.06f});
    box({2.25f,.65f,-6.5f},{.65f,1.3f,.65f},{.28f,.32f,.29f});
    detail({2.25f,1.35f,-6.5f},{.72f,.08f,.72f},{.35f,.38f,.34f});
    detail({2.05f,1.42f,-6.5f},{.16f,.12f,.12f},{.72f,.76f,.69f});

    // Room 2: open weapon rack with several recognizable rifle silhouettes.
    for(float x:{6.75f,11.25f}) {
        detail({x,1.08f,-6.9f},{.12f,2.16f,.22f},{.27f,.30f,.28f});
        detail({x,2.2f,-6.9f},{.22f,.10f,.34f},{.43f,.39f,.30f});
    }
    for(float y:{.65f,1.55f,2.12f})
        detail({9.f,y,-6.9f},{4.5f,.09f,.24f},{.22f,.25f,.23f});
    auto rifleShape=[&](float x,float y,float z) {
        const glm::vec3 metal(.18f,.20f,.19f), stock(.34f,.26f,.19f);
        detail({x,y,z},{.52f,.11f,.10f},metal);
        detail({x+.40f,y+.015f,z},{.52f,.045f,.045f},metal);
        detail({x-.38f,y,z},{.28f,.13f,.11f},stock);
        detail({x-.06f,y-.11f,z},{.11f,.18f,.09f},stock);
        detail({x+.10f,y-.14f,z},{.12f,.22f,.08f},metal);
        detail({x+.50f,y+.035f,z},{.12f,.06f,.07f},{.30f,.31f,.29f});
    };
    for(float y:{.98f,1.72f,2.45f}) rifleShape(9.f,y,-6.68f);
    detail({9.f,.43f,-6.55f},{3.6f,.06f,.5f},{.30f,.31f,.28f});

    // Room 3: hanging clothes on a freestanding rail and a low dresser.
    box({14.8f,.12f,-6.9f},{3.5f,.24f,.75f},{.30f,.25f,.20f});
    box({14.8f,.68f,-6.9f},{3.3f,.9f,.65f},{.32f,.27f,.21f});
    for(float x:{13.2f,16.4f}) {
        detail({x,1.55f,-6.6f},{.08f,2.7f,.08f},{.23f,.25f,.24f});
        detail({x,2.9f,-6.6f},{.12f,.10f,.12f},{.34f,.35f,.31f});
    }
    detail({14.8f,2.75f,-6.6f},{3.3f,.12f,.12f},{.24f,.26f,.24f});
    for(int garment=0;garment<4;++garment) {
        const float x=13.7f+garment*.72f;
        const glm::vec3 cloth=garment%2?glm::vec3(.39f,.34f,.25f):glm::vec3(.27f,.35f,.33f);
        detail({x,2.56f,-6.59f},{.28f,.05f,.07f},{.62f,.58f,.45f});
        detail({x,2.12f,-6.59f},{.48f,.72f,.12f},cloth);
        detail({x-.28f,2.24f,-6.59f},{.20f,.42f,.11f},cloth,IndustrialZone::Shape::Box,{0,0,-12});
        detail({x+.28f,2.24f,-6.59f},{.20f,.42f,.11f},cloth,IndustrialZone::Shape::Box,{0,0,12});
        detail({x-.12f,1.53f,-6.59f},{.20f,.62f,.11f},cloth);
        detail({x+.12f,1.53f,-6.59f},{.20f,.62f,.11f},cloth);
    }
    detail({14.8f,1.22f,-6.45f},{3.4f,.08f,.8f},{.40f,.35f,.27f});

    // Room 4: a worn bed and a stocked water-only refrigerator.
    box({19.55f,.25f,-6.25f},{2.55f,.5f,3.05f},{.29f,.27f,.23f});
    box({19.55f,.58f,-6.25f},{2.48f,.20f,2.98f},{.54f,.50f,.40f});
    detail({19.55f,.72f,-6.38f},{2.22f,.16f,1.70f},{.43f,.40f,.34f});
    detail({19.55f,.78f,-7.38f},{.78f,.20f,.58f},{.70f,.67f,.57f});
    box({19.55f,.78f,-4.72f},{2.55f,1.56f,.16f},{.27f,.28f,.25f});
    box({22.75f,.98f,-6.65f},{1.35f,1.96f,1.25f},{.57f,.58f,.53f});
    detail({22.75f,1.02f,-6.00f},{1.20f,1.72f,.08f},{.48f,.51f,.49f});
    detail({22.18f,1.02f,-5.94f},{.05f,1.55f,.06f},{.28f,.30f,.28f});
    detail({23.20f,1.00f,-5.92f},{.06f,.34f,.07f},{.74f,.74f,.66f});
    detail({22.75f,1.98f,-6.00f},{1.22f,.08f,.09f},{.68f,.70f,.64f});
    detail({22.75f,.72f,-6.00f},{1.22f,.08f,.09f},{.68f,.70f,.64f});
    for(float x:{22.48f,22.78f,23.05f}) {
        detail({x,1.40f,-6.38f},{.14f,.38f,.14f},{.67f,.72f,.61f},IndustrialZone::Shape::Cylinder);
        detail({x,1.61f,-6.38f},{.15f,.05f,.15f},{.78f,.77f,.68f});
    }
    detail({20.15f,1.82f,-7.72f},{.75f,.12f,.05f},{.35f,.38f,.33f});

    // Torn wallpaper and dust collect along the wall joints and beneath the furniture.
    for(int room=0;room<4;++room) {
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
