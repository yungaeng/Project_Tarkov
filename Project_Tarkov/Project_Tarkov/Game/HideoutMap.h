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
    const glm::vec3 wall(.65f,.62f,.53f), trim(.25f,.29f,.28f);
    // Separate room and terrace slabs eliminate the side passage beside room 1.
    // No ceiling: the entire apartment opens to the sky.
    box({12,-.15f,-4},{24,.3f,8},{.32f,.31f,.28f});
    box({12,-.15f,2},{28,.3f,4},{.42f,.41f,.37f});
    box({12,1.6f,-8},{24,3.2f,.2f},wall);
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
        if(i>=2) box({x+3,1.25f,0},{2,2.5f,.18f},trim);
        // Raised room-number tally marks above each doorway.
        for(int n=0;n<=i;++n)
            box({x+2.65f+n*.22f,2.85f,.13f},{.08f,.3f,.05f},{.85f,.75f,.45f});
    }
    box({24,1.6f,-4},{.2f,3.2f,8},wall);
    // Room 1, left to right: medical table, armory cabinet, wardrobe.
    box({1.2f,.85f,-6},{1.5f,.15f,1},{.72f,.74f,.7f});
    for(float x:{.65f,1.75f}) box({x,.4f,-6},{.12f,.8f,.8f},trim);
    box({1.2f,1.05f,-6},{.5f,.25f,.4f},{.85f,.85f,.8f});
    box({1.2f,1.19f,-6},{.3f,.03f,.1f},{.7f,.12f,.12f});
    box({3,1.1f,-6.4f},{1.3f,2.2f,.8f},{.23f,.3f,.27f});
    box({4.8f,1.1f,-6.4f},{1.4f,2.2f,.8f},{.42f,.28f,.18f});
    for(float x:{2.75f,3.25f,4.55f,5.05f})
        box({x,1.1f,-5.96f},{.06f,.25f,.06f},{.75f,.73f,.65f});
    box({-1.85f,1.25f,2},{.1f,2.5f,1.8f},{.2f,.43f,.34f});
    IndustrialZone::Bake(blocks,batches);
}
}
