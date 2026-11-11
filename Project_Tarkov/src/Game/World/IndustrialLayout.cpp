#include "../../Core/AssetCatalog.h"
#include "IndustrialZone.h"
#include "../../Core/RuntimePaths.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>

namespace {
struct Layout {
    std::array<glm::vec3,2> spawns{};
    std::array<IndustrialZone::Exit,2> exits{{{"X1 북동쪽 검문소",{}},{"X2 남동쪽 폐도로",{}}}};
    std::array<glm::vec3,5> buildings{},enemies{};
};
const Layout& Data() {
    static const auto data=[] {
        Layout result;
        std::array<bool,14> seen{};
        std::ifstream input(RuntimePaths::Asset(AssetCatalog::IndustrialLocations));
        if(!input) throw std::runtime_error("Missing industrial locations.layout");
        std::string line;
        while(std::getline(input,line)) {
            std::istringstream row(line);std::string kind;int index;glm::vec3 position;
            if(!(row>>kind)||kind[0]=='#') continue;
            if(!(row>>index>>position.x>>position.y>>position.z)||index<0||
                !std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))
                throw std::runtime_error("Invalid industrial location: "+line);
            int slot=-1;
            if(kind=="spawn"&&index<2) {slot=index;result.spawns[index]=position;}
            else if(kind=="exit"&&index<2) {slot=2+index;result.exits[index].position=position;}
            else if(kind=="building"&&index<5) {slot=4+index;result.buildings[index]=position;}
            else if(kind=="enemy"&&index<5) {slot=9+index;result.enemies[index]=position;}
            if(slot<0||seen[slot]) throw std::runtime_error("Unknown or duplicate industrial location: "+line);
            seen[slot]=true;
        }
        for(bool present:seen) if(!present) throw std::runtime_error("Incomplete industrial locations.layout");
        return result;
    }();
    return data;
}
}
namespace IndustrialZone {
const std::array<glm::vec3,2>& Spawns() {return Data().spawns;}
const std::array<Exit,2>& Exits() {return Data().exits;}
const std::array<glm::vec3,5>& Buildings() {return Data().buildings;}
const std::array<glm::vec3,5>& Enemies() {return Data().enemies;}
}
