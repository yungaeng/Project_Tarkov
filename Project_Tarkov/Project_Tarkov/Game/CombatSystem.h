#pragma once
#include "Character.h"
#include "../Animation/CharacterVisual.h"
#include <memory>
#include <vector>
#include <string>
class Player;
class Camera;
class LootSystem;
class Shader;
enum class EnemyState { Patrol, Chase, Attack, Search, Dead };
struct Enemy
{
    Character body;
    glm::vec3 home{0}, lastSeen{0};
    float health = 100, attackTimer = 1, searchTimer = 0;
    bool returning = false, lootDropped = false;
    EnemyState state = EnemyState::Patrol;
    std::unique_ptr<CharacterVisual> visual;
    bool fired = false;
    float aimPitch = 0;
};
class CombatSystem
{
public:
    void Init(const CollisionWorld& world);
    void Update(float dt, Player& player, Camera& camera, const CollisionWorld& world, LootSystem& loot, bool enabled);
    void Render(Shader& shader, Camera& camera, float width, float height);
    int Magazine() const { return magazine; }
    bool Equipped() const { return equipped; }
    bool HasRifle() const { return hasRifle; }
    void SetLoadout(bool rifle, int rounds) { hasRifle = equipped = rifle; magazine = rifle ? rounds : 0; }
    bool Aiming() const { return aiming; }
    bool Fired() const { return fired; }
    float ReloadTime() const { return reload; }
    const std::string& Message() const { return message; }
    int LivingEnemies() const;
    void CancelInput() { reload = 0; aiming = false; }
    void Reset() { enemies.clear(); CancelInput(); }
private:
    std::vector<Enemy> enemies;
    int magazine = 30;
    bool equipped = true, hasRifle = true, aiming = false;
    bool fired = false;
    float cooldown = 0, reload = 0, messageTime = 0;
    std::string message;
};
