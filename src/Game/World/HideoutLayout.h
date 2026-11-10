#pragma once
#include "../../Graphics/WorldGeometry.h"
#include "../../Physics/CollisionWorld.h"
#include <string>
#include <cstdint>
namespace HideoutLayout {
void AppendShell(std::uint32_t rescuedRoomMask, CollisionWorld&, std::vector<WorldGeometry::Block>&);
// Furniture IDs are persistence identifiers; they must not change with file order.
void AppendFurniture(int room, bool blanket, CollisionWorld&, std::vector<WorldGeometry::Block>&);
int NearestStorage(int room, const glm::vec3& position);
glm::vec3 Anchor(int room, const std::string& prefab);
float InteractionRadius(const std::string& prefab);
}
