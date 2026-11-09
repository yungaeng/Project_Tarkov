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
        if(i>=2) box({x+3.f,1.25f,0},{2.f,2.5f,.2f},wall);
        // Raised room-number tally marks above each doorway.
        for(int n=0;n<=i;++n)
            box({x+2.65f+n*.22f,2.85f,.13f},{.08f,.3f,.05f},{.85f,.75f,.45f});
    }
    box({24,1.6f,-4},{.2f,3.2f,8},wall);
    // Room 1: one compact medical table, a cabinet, and a single outfit on a rail.
    propBox({1.2f,.85f,-6},{1.5f,.15f,1},{.72f,.74f,.7f});
    for(float x:{.65f,1.75f}) propBox({x,.4f,-6},{.12f,.8f,.8f},trim);
    propBox({1.2f,1.05f,-6},{.5f,.25f,.4f},{.85f,.85f,.8f});
    propDetail({.84f,1.15f,-6.28f},{.18f,.18f,.18f},{.84f,.82f,.72f});
    propDetail({1.16f,1.15f,-6.30f},{.16f,.18f,.16f},{.63f,.72f,.61f},IndustrialZone::Shape::Cylinder);
    propDetail({1.48f,1.15f,-6.28f},{.22f,.16f,.16f},{.75f,.70f,.57f});
    propDetail({1.2f,2.12f,-7.82f},{.72f,.72f,.07f},{.83f,.79f,.67f});
    propDetail({1.2f,2.12f,-7.77f},{.16f,.52f,.035f},{.76f,.08f,.06f});
    propDetail({1.2f,2.12f,-7.75f},{.52f,.16f,.04f},{.76f,.08f,.06f});
    propBox({2.25f,.65f,-6.5f},{.65f,1.3f,.65f},{.28f,.32f,.29f});
    propDetail({2.25f,1.35f,-6.5f},{.72f,.08f,.72f},{.35f,.38f,.34f});
    propDetail({2.05f,1.42f,-6.5f},{.16f,.12f,.12f},{.72f,.76f,.69f});

    const glm::vec3 cloth(.27f,.35f,.33f);
    propBox({4.45f,.12f,-6.8f},{1.8f,.24f,.65f},{.30f,.25f,.20f});
    for(float x:{3.65f,5.25f}) {
        propDetail({x,1.15f,-6.6f},{.08f,2.1f,.08f},{.23f,.25f,.24f});
        propDetail({x,2.2f,-6.6f},{.12f,.10f,.12f},{.34f,.35f,.31f});
    }
    propDetail({4.45f,2.12f,-6.6f},{1.65f,.12f,.12f},{.24f,.26f,.24f});
    propDetail({4.45f,1.52f,-6.59f},{.28f,.05f,.07f},{.62f,.58f,.45f});
    propDetail({4.45f,1.15f,-6.59f},{.48f,.72f,.12f},cloth);
    propDetail({4.17f,1.27f,-6.59f},{.20f,.42f,.11f},cloth,IndustrialZone::Shape::Box,{0,0,-12});
    propDetail({4.73f,1.27f,-6.59f},{.20f,.42f,.11f},cloth,IndustrialZone::Shape::Box,{0,0,12});

    // Room 2: one rifle, a compact bed, and the water refrigerator.
    for(float x:{7.5f,10.5f}) {
        propDetail({x,1.08f,-7.15f},{.12f,2.16f,.22f},{.27f,.30f,.28f});
        propDetail({x,2.2f,-7.15f},{.22f,.10f,.34f},{.43f,.39f,.30f});
    }
    propDetail({9.f,1.45f,-7.15f},{3.f,.09f,.24f},{.22f,.25f,.23f});
    const glm::vec3 metal(.18f,.20f,.19f), stock(.34f,.26f,.19f);
    propDetail({9.f,1.45f,-6.95f},{.52f,.11f,.10f},metal);
    propDetail({9.40f,1.465f,-6.95f},{.52f,.045f,.045f},metal);
    propDetail({8.62f,1.45f,-6.95f},{.28f,.13f,.11f},stock);
    propDetail({8.94f,1.34f,-6.95f},{.11f,.18f,.09f},stock);
    propDetail({9.10f,1.31f,-6.95f},{.12f,.22f,.08f},metal);
    propDetail({9.50f,1.485f,-6.95f},{.12f,.06f,.07f},{.30f,.31f,.29f});
    propDetail({9.f,.43f,-6.8f},{2.7f,.06f,.5f},{.30f,.31f,.28f});

    propBox({7.65f,.25f,-5.35f},{1.8f,.5f,2.8f},{.29f,.27f,.23f});
    propBox({7.65f,.58f,-5.35f},{1.75f,.20f,2.72f},{.54f,.50f,.40f});
    propDetail({7.65f,.72f,-5.55f},{1.55f,.16f,1.55f},{.43f,.40f,.34f});
    propDetail({7.65f,.78f,-6.55f},{.62f,.20f,.52f},{.70f,.67f,.57f});

    propBox({10.65f,.98f,-5.85f},{1.22f,1.76f,1.12f},{.57f,.58f,.53f});
    propDetail({10.65f,1.02f,-5.28f},{1.08f,1.54f,.08f},{.48f,.51f,.49f});
    propDetail({10.10f,1.02f,-5.22f},{.05f,1.40f,.06f},{.28f,.30f,.28f});
    propDetail({11.12f,1.00f,-5.20f},{.06f,.34f,.07f},{.74f,.74f,.66f});
    propDetail({10.65f,1.90f,-5.28f},{1.10f,.08f,.09f},{.68f,.70f,.64f});
    propDetail({10.65f,.76f,-5.28f},{1.10f,.08f,.09f},{.68f,.70f,.64f});
    propDetail({10.65f,1.40f,-5.66f},{.14f,.38f,.14f},{.67f,.72f,.61f},IndustrialZone::Shape::Cylinder);
    propDetail({10.65f,1.61f,-5.66f},{.15f,.05f,.15f},{.78f,.77f,.68f});
    propDetail({10.15f,1.60f,-7.72f},{.50f,.12f,.05f},{.35f,.38f,.33f});

    // Torn wallpaper and dust collect along the wall joints and beneath the furniture.
    for(int room=0;room<2;++room) {
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
