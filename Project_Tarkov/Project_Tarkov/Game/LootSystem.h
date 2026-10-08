#pragma once
#include "Entity.h"
#include "Inventory.h"
#include <string>
#include <vector>

class Player;
class Camera;
class CollisionWorld;
class Shader;
class Mesh;
struct WorldItem : Entity
{
    ItemStack stack;
    WorldItem(glm::vec3 center, ItemStack contents) : stack(contents) { position = center; }
    static constexpr float HalfSize = 0.2f;
};

class LootSystem
{
public:
    void Init();
    void Update(float dt, Player& player, const Camera& camera, const CollisionWorld& world,
        bool inputEnabled, bool pickupPressed);
    void Render(Shader& shader, Mesh& mesh, Camera& camera, float width, float height);
    const WorldItem* Target() const;
    const std::string& Message() const { return message; }
    void Reset();
private:
    std::vector<WorldItem> items;
    int target = -1;
    std::string message;
    float messageTime = 0;
};
