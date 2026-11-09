#pragma once
#include "Character.h"
#include "../Core/GunshotAudio.h"
#include "../Animation/CharacterVisual.h"
#include "../RaidSummary.h"
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
    bool returning = false;
    Inventory inventory;
    Inventory wornEquipment;
    bool rifle = true;
    float deadTime = 0;
    EnemyState state = EnemyState::Patrol;
    std::unique_ptr<CharacterVisual> visual;
    bool fired = false;
    float aimPitch = 0;
};
struct RescueSurvivor
{
    Character body;
    std::unique_ptr<CharacterVisual> visual;
    glm::vec3 target{0};
    float health = 100;
    float moveTimer = 0;
    bool available = false;
    bool recruited = false;
};
class CombatSystem
{
public:
    void Init(const CollisionWorld& world);
    bool SpawnRescueSurvivor(const glm::vec3& playerSpawn, const CollisionWorld& world);
    void Update(float dt, Player& player, Camera& camera, const CollisionWorld& world, LootSystem& loot, bool enabled);
    void Render(Shader& shader, Camera& camera, float width, float height);
    bool CanInteractWithRescue(const Player& player, const CollisionWorld& world) const;
    bool RecruitRescue() { if (!rescue.available || rescue.recruited || rescue.health <= 0) return false; rescue.recruited = true; rescue.target = rescue.body.position; return true; }
    bool HasRecruitedRescue() const { return rescue.recruited && rescue.health > 0; }
    const Character* RescueBody() const { return rescue.available && rescue.health > 0 ? &rescue.body : nullptr; }
    int Magazine() const { const auto* w = ActiveWeapon(); return w ? w->rounds : 0; }
    bool Equipped() const { return equipped; }
    bool HasWeapon() const { return carried && (carried->At(-1) || carried->At(-2) || carried->At(-3)); }
    bool HasRifle() const { return carried && (carried->At(-1) || carried->At(-2)); }
    void SetLoadout(Inventory& inventory) { carried = &inventory; activeSlot = inventory.At(-1) ? -1 : inventory.At(-2) ? -2 : -3; equipped = ActiveWeapon() != nullptr; }
    const ItemStack* ActiveWeapon() const { return carried ? carried->At(activeSlot) : nullptr; }
    bool ShowFireMode() const { return fireModeDisplayTime>0; }
    const char* FireModeName() const { const auto* w = ActiveWeapon(); return !w ? "비무장" : w->fireMode == FireMode::Safe ? "안전" : w->fireMode == FireMode::Auto ? "연발" : "단발"; }
    bool Aiming() const { return aiming; }
    bool Fired() const { return fired; }
    float ReloadTime() const { return reload; }
    const std::string& Message() const { return message; }
    int LivingEnemies() const;
    const std::vector<KillRecord>& KillRecords() const { return kills; }
    int FindCorpse(const Player& player, const CollisionWorld& world) const;
    const Enemy* Corpse(int index) const;
    Enemy* EditableCorpse(int index) { return Corpse(index) ? &enemies[static_cast<std::size_t>(index)] : nullptr; }
    bool TakeCorpseItem(int index, StackId id, Player& player);
    bool TakeCorpseRifle(int index, Player& player);
    bool LootSpaceClear(const glm::vec3& center, int ignoredCorpse) const;
    void CancelInput() { reload = 0; aiming = false; }
    void Reset() { carried = nullptr; gunshotAudio.Reset(); enemies.clear(); kills.clear(); rescue = RescueSurvivor{}; elapsedSeconds = 0; CancelInput(); }
private:
    GunshotAudio gunshotAudio;
    std::vector<Enemy> enemies;
    std::vector<KillRecord> kills;
    RescueSurvivor rescue;
    Inventory* carried = nullptr;
    int activeSlot = -1;
    StackId reloadWeapon = 0, reloadMagazine = 0;
    int magazine = 30;
    bool equipped = true, aiming = false;
    bool fired = false;
    float fireModeDisplayTime = 0;
    float cooldown = 0, reload = 0, messageTime = 0, elapsedSeconds = 0;
    std::string message;
};
