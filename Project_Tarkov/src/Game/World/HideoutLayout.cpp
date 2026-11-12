#include "../../Core/AssetCatalog.h"
#include "HideoutLayout.h"
#include "../../Core/RuntimePaths.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>

namespace {
struct Part { WorldGeometry::Block block; bool solid=false, blanket=false, wall=false; };
struct Placement { std::string name; float x=0,z=0,radius=0; int storage=-1,room=-1; std::vector<Part> parts; };
bool Finite(glm::vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
const std::vector<Placement>& Placements() {
    static const auto placements=[] {
        std::vector<Placement> result;
        std::ifstream layout(RuntimePaths::Asset(AssetCatalog::HideoutRoomLayout));
        if(!layout) throw std::runtime_error("Missing hideout room.layout");
        std::string line;
        while(std::getline(layout,line)) {
            std::istringstream row(line); Placement placement;
            if(!(row>>placement.name) || placement.name[0]=='#') continue;
            if(placement.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos ||
                !(row>>placement.x>>placement.z>>placement.storage>>placement.radius) ||
                !std::isfinite(placement.x)||!std::isfinite(placement.z)||!std::isfinite(placement.radius)||
                placement.radius<0 || placement.storage < -1 || placement.storage>4)
                throw std::runtime_error("Invalid hideout placement: "+line);
            row >> std::ws;
            if (!row.eof() && (!(row >> placement.room) || placement.room < -1 || placement.room > 3))
                throw std::runtime_error("Invalid hideout room: "+line);
            for(const auto& previous:result)
                if(previous.name==placement.name || (placement.storage>=0 && previous.storage==placement.storage))
                    throw std::runtime_error("Duplicate hideout placement: "+placement.name);
            std::ifstream input(RuntimePaths::Asset(std::string(AssetCatalog::HideoutPrefabs)+placement.name+".parts"));
            if(!input) throw std::runtime_error("Missing hideout prefab: "+placement.name);
            std::string partLine;
            while(std::getline(input,partLine)) {
                std::istringstream data(partLine); std::string condition,anchor; int solid=0,shape=0; Part part;
                if(!(data>>condition)||condition[0]=='#') continue;
                auto& b=part.block;
                if(!(data>>anchor>>solid>>shape>>b.center.x>>b.center.y>>b.center.z>>b.size.x>>b.size.y>>b.size.z
                    >>b.color.r>>b.color.g>>b.color.b>>b.rotation.x>>b.rotation.y>>b.rotation.z) ||
                    (condition!="always"&&condition!="blanket") || (anchor!="local"&&anchor!="wall") ||
                    (solid!=0&&solid!=1)||shape<0||shape>3||!Finite(b.center)||!Finite(b.size)||!Finite(b.color)||!Finite(b.rotation)||
                    b.size.x<=0||b.size.y<=0||b.size.z<=0 ||
                    (solid && (b.rotation.x!=0||b.rotation.y!=0||b.rotation.z!=0)))
                    throw std::runtime_error("Invalid hideout prefab part: "+placement.name);
                b.shape=static_cast<WorldGeometry::Shape>(shape);
                part.solid=solid!=0;part.blanket=condition=="blanket";part.wall=anchor=="wall";
                placement.parts.push_back(part);
            }
            if(placement.parts.empty()) throw std::runtime_error("Empty hideout prefab: "+placement.name);
            result.push_back(std::move(placement));
        }
        for(const auto* required:{"medicalTable","pelicanCase","refrigerator","bed","chair"}) {
            if(std::none_of(result.begin(),result.end(),[&](const auto& p){return p.name==required;}))
                throw std::runtime_error(std::string("Missing hideout placement: ")+required);
        }
        std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.storage<b.storage;});
        return result;
    }();
    return placements;
}
const Placement& Find(const std::string& name) {
    for(const auto& p:Placements()) if(p.name==name) return p;
    throw std::runtime_error("Unknown hideout prefab: "+name);
}
}
namespace HideoutLayout {
void AppendFurniture(int room,bool blanket,CollisionWorld& world,std::vector<WorldGeometry::Block>& blocks) {
    for(const auto& placement:Placements()) for(const auto& part:placement.parts) {
        if(placement.room>=0 && placement.room!=room) continue;
        if(part.blanket&&!blanket) continue;
        auto block=part.block;
        block.center+=glm::vec3(room*6.f+placement.x,0,part.wall?0:placement.z);
        blocks.push_back(block);
        if(part.solid) world.AddBox({block.center-block.size*.5f,block.center+block.size*.5f});
    }
}
int NearestStorage(int room,const glm::vec3& p) {
    int result=-1; float nearest=(std::numeric_limits<float>::max)();
    for(const auto& placement:Placements()) {
        if(placement.storage<0 || (placement.room>=0 && placement.room!=room)) continue;
        const auto delta=p-Anchor(room,placement.name);
        if(placement.name=="pelicanCase" && delta.z<.4f) continue;
        const float distance=delta.x*delta.x+delta.z*delta.z;
        if(distance<placement.radius*placement.radius && distance<nearest) {nearest=distance;result=placement.storage;}
    }
    return result;
}
glm::vec3 Anchor(int room,const std::string& name) { const auto& p=Find(name);return {room*6.f+p.x,0,p.z}; }
float InteractionRadius(const std::string& name) {return Find(name).radius;}
}

namespace HideoutLayout {
void AppendShell(std::uint32_t rescuedRoomMask,CollisionWorld& world,std::vector<WorldGeometry::Block>& blocks) {
    struct Entry { int states=0,collision=0; WorldGeometry::Block block; };
    static const auto shell=[] {
        std::vector<Entry> result;
        std::ifstream input(RuntimePaths::Asset(AssetCatalog::HideoutShellLayout));
        if(!input) throw std::runtime_error("Missing hideout shell.layout");
        std::string line;
        while(std::getline(input,line)) {
            std::istringstream data(line);std::string first;Entry entry;int shape=0;
            if(!(data>>first)||first[0]=='#') continue;
            std::istringstream number(first);
            auto& b=entry.block;
            if(!(number>>entry.states)||entry.states<1||entry.states>15 ||
                !(data>>entry.collision>>shape>>b.center.x>>b.center.y>>b.center.z>>b.size.x>>b.size.y>>b.size.z
                    >>b.color.r>>b.color.g>>b.color.b>>b.rotation.x>>b.rotation.y>>b.rotation.z)||
                entry.collision<0||entry.collision>2||shape<0||shape>3||!Finite(b.center)||!Finite(b.size)||!Finite(b.color)||!Finite(b.rotation)||
                b.size.x<=0||b.size.y<=0||b.size.z<=0 ||
                (entry.collision && (b.rotation.x!=0||b.rotation.y!=0||b.rotation.z!=0)))
                throw std::runtime_error("Invalid hideout shell entry: "+line);
            b.shape=static_cast<WorldGeometry::Shape>(shape);result.push_back(entry);
        }
        if(result.empty()) throw std::runtime_error("Empty hideout shell.layout");
        return result;
    }();
    const int state=1<<static_cast<int>(rescuedRoomMask&3u);
    for(const auto& entry:shell) {
        if(!(entry.states&state)) continue;
        const auto& b=entry.block;
        if(entry.collision!=2) blocks.push_back(b);
        if(entry.collision) world.AddBox({b.center-b.size*.5f,b.center+b.size*.5f});
    }
}
}
