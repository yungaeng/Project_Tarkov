#pragma once
#include "IndustrialZone.h"
#include <glm/geometric.hpp>
#include <cmath>

namespace IndustrialZone
{
    inline void AddDetails(CollisionWorld& world, std::vector<Block>& blocks)
    {
        const glm::vec3 steel(.24f,.29f,.30f), rust(.46f,.25f,.15f), wood(.46f,.34f,.20f);
        const glm::vec3 dark(.09f,.12f,.13f), concrete(.48f,.47f,.42f), yellow(.78f,.60f,.14f);
        auto part = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c, Shape shape = Shape::Box,
            glm::vec3 rotation = glm::vec3(0)) { blocks.push_back({p,s,c,shape,rotation}); };
        // Deliberately separate broad collision proxies from small visible trim.
        auto solid = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c) {
            part(p,s,c); world.AddBox({p-s*.5f,p+s*.5f});
        };
        auto drum = [&](glm::vec3 p) {
            part(p+glm::vec3(0,.6f,0), { .85f,1.2f,.85f }, rust, Shape::Cylinder);
            for (float y : {.12f, .95f}) part(p+glm::vec3(0,y,0), {.9f,.09f,.9f}, steel, Shape::Cylinder);
            world.AddBox({p-glm::vec3(.43f,0,.43f),p+glm::vec3(.43f,1.2f,.43f)});
        };
        auto burningDrum = [&](glm::vec3 p) {
            drum(p);
            part(p+glm::vec3(0,1.28f,0), {.42f,.16f,.42f}, {1.0f,.22f,.025f}, Shape::Cylinder);
            part(p+glm::vec3(0,1.7f,0), {.5f,.9f,.5f}, {1.0f,.24f,.025f}, Shape::Cone);
            part(p+glm::vec3(.08f,1.56f,0), {.25f,.55f,.25f}, {1.0f,.72f,.08f}, Shape::Cone);
        };
        auto tent = [&](glm::vec3 p, float yaw) {
            const float angle = glm::radians(yaw);
            const glm::vec3 right(std::cos(angle),0,-std::sin(angle));
            const glm::vec3 side = right * .62f;
            const glm::vec3 khaki(.39f,.39f,.26f), canvas(.31f,.34f,.23f);
            part(p+glm::vec3(0,.035f,0), {2.5f,.07f,4.0f}, dark);
            part(p+side+glm::vec3(0,.78f,0), {1.75f,.12f,4.0f}, khaki, Shape::Box, {0,yaw,48});
            part(p-side+glm::vec3(0,.78f,0), {1.75f,.12f,4.0f}, canvas, Shape::Box, {0,yaw,-48});
            part(p+glm::vec3(0,1.39f,0), {.12f,.12f,4.1f}, steel, Shape::Box, {0,yaw,0});
            for (float z : {-1.85f,1.85f}) {
                const glm::vec3 end(std::sin(angle)*z,0,std::cos(angle)*z);
                part(p+glm::vec3(0,.69f,0)+end, {1.9f,.08f,.08f}, wood,Shape::Box,{0,yaw,0});
            }
            const glm::vec3 half(
                1.05f*std::abs(std::cos(angle))+1.8f*std::abs(std::sin(angle)),
                .78f,
                1.05f*std::abs(std::sin(angle))+1.8f*std::abs(std::cos(angle)));
            world.AddBox({p-half,p+half});
        };
        const std::array<std::vector<glm::vec3>,4> trails{{
            {{-197,0,-193},{-190,0,-179},{-197,0,-157},{-181,0,-143},{-190,0,-124},{-166,0,-103}},
            {{-198,0,-76},{-179,0,-58},{-192,0,-37},{-173,0,-19},{-187,0,1},{-166,0,22},{-176,0,44}},
            {{-194,0,179},{-174,0,161},{-189,0,141},{-169,0,123},{-179,0,101}},
            {{166,0,-184},{184,0,-163},{168,0,-142},{187,0,-121},{170,0,-101},{184,0,-80},{166,0,-60},{180,0,-39}}
        }};
        const std::array<glm::vec3,4> camps{{
            {-174,0,-145},{-174,0,-31},{-170,0,145},{174,0,-112}
        }};
        auto nearTrail = [&](glm::vec3 p, float clearance) {
            for (const auto& trail : trails) for (std::size_t i=1;i<trail.size();++i) {
                const glm::vec2 a(trail[i-1].x,trail[i-1].z), b(trail[i].x,trail[i].z);
                const glm::vec2 point(p.x,p.z), span=b-a;
                const float length2=glm::dot(span,span);
                const float t=length2>0 ? std::clamp(glm::dot(point-a,span)/length2,0.0f,1.0f) : 0.0f;
                if (glm::length(point-(a+span*t))<clearance) return true;
            }
            return false;
        };
        auto pallet = [&](glm::vec3 p) {
            for (float x : {-.65f,0.f,.65f}) part(p+glm::vec3(x,.12f,0), {.16f,.24f,1.5f},wood);
            for (int k=0;k<5;++k) part(p+glm::vec3(0,.28f,-.6f+k*.3f), {1.6f,.12f,.22f},wood);
            solid(p+glm::vec3(0,.85f,0), {1.3f,1.1f,1.2f}, wood);
            for (float x : {-.48f,.48f}) part(p+glm::vec3(x,.86f,.61f), {.08f,1.1f,.04f},steel);
        };
        for (std::size_t i=0;i<Buildings.size();++i) {
            const glm::vec3 p=Buildings[i];
            const float w=i==2?64.f:40.f, d=i==2?54.f:30.f, h=i==2?10.f:6.f;
            // Columns, rafters, lintel trim and raised roof vents.
            for (float x : {-w/2+1,w/2-1}) {
                for (float z : {-d/2+1,0.f,d/2-1})
                    solid(p+glm::vec3(x,h/2,z), {.45f,h,.45f},steel);
            }
            for (float z : {-d*.33f,0.f,d*.33f}) {
                part(p+glm::vec3(0,h-.55f,z), {w,.35f,.32f},steel);
                part(p+glm::vec3(0,h-.8f,z), {3,.12f,.5f}, { .85f,.83f,.65f });
            }
            for (float side : {-1.f,1.f}) {
                for (float x : {-3.15f,3.15f}) part(p+glm::vec3(x,1.5f,side*(d/2+.55f)), {.2f,3,.18f},yellow);
                part(p+glm::vec3(0,3.2f,side*(d/2+.55f)), {6.5f,.3f,.18f},steel);
                part(p+glm::vec3(side*9,h+.85f,0), {3,1.2f,2},steel);
                for (int slat=0;slat<6;++slat)
                    part(p+glm::vec3(side*9,h+.4f+slat*.17f,1.02f), {2.7f,.07f,.04f},dark);
            }
            // Keep x=-7..7 clear for entrances, patrols and existing loot at (+/-5,+/-8).
            if (i<2) {
                for (float x : {-14.f,14.f}) for (float z : {-7.f,7.f}) {
                    for (float dx : {-2.5f,2.5f}) for (float dz : {-1.f,1.f})
                        part(p+glm::vec3(x+dx,2,z+dz), {.15f,4,.15f},steel);
                    for (float y : {.4f,1.8f,3.2f}) {
                        part(p+glm::vec3(x,y,z), {5.3f,.15f,2.3f},steel);
                        for (float dx : {-1.5f,0.f,1.5f})
                            part(p+glm::vec3(x+dx,y+.55f,z), {1.2f,.95f,1.5f},wood);
                    }
                    world.AddBox({p+glm::vec3(x-2.7f,0,z-1.2f),p+glm::vec3(x+2.7f,4,z+1.2f)});
                }
                pallet(p+glm::vec3(-10,0,0)); drum(p+glm::vec3(10,0,1));
            }
            else if (i==3) {
                for (float side : {-1.f,1.f}) {
                    // Room divider with a 4m doorway at its center.
                    for (float z : {-8.f,8.f}) solid(p+glm::vec3(side*8,1.6f,z), {.2f,3.2f,12},concrete);
                    for (float z : {-8.f,7.f}) {
                        const auto desk=p+glm::vec3(side*13,0,z);
                        solid(desk+glm::vec3(0,.77f,0), {3,.16f,1.4f},wood);
                        for (float dx : {-1.3f,1.3f}) for (float dz : {-.5f,.5f})
                            part(desk+glm::vec3(dx,.36f,dz), {.12f,.72f,.12f},steel);
                        part(desk+glm::vec3(0,1.25f,.4f), {.9f,.65f,.12f},dark);
                        part(desk+glm::vec3(0,1.25f,.33f), {.75f,.5f,.015f}, {.19f,.32f,.35f});
                        solid(desk+glm::vec3(0,.5f,-1.5f), {.65f,1,.65f},steel);
                        part(desk+glm::vec3(0,1.1f,-1.75f), {.65f,.7f,.15f},dark);
                    }
                    solid(p+glm::vec3(side*17,1.1f,0), {1.2f,2.2f,3},steel);
                    for (int drawer=0;drawer<5;++drawer)
                        part(p+glm::vec3(side*16.38f,.3f+drawer*.4f,0), {.04f,.04f,.6f},dark);
                }
            }
            else {
                for (float side : {-1.f,1.f}) {
                    const auto machine=p+glm::vec3(side*(i==2?18.f:13.f),0,0);
                    solid(machine+glm::vec3(0,.35f,0), {7,.7f,9},concrete);
                    solid(machine+glm::vec3(0,1.4f,0), {5,1.5f,6},steel);
                    world.AddBox({machine+glm::vec3(-2.5f,.7f,-3),machine+glm::vec3(2.5f,4.2f,3)});
                    part(machine+glm::vec3(0,2.7f,0), {3,4,3},rust,Shape::Cylinder,{90,0,0});
                    for (float z : {-1.4f,0.f,1.4f})
                        part(machine+glm::vec3(0,2.7f,z), {3.2f,.15f,3.2f},steel,Shape::Cylinder,{90,0,0});
                    solid(machine+glm::vec3(0,1.2f,5.5f), {2,2.4f,.8f},steel);
                    for (int button=0;button<4;++button)
                        part(machine+glm::vec3(-.6f+button*.4f,1.7f,5.92f), {.12f,.12f,.04f},yellow);
                }
                for (float x : {-11.f,11.f})
                    part(p+glm::vec3(x,h-1.3f,0), {.5f,d-2,.5f},rust,Shape::Cylinder,{90,0,0});
            }
        }
        // Container corrugation, corner posts, double doors and locking bars.
        for (int row=0;row<5;++row) for (int col=0;col<8;++col) {
            const glm::vec3 p(78.f+col*10,0,-155.f+row*18);
            for (float side : {-1.f,1.f}) {
                for (int rib=0;rib<16;++rib)
                    part(p+glm::vec3(side*2.04f,1.5f,-5.5f+rib*.73f), {.08f,2.65f,.12f},steel);
                for (float z : {-6.05f,6.05f}) {
                    part(p+glm::vec3(side*1.9f,1.5f,z), {.18f,3.05f,.18f},steel);
                    part(p+glm::vec3(side*.96f,1.5f,z), {1.8f,2.75f,.08f},rust);
                    part(p+glm::vec3(side*.8f,1.5f,z+std::copysign(.09f,z)), {.07f,2.6f,.07f},yellow);
                }
            }
        }
        // Exterior tanks, stack and connecting pipe runs near the factory/power station.
        for (const auto p : {glm::vec3(44,0,-12),glm::vec3(44,0,12),glm::vec3(153,0,108)}) {
            part(p+glm::vec3(0,4,0), {9,8,9},steel,Shape::Cylinder);
            part(p+glm::vec3(0,8.6f,0), {9,1.2f,9},concrete,Shape::Cone);
            world.AddBox({p-glm::vec3(4.5f,0,4.5f),p+glm::vec3(4.5f,8,4.5f)});
            for (float y : {.3f,4.f,7.7f}) part(p+glm::vec3(0,y,0), {9.2f,.18f,9.2f},rust,Shape::Cylinder);
        }
        for (int band=0;band<8;++band)
            part({-25,1.5f+band*3,35},{3,3,3},band%2?concrete:rust,Shape::Cylinder);
        world.AddBox({{-26.5f,0,33.5f},{-23.5f,24,36.5f}});
        // Wrecks and layered barriers along the approach roads.
        for (int i=0;i<8;++i) {
            const glm::vec3 p(-175.f+i*48,0,177);
            solid(p+glm::vec3(0,.85f,0), {5,1.1f,2.3f},i%2?rust:steel);
            solid(p+glm::vec3(-.3f,1.6f,0), {2.6f,.8f,2.1f},steel);
            part(p+glm::vec3(-.3f,1.75f,1.06f), {2.15f,.42f,.025f},dark);
            for (float x : {-1.6f,1.6f}) for (float z : {-1.2f,1.2f})
                part(p+glm::vec3(x,.5f,z), {1,.3f,1},dark,Shape::Cylinder,{90,0,0});
            for (int layer=0;layer<3;++layer) for (int bag=0;bag<4;++bag)
                solid(p+glm::vec3(-2+bag*1.1f+(layer%2)*.25f,.2f+layer*.38f,7), {1.05f,.36f,.65f},{.44f,.42f,.28f});
        }
        // Dense growth in the interior blocks, with reproducible placement.
        auto plantingSpace = [&](glm::vec3 p, float radius) {
            if (std::abs(p.x)>198-radius || std::abs(p.z)>198-radius) return false;
            if (std::abs(p.x-70)<8+radius || std::abs(p.z-70)<8+radius) return false;
            if (nearTrail(p,3.8f+radius)) return false;
            for (const auto camp : camps)
                if (glm::length(glm::vec2(p.x-camp.x,p.z-camp.z))<9.0f+radius) return false;
            for (std::size_t i=0;i<Buildings.size();++i) {
                const auto offset=p-Buildings[i];
                const float halfW=i==2?32.f:20.f, halfD=i==2?27.f:15.f;
                if (std::abs(offset.x)<halfW+3+radius && std::abs(offset.z)<halfD+3+radius) return false;
                // Reserve the approaches to both entrances.
                if (std::abs(offset.x)<7+radius && std::abs(offset.z)<halfD+16+radius) return false;
            }
            if (p.x>73-radius && p.x<155+radius && p.z>-165-radius && p.z<-73+radius) return false;
            for (auto spawn : Spawns) if (glm::length(p-spawn)<16+radius) return false;
            for (const auto& exit : Exits) if (glm::length(p-exit.position)<18+radius) return false;
            for (auto enemy : Enemies) if (glm::length(p-enemy)<9+radius) return false;
            return world.CanOccupy(p+glm::vec3(0,.05f,0),radius,12.f);
        };
        const glm::vec3 pine(.13f,.25f,.12f), leaf(.19f,.30f,.13f);
        const glm::vec3 shrubGreen(.23f,.31f,.14f), grassGreen(.30f,.36f,.17f);
        for (int row=0;row<27;++row) for (int col=0;col<27;++col) {
            const int seed=row*97+col*53;
            const glm::vec3 p(-190.f+col*14.5f+std::sin(float(seed))*3.8f,0,
                -190.f+row*14.5f+std::cos(float(seed*3))*3.8f);
            if (seed%5==0 || !plantingSpace(p,3.5f)) continue;
            const float height=6.5f+(seed%7)*.65f;
            part(p+glm::vec3(0,height*.35f,0), {.65f,height*.7f,.65f},wood,Shape::Cylinder);
            world.AddBox({p-glm::vec3(.325f,0,.325f),p+glm::vec3(.325f,height*.7f,.325f)});
            if (seed%3) {
                for (int tier=0;tier<3;++tier)
                    part(p+glm::vec3(0,height*.48f+tier*1.35f,0),
                        {5.5f-tier,4.2f,5.5f-tier},pine,Shape::Cone);
            } else {
                for (int crown=0;crown<3;++crown)
                    part(p+glm::vec3((crown-1)*1.2f,height-.5f+crown*.35f,crown%2),
                        {4.8f,4.2f,4.8f},leaf,Shape::Rock,{0,float(seed+crown*47),0});
            }
        }
        // Low growth is visual only, allowing movement through shrubs and grass.
        for (int row=0;row<55;++row) for (int col=0;col<55;++col) {
            const int seed=row*71+col*43;
            const glm::vec3 p(-194.f+col*7.2f+std::sin(float(seed))*2.2f,0,
                -194.f+row*7.2f+std::cos(float(seed*2))*2.2f);
            if (seed%6==0 || !plantingSpace(p,1.6f)) continue;
            if (seed%3==0) {
                for (int clump=0;clump<3;++clump)
                    part(p+glm::vec3((clump-1)*.65f,.5f,clump%2*.45f),
                        {1.65f,1.2f,1.5f},shrubGreen,Shape::Rock,{0,float(seed+clump*31),0});
            } else {
                for (int tuft=0;tuft<3;++tuft) {
                    const float height=.35f+(seed+tuft)%4*.12f;
                    part(p+glm::vec3((tuft-1)*.4f,height*.5f,tuft%2*.35f),
                        {.55f,height,.55f},grassGreen,Shape::Cone);
                }
            }
        }
        // Extra saplings frame the clearings while keeping the paths and tent footprints open.
        for (std::size_t i=0;i<camps.size();++i) for (int sapling=0;sapling<4;++sapling) {
            const float signX=sapling%2 ? 1.0f : -1.0f;
            const float signZ=sapling<2 ? 1.0f : -1.0f;
            const glm::vec3 p=camps[i]+glm::vec3(signX*(12.0f+(i%2)*2.0f),0,signZ*11.0f);
            if (!plantingSpace(p,2.8f)) continue;
            const float height=4.2f+float((i*3+sapling)%4)*.45f;
            part(p+glm::vec3(0,height*.38f,0), {.42f,height*.76f,.42f},wood,Shape::Cylinder);
            world.AddBox({p-glm::vec3(.21f,0,.21f),p+glm::vec3(.21f,height*.76f,.21f)});
            const glm::vec3 crownColor=sapling%2 ? glm::vec3(.20f,.32f,.15f) : glm::vec3(.25f,.34f,.16f);
            for (int crown=0;crown<2;++crown)
                part(p+glm::vec3((crown?.6f:-.6f),height*.77f+crown*.25f,0),
                    {3.6f,3.2f,3.4f},crownColor,Shape::Rock,{0,float(i*67+sapling*31+crown*19),0});
        }
        // Deterministic forest variation; clear the spawn/extraction approaches.
        auto clear = [&](glm::vec3 p) {
            for (auto spawn : Spawns) if (glm::length(p-spawn)<15) return false;
            for (auto exit : Exits) if (glm::length(p-exit.position)<16) return false;
            return true;
        };
        for (int edge=0;edge<4;++edge) for (int i=0;i<28;++i) {
            const float along=-230+i*17.f;
            const float across=228.f+(i%3)*5;
            glm::vec3 p=edge<2?glm::vec3(edge?across:-across,0,along):glm::vec3(along,0,edge==2?-across:across);
            if (!clear(p)) continue;
            const float height=6+(i%5)*.8f;
            part(p+glm::vec3(0,height*.35f,0), {.7f,height*.7f,.7f},wood,Shape::Cylinder);
            world.AddBox({p-glm::vec3(.35f,0,.35f),p+glm::vec3(.35f,height*.7f,.35f)});
            if (i%3) {
                for (int tier=0;tier<3;++tier)
                    part(p+glm::vec3(0,height*.5f+tier*1.25f,0), {5.f-tier,3.8f,5.f-tier},
                        {.13f,.25f+tier*.025f,.12f},Shape::Cone);
            } else {
                for (int crown=0;crown<3;++crown)
                    part(p+glm::vec3((crown-1)*1.3f,height-.7f+std::sin(float(i+crown)),crown%2),
                        {4.5f,4,4.5f},{.19f,.30f,.13f},Shape::Rock,{0,float(i*23+crown*47),0});
            }
            const glm::vec3 shrub=p+glm::vec3(3,0,2);
            for (int clump=0;clump<3;++clump)
                part(shrub+glm::vec3(clump*.65f,.55f,clump%2*.5f), {1.8f,1.4f,1.7f},{.23f,.31f,.14f},Shape::Rock);
            if (i%4==0) {
                const auto rock=p+glm::vec3(-3,0,3);
                part(rock+glm::vec3(0,.8f,0), {3,2,2.6f},concrete,Shape::Rock,{0,float(i*17),0});
                // Inset proxy avoids an invisible square around the irregular rock silhouette.
                world.AddBox({rock-glm::vec3(.8f,0,.7f),rock+glm::vec3(.8f,1.25f,.7f)});
            }
        }
        // Meandering dirt tracks link the wooded approaches without replacing the main roads.
        const glm::vec3 trailColor(.34f,.29f,.20f), trailEdge(.39f,.33f,.22f);
        for (std::size_t route=0;route<trails.size();++route) {
            const auto& points=trails[route];
            for (std::size_t i=1;i<points.size();++i) {
                const glm::vec3 delta=points[i]-points[i-1];
                const float length=glm::length(glm::vec2(delta.x,delta.z));
                const float yaw=glm::degrees(std::atan2(delta.x,delta.z));
                const float width=route==3 ? 4.6f : 4.0f;
                part((points[i]+points[i-1])*.5f+glm::vec3(0,.018f,0),
                    {width,.025f,length+.35f},route%2 ? trailColor : trailEdge,Shape::Box,{0,yaw,0});
                if ((i+route)%2==0) {
                    const float side=(i%2) ? 1.0f : -1.0f;
                    const glm::vec3 midpoint=(points[i]+points[i-1])*.5f;
                    const glm::vec3 offset(side*std::cos(glm::radians(yaw))*2.5f,0,
                        -side*std::sin(glm::radians(yaw))*2.5f);
                    for (int tuft=0;tuft<3;++tuft)
                        part(midpoint+offset+glm::vec3((tuft-1)*.55f,.32f,tuft%2*.3f),
                            {.8f,.7f,.8f},tuft%2 ? shrubGreen : grassGreen,Shape::Rock,
                            {0,float(route*37+i*19+tuft*41),0});
                }
            }
        }
        // Small canvas camps sit just off the tracks; the fire barrels also block movement.
        for (std::size_t i=0;i<camps.size();++i) {
            const float yaw=static_cast<float>((i*37)%90)-35.0f;
            tent(camps[i],yaw);
            burningDrum(camps[i]+glm::vec3(3.0f,0,1.0f));
            for (int log=0;log<3;++log) {
                const float angle=glm::radians(float(log*60+i*23));
                const glm::vec3 offset(std::cos(angle)*4.3f,0,std::sin(angle)*4.3f);
                part(camps[i]+offset+glm::vec3(0,.16f,0), {1.5f,.28f,.28f},wood,
                    Shape::Cylinder,{0,float(log*60+i*23),90});
            }
            const glm::vec3 shrub=camps[i]+glm::vec3(-3,0,-2.7f);
            for (int clump=0;clump<4;++clump)
                part(shrub+glm::vec3((clump%2)*.8f,.48f,(clump/2)*.75f),
                    {1.4f,1.1f,1.3f},shrubGreen,Shape::Rock,{0,float(i*53+clump*29),0});
        }
    }
}
