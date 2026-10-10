#pragma once
struct GLFWwindow;
class Player;
class CombatSystem;
class InventoryActions;
class LootSystem;
struct RaidStatus;
class Hideout;
class CharacterVisual;
class Shader;
class Camera;

class RaidHud
{
public:
    RaidHud() = default;
    ~RaidHud();
    RaidHud(const RaidHud&) = delete;
    RaidHud& operator=(const RaidHud&) = delete;
    void Init(GLFWwindow* window);
    void Render(const Player& player, const LootSystem& loot, const CombatSystem& combat, InventoryActions& actions, bool inventoryOpen, RaidStatus& raid);
    bool RenderHideout(Hideout& hideout, Player& character, CharacterVisual& visual, Shader& shader);
    void Shutdown();
private:
    bool contextReady = false;
    bool windowReady = false;
    bool rendererReady = false;
    float hideoutPreviewRotation = 0.f;
};
