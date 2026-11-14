#pragma once
#include "../Inventory/Inventory.h"
#include "../Trading/TradingTypes.h"
#include <array>
#include <memory>
#include "../../Graphics/Texture.h"
struct GLFWwindow;
class Player;
class Character;
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
namespace HudWidgets { struct CharacterPreview; }

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
        bool* takeWeapon = nullptr, bool* closeLoot = nullptr, bool* rescuePromptOpen = nullptr,
        bool* rescueRequested = nullptr);
    void ResetAmmoIndicator();
    static float LootPanelWidth(float screenWidth) { return screenWidth * .42f < 520.f ? screenWidth * .42f : 520.f; }
    void RenderHideout(Hideout& hideout, Player& character, CharacterVisual& visual, Shader& shader,
        bool mapPanel = false, bool* exitMenuOpen = nullptr, int storageRoom = -1, int storageFurniture = -1,
        const CollisionWorld* world = nullptr);
    void RenderHideoutStorage(Hideout& hideout, int room, int furniture, Player& character,
        CharacterVisual& visual, Shader& shader, bool& panelOpen, bool& exitMenuOpen, const CollisionWorld& world);
    void RenderHideoutPauseMenu(bool& exitMenuOpen);
    void RenderTrading(Hideout& hideout, Trading::TradeSession& session, bool& panelOpen);
    void RenderScavDialogue(Hideout& hideout, int profileIndex, bool& panelOpen);
    void SetHideoutHealthTab(bool health) { hideoutHealthTab = health; }
    bool RenderExtractionSummary(const RaidSummary& summary, Player& character, CharacterVisual& visual, Shader& shader,
        Character* rescuedCharacter = nullptr, CharacterVisual* rescuedVisual = nullptr);
    void Shutdown();
private:
    void RenderInventoryPanel(Player&, LootSystem&, InventoryActions&, const CollisionWorld&, Enemy*,
        HudWidgets::CharacterPreview&, HudWidgets::CharacterPreview&, bool* takeWeapon, bool* closeLoot);
    Texture itemImages;
    std::array<Texture, Trading::TraderCount> traderPortraits;
    std::unique_ptr<CharacterVisual> inventoryPreviewVisual;
    float inventoryPreviewRotation = 0.f;
    float enemyPreviewRotation = 0.f;
    bool contextReady = false;
    bool windowReady = false;
    bool rendererReady = false;
    bool hideoutHealthTab = false;
    char hideoutStashSearch[128]{};
    int hideoutStashCategory = 0;
    float hideoutPreviewRotation = 0.f;
    float reloadHoldSeconds = 0.f;
    float ammoDisplaySeconds = 0.f;
    bool reloadHoldTriggered = false;
    bool reloadingPreviousFrame = false;
};
