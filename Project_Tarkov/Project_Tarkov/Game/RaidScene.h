#pragma once
#include "../Scene/Scene.h"
#include "../Physics/CollisionWorld.h"
#include "Player.h"
#include "PlayerController.h"
#include "LootSystem.h"
#include "RaidHud.h"
#include "InventoryActions.h"
#include "CombatSystem.h"
#include "../Graphics/ThirdPersonCameraController.h"
#include "../Animation/CharacterVisual.h"
#include "../Graphics/Shader.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/Camera.h"
#include <memory>
#include "IndustrialGeometry.h"
#include "RaidStatus.h"
#include "Hideout.h"
#include "../RaidSummary.h"

class SceneManager;
class RaidScene : public Scene
{
public:
    explicit RaidScene(SceneManager& manager);
    RaidScene(SceneManager& manager, Hideout carriedHideout);
    void Init() override;
    void Update(float dt) override;
    void Render() override;
    void Shutdown() override;

private:
    // Declared before the player so the collision world outlives it.
    CollisionWorld collisionWorld;
    std::unique_ptr<Player> player;
    PlayerController playerController;
    ThirdPersonCameraController cameraController;
    CharacterVisual playerVisual;
    Shader shader;
    Mesh cubeMesh;
    Camera camera;
    // Free look changes only the rendered view; gameplay keeps using camera.
    Camera previewCamera;
    bool freeLook = false;
    bool firstPerson = false;
    LootSystem loot;
    RaidHud hud;
    InventoryActions actions;
    CombatSystem combat;
    unsigned focusGeneration = 0;
    std::vector<IndustrialZone::Batch> mapBatches;
    RaidStatus raid;
    Hideout hideout;
    bool deployRequested = false;
    SceneManager& sceneManager;
    bool hasProvidedHideout = false;
    Inventory startingInventory;
    std::uint32_t startingClothingMask = 0;
    bool startedWithRifle = false;
    RaidSummary summary;
    int lootingCorpse = -1, corpseTarget = -1;
    bool closeLootRequested = false;
    StackId takeCorpseStack = 0;
    bool takeCorpseWeapon = false;
    glm::vec3 beforeLootPosition{0};
    float beforeLootRotation = 0;
    Camera beforeLootCamera;
    void CloseCorpseLoot();
    void StartRaid();
    void FinishRaid(RaidPhase result);
    void FollowCamera(Camera& camera) const;
};
