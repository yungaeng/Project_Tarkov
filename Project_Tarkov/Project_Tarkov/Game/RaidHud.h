#pragma once
#include "Inventory.h"
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
struct Enemy;

class RaidHud
{
public:
    RaidHud() = default;
    ~RaidHud();
    RaidHud(const RaidHud&) = delete;
    RaidHud& operator=(const RaidHud&) = delete;
    void Init(GLFWwindow* window);
    void Render(const Player& player, const LootSystem& loot, const CombatSystem& combat, InventoryActions& actions, bool inventoryOpen, RaidStatus& raid,
        const Enemy* corpse = nullptr, int corpseTarget = -1, StackId* takeStack = nullptr,
        bool* takeWeapon = nullptr, bool* closeLoot = nullptr);
    void ResetAmmoIndicator();
    static float LootPanelWidth(float screenWidth) { return screenWidth * .42f < 520.f ? screenWidth * .42f : 520.f; }
    bool RenderHideout(Hideout& hideout, Player& character, CharacterVisual& visual, Shader& shader);
    bool RenderExtractionSummary(const RaidSummary& summary, Player& character, CharacterVisual& visual, Shader& shader);
    void Shutdown();
private:
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
