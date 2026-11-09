#pragma once
#include "Inventory.h"
#include <memory>
#include "../Graphics/Texture.h"
struct GLFWwindow;
class Player;
class CombatSystem;
class InventoryActions;
class LootSystem;
struct RaidStatus;
struct RaidSummary;
class Hideout;
class CharacterVisual;
class Shader;
class Camera;
class CollisionWorld;
struct Enemy;

class RaidHud
{
public:
    RaidHud();
    ~RaidHud();
    RaidHud(const RaidHud&) = delete;
    RaidHud& operator=(const RaidHud&) = delete;
    void Init(GLFWwindow* window);
    void Render(Player& player, LootSystem& loot, CombatSystem& combat, InventoryActions& actions, bool inventoryOpen, RaidStatus& raid,
        const CollisionWorld& world, Shader& shader, Enemy* corpse = nullptr, int corpseTarget = -1, StackId* takeStack = nullptr,
        bool* takeWeapon = nullptr, bool* closeLoot = nullptr);
    void ResetAmmoIndicator();
    static float LootPanelWidth(float screenWidth) { return screenWidth * .42f < 520.f ? screenWidth * .42f : 520.f; }
    void RenderHideout(Hideout& hideout, Player& character, CharacterVisual& visual, Shader& shader,
        bool mapPanel = false, bool* exitMenuOpen = nullptr);
    void RenderHideoutPauseMenu(bool& exitMenuOpen);
    void SetHideoutHealthTab(bool health) { hideoutHealthTab = health; }
    bool RenderExtractionSummary(const RaidSummary& summary, Player& character, CharacterVisual& visual, Shader& shader);
    void Shutdown();
private:
    Texture itemImages;
    std::unique_ptr<CharacterVisual> inventoryPreviewVisual;
    float inventoryPreviewRotation = 0.f;
    float enemyPreviewRotation = 0.f;
    bool contextReady = false;
    bool windowReady = false;
    bool rendererReady = false;
    bool hideoutHealthTab = false;
    char hideoutStashSearch[128]{};
    float hideoutPreviewRotation = 0.f;
    float reloadHoldSeconds = 0.f;
    float ammoDisplaySeconds = 0.f;
    bool reloadHoldTriggered = false;
    bool reloadingPreviousFrame = false;
};
